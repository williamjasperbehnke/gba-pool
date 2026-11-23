#include "pool_view.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifdef __GBA__
#include "bn_common.h"
#define POOL_RASTER_CODE BN_CODE_IWRAM
#else
#define POOL_RASTER_CODE
#endif

namespace pool
{
namespace
{
constexpr int norm = direction_unit;
constexpr int aim_distance = 55 * unit;

constexpr int square_root(int n, int high = norm + 1)
{
    int low = 0;
    while(high - low > 1)
    {
        int middle = (low + high) / 2;
        if(middle * middle <= n)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

constexpr auto make_normals()
{
    struct Map { uint16_t texels[33][33]{}; };
    Map map;
    for(int y = -16; y <= 16; ++y)
    {
        for(int x = -16; x <= 16; ++x)
        {
            int front_squared = (256 - x * x - y * y) * (norm * norm / 256);
            int front = front_squared >= 0 ? square_root(front_squared) : 0;
            int light = (front * 7 - y * 336 - x * 192) / 16;
            int shade = 8 + (light > -45) + (light > 135) + (light > 300) + (light > 465);
            map.texels[y + 16][x + 16] = uint16_t(front | (shade << 11));
        }
    }
    return map;
}

constexpr auto normals = make_normals();

constexpr auto make_stretch()
{
    struct Lookup { int16_t values[193]{}; };
    Lookup lookup;
    for(int i = 0; i <= 192; ++i)
    {
        lookup.values[i] = int16_t(square_root((norm + i * 16) * norm, 2 * norm + 1));
    }
    return lookup;
}

constexpr auto stretch = make_stretch();

POOL_RASTER_CODE SphereProjection sphere_projection(CameraPoint point, int focal, Vec origin)
{
    // Reject spheres outside the view using their camera-space bounding boxes.
    int origin_y = origin.y / unit;
    int depth = point.depth;
    if(depth < near_depth || std::abs(point.x) * focal > 120 * depth + (120 + focal) * ball_height ||
       point.y * focal > (view_bottom - origin_y) * depth + (view_bottom - origin_y + focal) * ball_height ||
       -point.y * focal > (origin_y - view_top) * depth + (origin_y - view_top + focal) * ball_height)
    {
        return {};
    }

    // The tangent cone intersects the screen in an ellipse. Factor it into
    // horizontal shear and two scales so each scanline can stay in native pixels.
    int inverse_depth = (1 << 26) / (depth >> 4);
    int slope_x = ((point.x >> 4) * inverse_depth) >> 14;
    int slope_y = ((point.y >> 4) * inverse_depth) >> 14;
    int relative_radius = (ball_height >> 4) * inverse_depth >> 10;
    int center_shift = (relative_radius * relative_radius) >> 20;
    int qx = (slope_x + ((slope_x * center_shift) >> 13)) >> 2;
    int qy = (slope_y + ((slope_y * center_shift) >> 13)) >> 2;
    int vertical = norm + (qy * qy >> 10);
    int horizontal = norm + qx * qx / vertical;
    int skew = qx * qy / vertical;
    int screen_radius = focal * relative_radius >> 8;
    screen_radius += screen_radius * center_shift >> 13;
    Vec center = origin + Vec{ (((slope_x * center_shift) >> 12) + slope_x) * focal / 16,
                               (((slope_y * center_shift) >> 12) + slope_y) * focal / 16 };
    // Overlapping rays hit the nearer equal-radius sphere first by radial distance.
    int camera_x = point.x >> 4;
    int camera_y = point.y >> 4;
    int camera_z = depth >> 4;
    int distance_squared = camera_x * camera_x + camera_y * camera_y + camera_z * camera_z;
    return { center, distance_squared,
             screen_radius * stretch.values[std::min(192, (horizontal - norm + 8) / 16)] / norm,
             screen_radius * stretch.values[std::min(192, (vertical - norm + 8) / 16)] / norm, skew };
}

}

bool SphereProjection::visible() const
{
    int extent_x = radius_x + radius_y * std::abs(skew) / norm;
    return radius_x && center.x + extent_x > 0 && center.x - extent_x < 240 * unit &&
           center.y + radius_y > view_top * unit && center.y - radius_y < view_bottom * unit;
}

bool SphereProjection::covers(Vec pixel_center) const
{
    int dy = pixel_center.y - center.y;
    if(radius_x <= 0 || std::abs(dy) >= radius_y)
    {
        return false;
    }
    int dx = pixel_center.x - center.x - dy * skew / direction_unit;
    if(std::abs(dx) >= radius_x)
    {
        return false;
    }
    int nx = dx * direction_unit / radius_x;
    int ny = dy * direction_unit / radius_y;
    return nx * nx + ny * ny <= direction_unit * direction_unit;
}

POOL_RASTER_CODE bool shadow_fits_on_cloth(Vec point)
{
    if(point.y > top + 8 * unit && point.y < bottom - 8 * unit)
    {
        return true;
    }
    // Reserve the leather mouth and a margin for the contact shadow on either side.
    for(int x : { 17, 120, 223 })
    {
        int width = x == 120 ? 14 : 11;
        if(std::abs(point.x - x * unit) < width * unit)
        {
            return false;
        }
    }
    return true;
}

SphereProjection project_sphere(Vec point)
{
    int vertical = (overview_pitch.sine * (point.y - 84 * unit) - overview_pitch.cosine * ball_height) / 100;
    return sphere_projection({ point.x - 120 * unit, vertical, camera_depth(point, ball_height) }, overview_focal, px(120, overview_center_y));
}

AimCamera::AimCamera(Vec cue, int angle) : forward(direction(angle))
{
    eye = cue - travel(forward, aim_distance);
}

POOL_RASTER_CODE CameraPoint AimCamera::local(Vec point, int height) const
{
    Vec delta = point - eye;
    int along = (delta.x * forward.x + delta.y * forward.y) / direction_unit;
    int across = (delta.y * forward.x - delta.x * forward.y) / direction_unit;
    int down = elevation - height;
    return { across, (tilt.cosine * down - tilt.sine * along) / 100,
             (tilt.cosine * along + tilt.sine * down) / 100 };
}

CameraPoint AimCamera::clip_near(CameraPoint outside, CameraPoint inside)
{
    int ratio = (near_depth - outside.depth) * 4096 / (inside.depth - outside.depth);
    return { outside.x + (inside.x - outside.x) * ratio / 4096,
             outside.y + (inside.y - outside.y) * ratio / 4096, near_depth };
}

POOL_RASTER_CODE Vec AimCamera::screen(CameraPoint point) const
{
    // Use one perspective divide for both axes on the GBA.
    int scale = (focal << 23) / point.depth;
    return { 120 * unit + int((int64_t(point.x) * scale) >> 15),
             screen_center_y * unit + int((int64_t(point.y) * scale) >> 15) };
}

SphereProjection AimCamera::sphere(Vec point) const
{
    return sphere_projection(local(point, ball_height), focal, px(120, screen_center_y));
}

int AimCamera::cue_tip_row(int setback) const
{
    // The cue lies on the aiming camera's forward axis at every yaw.
    int along = aim_distance - setback;
    int down = elevation - ball_height;
    CameraPoint tip{ 0, (tilt.cosine * down - tilt.sine * along) / 100,
                        (tilt.cosine * along + tilt.sine * down) / 100 };
    return (screen(tip).y + unit / 2) >> 8;
}

bool AimCamera::segment(Vec& start, Vec& end, int height) const
{
    CameraPoint a = local(start, height);
    CameraPoint b = local(end, height);
    if(a.depth < near_depth && b.depth < near_depth)
    {
        return false;
    }
    if(a.depth < near_depth)
    {
        a = clip_near(a, b);
    }
    else if(b.depth < near_depth)
    {
        b = clip_near(b, a);
    }

    start = screen(a);
    end = screen(b);
    return clip_view_segment(start, end);
}

bool clip_view_segment(Vec& start, Vec& end)
{
    auto clip = [](Vec& from, const Vec& to, int boundary, bool vertical) {
        int along = vertical ? from.y : from.x;
        int other = vertical ? to.y : to.x;
        int offset = vertical ? from.x : from.y;
        int other_offset = vertical ? to.x : to.y;
        offset += int(int64_t(other_offset - offset) * (boundary - along) / (other - along));
        from = vertical ? Vec{ offset, boundary } : Vec{ boundary, offset };
    };

    constexpr int edges[] = { 0, 239 * unit, view_top * unit, view_bottom * unit };
    for(int edge = 0; edge < 4; ++edge)
    {
        bool vertical = edge >= 2;
        bool min_side = !(edge & 1);
        int boundary = edges[edge];
        int first = vertical ? start.y : start.x;
        int second = vertical ? end.y : end.x;
        bool first_out = min_side ? first < boundary : first > boundary;
        bool second_out = min_side ? second < boundary : second > boundary;
        if(first_out && second_out)
        {
            return false;
        }
        if(first_out)
        {
            clip(start, end, boundary, vertical);
        }
        else if(second_out)
        {
            clip(end, start, boundary, vertical);
        }
    }
    return true;
}

void RollingBall::roll(Vec displacement)
{
    int distance = length(displacement);
    int angle = (distance * 42 + unit / 2) / unit;
    if(! angle)
    {
        return;
    }
    Vec heading = normalized(displacement);
    int ax = -heading.y;
    int ay = heading.x;
    Vec rotation = direction(angle);
    int along = (ax * x + ay * y) / norm;
    int along_factor = along * (norm - rotation.x) / norm;
    int nx = (x * rotation.x + (ay * z / norm) * rotation.y + ax * along_factor) / norm;
    int ny = (y * rotation.x - (ax * z / norm) * rotation.y + ay * along_factor) / norm;
    int nz = (z * rotation.x + ((ax * y - ay * x) / norm) * rotation.y) / norm;
    int magnitude = square_root(nx * nx + ny * ny + nz * nz);
    if(magnitude)
    {
        x = nx * norm / magnitude;
        y = ny * norm / magnitude;
        z = nz * norm / magnitude;
    }
}

POOL_RASTER_CODE uint16_t RollingBall::draw(int number, const SphereProjection& projection, Vec screen_right,
                       CameraPitch pitch, uint32_t* tiles) const
{
    uint16_t touched = 0;
    if(! projection.visible())
    {
        return touched;
    }

    Vec top_left = projection.pixel_center() - Vec{ sphere_sprite_size / 2, sphere_sprite_size / 2 };
    int lateral = (x * screen_right.x + y * screen_right.y) / norm;
    int forward = (y * screen_right.x - x * screen_right.y) / norm;
    int texture_y = (pitch.sine * forward - pitch.cosine * z) / 100;
    int texture_front = (pitch.cosine * forward + pitch.sine * z) / 100;
    int first_y = std::max({ view_top, top_left.y, (projection.center.y - projection.radius_y) >> 8 });
    int last_y = std::min({ view_bottom, top_left.y + sphere_sprite_size,
                            ((projection.center.y + projection.radius_y) >> 8) + 1 });
    int inverse_x = (1 << 21) / projection.radius_x;
    int inverse_y = (1 << 21) / projection.radius_y;
    int pixel_step = inverse_x / 2;
    for(int sy = first_y; sy < last_y; ++sy)
    {
        int dy = sy * unit + unit / 2 - projection.center.y;
        if(std::abs(dy) >= projection.radius_y)
        {
            continue;
        }
        constexpr int normal_limit = 16 * unit;
        int y_normal = (dy * inverse_y) >> 9;
        int ny = y_normal / unit;
        int row_limit = normal_limit * normal_limit - y_normal * y_normal;
        int row = sy - top_left.y;
        int row_center = projection.center.x + dy * projection.skew / norm;
        int extent = projection.radius_x * (normals.texels[ny + 16][16] & 2047) / norm;
        int first_x = std::max({ 0, top_left.x, (row_center - extent + unit / 2) >> 8 });
        int last_x = std::min({ 240, top_left.x + sphere_sprite_size, (row_center + extent + unit / 2) >> 8 });
        int x_normal = ((first_x * unit + unit / 2 - row_center) * inverse_x) >> 9;
        int sx = first_x;
        while(sx < last_x)
        {
            int column = sx - top_left.x;
            int tile = (row >> 3) * (sphere_sprite_size / 8) + (column >> 3);
            int shift = (column & 7) * 4;
            int end = std::min(sx + 8 - (column & 7), last_x);
            uint32_t word = 0;
            for(; sx < end; ++sx, shift += 4, x_normal += pixel_step)
            {
                if(x_normal * x_normal > row_limit)
                {
                    continue;
                }
                int nx = x_normal / unit;
                uint16_t texel = normals.texels[ny + 16][nx + 16];
                int shade = texel >> 11;
                if(number >= 9)
                {
                    int front = texel & 2047;
                    int patch = (lateral * nx + texture_y * ny) / 16 + texture_front * front / norm;
                    if(std::abs(patch) > 505)
                    {
                        constexpr int ivory[] = { 13, 5, 6, 7, 14 };
                        shade = ivory[shade - 8];
                    }
                }
                word |= uint32_t(shade) << shift;
            }

            if(word)
            {
                unsigned tile_bit = 1u << tile;
                if(! (touched & tile_bit))
                {
                    std::memset(tiles + tile * 8, 0, 8 * sizeof(uint32_t));
                    touched = uint16_t(touched | tile_bit);
                }
                tiles[tile * 8 + (row & 7)] = word;
            }
        }
    }
    return touched;
}

}
