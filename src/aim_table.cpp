#include "aim_table.h"

#include <algorithm>
#include <cstdlib>

#include "bn_memory.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_tile.h"

namespace pool
{
namespace
{
constexpr int columns = 30;
// The close camera leaves the top two tile rows above the table's horizon.
constexpr int canvas_top = 16;
constexpr int height = view_bottom - canvas_top;
constexpr int rows = height / 8;
constexpr int first_row = canvas_top / 8;
constexpr int end_left = 6;
constexpr int end_right = 234;
constexpr int rail_top = 5;
constexpr int apron_bottom = -24;
constexpr int side_lip_x[] = { 113, 115, 118, 122, 125, 127 };
constexpr int side_lip_y[] = { 3, 2, 1, 1, 2, 3 };

BN_DATA_EWRAM_BSS bn::tile pixels[1 + columns * rows];
BN_DATA_EWRAM_BSS bn::regular_bg_map_cell cells[32 * 32];
constexpr bn::color colors[16] = {
    { 0, 0, 0 }, { 9, 5, 5 }, { 15, 8, 6 }, { 22, 13, 8 },
    { 28, 19, 12 }, { 13, 7, 5 }, { 7, 16, 12 }, { 9, 20, 14 },
    { 10, 8, 8 }, { 2, 3, 5 }, { 14, 9, 7 }, { 20, 11, 7 },
    { 30, 24, 15 }, { 19, 11, 7 }, { 7, 17, 13 }, { 6, 4, 5 }
};

struct Vertex
{
    int x;
    int y;
    int z;
};

BN_CODE_IWRAM void span(int start, int end, int y, uint32_t ink)
{
    start = std::max(0, start);
    end = std::min(240, end);
    if(start >= end)
    {
        return;
    }
    bn::tile* tile = pixels + 1 + ((y >> 3) - first_row) * columns + (start >> 3);
    int row = y & 7;
    int left = start & 7;
    int count = end - start;
    if(left)
    {
        int write = std::min(8 - left, count);
        uint32_t mask = (0xffffffffu >> ((8 - write) * 4)) << (left * 4);
        tile->data[row] = (tile->data[row] & ~mask) | (ink & mask);
        count -= write;
        ++tile;
    }
    while(count >= 32)
    {
        tile[0].data[row] = ink;
        tile[1].data[row] = ink;
        tile[2].data[row] = ink;
        tile[3].data[row] = ink;
        tile += 4;
        count -= 32;
    }
    while(count >= 8)
    {
        tile->data[row] = ink;
        ++tile;
        count -= 8;
    }
    if(count)
    {
        uint32_t mask = 0xffffffffu >> ((8 - count) * 4);
        tile->data[row] = (tile->data[row] & ~mask) | (ink & mask);
    }
}

inline void pixel(int x, int y, uint32_t color)
{
    uint32_t& word = pixels[1 + ((y >> 3) - first_row) * columns + (x >> 3)].data[y & 7];
    int shift = (x & 7) * 4;
    word = (word & ~(15u << shift)) | (color << shift);
}

struct ScanEdges
{
    int top;
    int bottom;
    int first[height];
    int second[height];

    int left(int y) const { return std::min(first[y - canvas_top], second[y - canvas_top]); }
    int right(int y) const { return std::max(first[y - canvas_top], second[y - canvas_top]) + 1; }
};

BN_CODE_IWRAM bool scan_edges(const Vec* points, int count, ScanEdges& edges)
{
    int min_y = view_bottom;
    int max_y = canvas_top;
    int min_x = 240;
    int max_x = -1;
    for(int i = 0; i < count; ++i)
    {
        min_y = std::min(min_y, points[i].y);
        max_y = std::max(max_y, points[i].y);
        min_x = std::min(min_x, points[i].x);
        max_x = std::max(max_x, points[i].x);
    }
    edges.top = std::max(canvas_top, min_y);
    edges.bottom = std::min(view_bottom, max_y);
    if(edges.top >= edges.bottom || min_x >= 240 || max_x < 0)
    {
        return false;
    }

    // A convex face contributes one rising and one falling edge on each scanline.
    Vec previous = points[count - 1];
    for(int i = 0; i < count; ++i)
    {
        Vec a = previous;
        Vec b = points[i];
        previous = b;
        int* edge = edges.first;
        if(b.y < a.y)
        {
            std::swap(a, b);
            edge = edges.second;
        }
        int begin = std::max(a.y, edges.top);
        int end = std::min(b.y, edges.bottom);
        if(begin >= end)
        {
            continue;
        }
        int step = (b.x - a.x) * unit / (b.y - a.y);
        int x = a.x * unit + step * (begin - a.y);
        for(int y = begin; y < end; ++y)
        {
            edge[y - canvas_top] = (x + unit / 2) >> 8;
            x += step;
        }
    }
    return true;
}

BN_CODE_IWRAM void raster(const Vec* points, int count, int color)
{
    ScanEdges edges;
    if(! scan_edges(points, count, edges))
    {
        return;
    }
    uint32_t fill = uint32_t(color) * 0x11111111u;
    uint32_t top = fill;
    if(color == 3)
    {
        top = 0x44444444;
    }
    else if(color == 1 || color == 2)
    {
        top = 0xbbbbbbbb;
    }
    span(edges.left(edges.top), edges.right(edges.top), edges.top, top);
    for(int y = edges.top + 1; y < edges.bottom; ++y)
    {
        span(edges.left(y), edges.right(y), y, fill);
    }
}

template<int Count>
int project_face(const AimCamera& camera, const Vertex (&vertices)[Count], Vec (&projected)[Count + 1])
{
    CameraPoint camera_vertices[Count];
    unsigned outside = 3;
    for(int index = 0; index < Count; ++index)
    {
        const Vertex& vertex = vertices[index];
        camera_vertices[index] = camera.local(px(vertex.x, vertex.y), vertex.z * unit);
        if(outside)
        {
            outside &= camera.outside_sides(camera_vertices[index]);
        }
    }
    if(outside)
    {
        return 0;
    }

    int count = 0;
    auto append = [&](CameraPoint point) {
        Vec screen = camera.screen(point);
        projected[count++] = { (screen.x + unit / 2) >> 8, (screen.y + unit / 2) >> 8 };
    };
    CameraPoint previous = camera_vertices[Count - 1];
    for(const CameraPoint current : camera_vertices)
    {
        bool previous_visible = previous.depth >= near_depth;
        bool current_visible = current.depth >= near_depth;
        if(previous_visible != current_visible)
        {
            append(previous_visible ? AimCamera::clip_near(current, previous) :
                                      AimCamera::clip_near(previous, current));
        }
        if(current_visible)
        {
            append(current);
        }
        previous = current;
    }
    return count;
}

template<int Count>
void face(const AimCamera& camera, const Vertex (&vertices)[Count], int color)
{
    Vec projected[Count + 1];
    int count = project_face(camera, vertices, projected);
    if(count >= 3)
    {
        raster(projected, count, color);
    }
}

void quad(const AimCamera& camera, int x0, int y0, int x1, int y1, int z, int color)
{
    face(camera, {{ x0, y0, z }, { x1, y0, z }, { x1, y1, z }, { x0, y1, z }}, color);
}

template<int Mouth, int Floor>
void pocket_well(const AimCamera& camera, const Vertex (&mouth)[Mouth], const Vertex (&floor)[Floor])
{
    Vec projected_mouth[Mouth + 1];
    ScanEdges opening;
    int count = project_face(camera, mouth, projected_mouth);
    if(count < 3 || ! scan_edges(projected_mouth, count, opening))
    {
        return;
    }

    Vec projected_floor[Floor + 1];
    ScanEdges well;
    count = project_face(camera, floor, projected_floor);
    bool visible = count >= 3 && scan_edges(projected_floor, count, well);
    for(int y = opening.top; y < opening.bottom; ++y)
    {
        int left = opening.left(y);
        int right = opening.right(y);
        // The felt edge masks the lower well as the camera looks across the mouth.
        span(left, right, y, 0xffffffff);
        if(visible && y >= well.top && y < well.bottom)
        {
            span(std::max(left, well.left(y)), std::min(right, well.right(y)), y, 0x99999999);
        }
    }
}

void apron(const AimCamera& camera, int x0, int y0, int x1, int y1, bool exterior, int color)
{
    auto band = [&](int top, int bottom, int material) {
        face(camera, {{ x0, y0, top }, { x1, y1, top }, { x1, y1, bottom }, { x0, y0, bottom }}, material);
    };
    if(exterior)
    {
        // Darker wood below the raised rail gives the apron a recessed lower edge.
        band(-17, apron_bottom, 15);
        band(-8, -17, 5);
        band(1, -8, color);
        band(rail_top, 1, 13);
    }
    else
    {
        band(rail_top, 0, color);
    }
}

constexpr auto felt_marks()
{
    struct Marks
    {
        Vec points[60];
    };
    Marks marks{};
    for(int row = 0; row < 6; ++row)
    {
        for(int column = 0; column < 10; ++column)
        {
            // Jitter a world-space grid to avoid a regular pattern on the cloth.
            int x = 20 + column * 20 + (column * 7 + row * 13) % 13;
            int y = 46 + row * 13 + (column * 11 + row * 5) % 9;
            marks.points[row * 10 + column] = px(x, y);
        }
    }
    return marks;
}

constexpr auto cloth_marks = felt_marks();

BN_CODE_IWRAM void felt_texture(const AimCamera& camera)
{
    for(int index = 0; index < 60; ++index)
    {
        CameraPoint point = camera.local(cloth_marks.points[index]);
        if(point.depth < near_depth || camera.outside_sides(point))
        {
            continue;
        }
        Vec screen = camera.screen(point);
        int x = (screen.x + unit / 2) >> 8;
        int y = (screen.y + unit / 2) >> 8;
        if(unsigned(x) < 240u && y >= canvas_top && y < view_bottom)
        {
            pixel(x, y, (index & 1) ? 6 : 7);
        }
    }
}

BN_CODE_IWRAM void wood_stroke(const AimCamera& camera, Vec start, Vec end, unsigned color)
{
    CameraPoint from = camera.local(start, rail_top * unit);
    CameraPoint to = camera.local(end, rail_top * unit);
    if(from.depth < near_depth || to.depth < near_depth ||
       (camera.outside_sides(from) & camera.outside_sides(to)))
    {
        return;
    }

    start = camera.screen(from);
    end = camera.screen(to);
    int x = (start.x + unit / 2) >> 8;
    int y = (start.y + unit / 2) >> 8;
    int target_x = (end.x + unit / 2) >> 8;
    int target_y = (end.y + unit / 2) >> 8;
    if(unsigned(x) >= 240u || unsigned(target_x) >= 240u ||
       y < canvas_top || y >= view_bottom || target_y < canvas_top || target_y >= view_bottom)
    {
        return;
    }
    int width = std::abs(target_x - x);
    int rise = std::abs(target_y - y);
    int step_x = x < target_x ? 1 : -1;
    int step_y = y < target_y ? 1 : -1;
    int error = width - rise;
    for(;;)
    {
        pixel(x, y, color);
        if(x == target_x && y == target_y)
        {
            break;
        }
        int twice = error * 2;
        if(twice > -rise)
        {
            error -= rise;
            x += step_x;
        }
        if(twice < width)
        {
            error += width;
            y += step_y;
        }
    }
}

void rail_sight(const AimCamera& camera, int x, int y)
{
    CameraPoint point = camera.local(px(x, y), rail_top * unit);
    if(point.depth < near_depth || camera.outside_sides(point))
    {
        return;
    }
    Vec screen = camera.screen(point);
    int column = (screen.x + unit / 2) >> 8;
    int row = (screen.y + unit / 2) >> 8;
    if(column < 0 || column >= 240 || row < canvas_top || row >= view_bottom)
    {
        return;
    }
    constexpr uint32_t ivory = 0xcccccccc;
    int width = point.depth < 75 * unit ? 2 : 1;
    span(column - width, column + width + 1, row, ivory);
    if(width == 2 && row > canvas_top && row + 1 < view_bottom)
    {
        span(column - 1, column + 2, row - 1, ivory);
        span(column - 1, column + 2, row + 1, ivory);
    }
}

void long_rail(const AimCamera& camera, int edge, int inward)
{
    int back = edge + inward * 3;
    int inner = edge + inward * 8;
    int nose = edge + inward * 11;
    quad(camera, end_left, std::min(edge, back), side_lip_x[0], std::max(edge, back), rail_top, 3);
    quad(camera, side_lip_x[5], std::min(edge, back), end_right, std::max(edge, back), rail_top, 3);
    for(int index = 0; index < 5; ++index)
    {
        int x0 = side_lip_x[index];
        int x1 = side_lip_x[index + 1];
        int y0 = edge + inward * side_lip_y[index];
        int y1 = edge + inward * side_lip_y[index + 1];
        face(camera, {{ x0, edge, rail_top }, { x1, edge, rail_top },
                       { x1, y1, rail_top }, { x0, y0, rail_top }}, 3);
    }

    face(camera, {{ 20, back, rail_top }, { 114, back, rail_top },
                   { 112, inner, rail_top }, { 24, inner, rail_top }}, 3);
    face(camera, {{ 126, back, rail_top }, { 220, back, rail_top },
                   { 216, inner, rail_top }, { 128, inner, rail_top }}, 3);
    int cushion = inward > 0 ? 7 : 6;
    face(camera, {{ 24, inner, rail_top }, { 112, inner, rail_top }, { 108, nose, 0 }, { 27, nose, 0 }},
         cushion);
    face(camera, {{ 128, inner, rail_top }, { 216, inner, rail_top }, { 213, nose, 0 }, { 132, nose, 0 }},
         cushion);

    for(int first : { 26, 130 })
    {
        for(int x = first + 15; x < first + 74; x += 26)
        {
            if(x != first + 41)
            {
                int grain_y = back + inward * (1 + (x & 1));
                wood_stroke(camera, px(x - 11, grain_y), px(x + 4, grain_y), (x & 2) ? 2 : 4);
            }
            rail_sight(camera, x, back + inward * 2);
        }
    }
}

void short_rail(const AimCamera& camera, int edge, int inward)
{
    int back = edge + inward * 5;
    int inner = edge + inward * 8;
    int nose = edge + inward * 11;
    quad(camera, std::min(edge, back), 32, std::max(edge, back), 136, rail_top, 3);
    face(camera, {{ back, 47, rail_top }, { inner, 49, rail_top },
                   { inner, 119, rail_top }, { back, 121, rail_top }}, 3);
    face(camera, {{ inner, 49, rail_top }, { nose, 52, 0 }, { nose, 116, 0 }, { inner, 119, rail_top }}, 6);

    for(int y : { 62, 84, 106 })
    {
        if(y != 84)
        {
            int grain_x = edge + inward * (2 + y % 3);
            wood_stroke(camera, px(grain_x, y - 7), px(grain_x, y + 4), y == 106 ? 4 : 2);
        }
        rail_sight(camera, back, y);
    }
}

void side_pocket(const AimCamera& camera, int edge, int inward)
{
    int back = edge + inward * 3;
    int heel = edge + inward * 6;
    int inner = edge + inward * 8;
    int nose = edge + inward * 11;
    int mouth = edge + inward * 14;
    int curve = mouth - inward;
    pocket_well(camera, {{ 113, back, 0 }, { 127, back, 0 }, { 132, heel, 0 }, { 132, nose, 0 },
                         { 129, curve, 0 }, { 125, mouth, 0 }, { 115, mouth, 0 }, { 111, curve, 0 },
                         { 108, nose, 0 }, { 108, heel, 0 }},
                        {{ 114, back, -4 }, { 126, back, -4 }, { 131, heel, -4 }, { 131, nose, -4 },
                         { 127, curve, -4 }, { 113, curve, -4 }, { 109, nose, -4 }, { 109, heel, -4 }});

    // Follow the curved cutout in the wood down into the leather backing.
    for(int index = 0; index < 5; ++index)
    {
        int x0 = side_lip_x[index];
        int x1 = side_lip_x[index + 1];
        int y0 = edge + inward * side_lip_y[index];
        int y1 = edge + inward * side_lip_y[index + 1];
        face(camera, {{ x0, y0, rail_top }, { x1, y1, rail_top },
                       { x1, y1 + inward * 4, 0 }, { x0, y0 + inward * 4, 0 }},
             index < 2 ? 10 : 8);
    }

    // Dark cushion facings are visible between the wood and the opening.
    face(camera, {{ 114, back, rail_top }, { 112, inner, rail_top }, { 108, nose, 0 }, { 113, heel, 0 }}, 6);
    face(camera, {{ 126, back, rail_top }, { 128, inner, rail_top }, { 132, nose, 0 }, { 127, heel, 0 }}, 6);
}

void corner_pocket(const AimCamera& camera, int x, int y)
{
    int dx = x == end_left ? 1 : -1;
    int dy = y == 32 ? 1 : -1;
    auto point = [=](int u, int v, int z) -> Vertex { return { x + dx * u, y + dy * v, z }; };
    pocket_well(camera, { point(5, 3, 0), point(14, 3, 0), point(21, 11, 0), point(20, 15, 0),
                          point(17, 18, 0), point(13, 20, 0), point(9, 20, 0), point(5, 17, 0) },
                         { point(6, 4, -4), point(14, 4, -4), point(20, 11, -4), point(19, 14, -4),
                           point(16, 17, -4), point(12, 19, -4), point(9, 19, -4), point(6, 16, -4) });

    // The rounded collar joins the two wood rails around the back of the drop.
    constexpr int lip[][2] = { { 14, 3 }, { 10, 3 }, { 7, 4 }, { 5, 7 }, { 5, 11 }, { 5, 15 } };
    constexpr int base[][2] = { { 14, 6 }, { 11, 6 }, { 9, 7 }, { 8, 9 }, { 8, 12 }, { 7, 15 } };
    for(int index = 0; index < 5; ++index)
    {
        Vertex a = point(lip[index][0], lip[index][1], rail_top);
        Vertex b = point(lip[index + 1][0], lip[index + 1][1], rail_top);
        Vertex c = point(base[index + 1][0], base[index + 1][1], 0);
        Vertex d = point(base[index][0], base[index][1], 0);
        face(camera, { a, b, c, d }, index < 2 ? 10 : 8);
        face(camera, { point(5, 3, rail_top), a, b }, 10);
    }

    face(camera, { point(14, 3, rail_top), point(18, 8, rail_top), point(21, 11, 0), point(14, 6, 0) }, 6);
    face(camera, { point(5, 15, rail_top), point(8, 17, rail_top), point(11, 20, 0), point(7, 15, 0) }, 6);
}

}

void AimTable::show()
{
    if(background)
    {
        return;
    }
    for(int row = 0; row < rows; ++row)
    {
        for(int column = 0; column < columns; ++column)
        {
            cells[(first_row + row) * 32 + column] = bn::regular_bg_map_cell(1 + row * columns + column);
        }
    }
    bn::memory::clear(pixels);
    bn::regular_bg_item item(pixels, colors, bn::bpp_mode::BPP_4, cells[0], bn::size(32, 32));
    background = item.create_bg(8, 48);
    background->set_priority(2);
    background->set_visible(false);
}

void AimTable::close()
{
    background.reset();
}

void AimTable::hide()
{
    if(background)
    {
        background->set_visible(false);
    }
}

void AimTable::commit()
{
    bn::regular_bg_tiles_ptr tiles = background->tiles();
    tiles.reload_tiles_ref();
    background->set_visible(true);
}

void AimTable::draw(const AimCamera& camera)
{
    bn::memory::clear(pixels);
    quad(camera, end_left, 32, end_right, 136, 0, 14);
    felt_texture(camera);
    side_pocket(camera, 32, 1);
    side_pocket(camera, 136, -1);
    corner_pocket(camera, end_left, 32);
    corner_pocket(camera, end_right, 32);
    corner_pocket(camera, end_left, 136);
    corner_pocket(camera, end_right, 136);

    Vec eye = camera.position();
    // On the far sides, the short upper walls fill the seam below the raised rails.
    for(int x : { end_left, end_right })
    {
        bool exterior = x == end_left ? eye.x < x * unit : eye.x > x * unit;
        apron(camera, x, 32, x, 136, exterior, 1);
    }
    for(int y : { 32, 136 })
    {
        bool exterior = y == 32 ? eye.y < y * unit : eye.y > y * unit;
        apron(camera, end_left, y, end_right, y, exterior, 2);
    }

}

void AimTable::finish(const AimCamera& camera)
{
    long_rail(camera, 32, 1);
    long_rail(camera, 136, -1);
    short_rail(camera, end_left, 1);
    short_rail(camera, end_right, -1);
}
}
