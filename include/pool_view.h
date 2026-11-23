#ifndef POOL_VIEW_H
#define POOL_VIEW_H

#include "pool.h"

namespace pool
{
constexpr int ball_height = diameter / 2;
constexpr int overview_focal = 149;
constexpr int overview_center_y = 78;

struct CameraPitch
{
    int sine;
    int cosine;
};

constexpr CameraPitch overview_pitch{ 96, 28 };
constexpr CameraPitch aim_pitch{ 52, 85 };

// Fixed perspective camera. The offline table mesh uses this same projection.
constexpr int camera_depth(Vec point, int height)
{
    return 160 * unit - (overview_pitch.cosine * (point.y - 84 * unit) + overview_pitch.sine * height) / 100;
}

constexpr Vec project(Vec point, int height = 0)
{
    int depth = camera_depth(point, height);
    int vertical = (overview_pitch.sine * (point.y - 84 * unit) - overview_pitch.cosine * height) / 100;
    return { 120 * unit + (point.x - 120 * unit) * (overview_focal * unit) / depth,
             overview_center_y * unit + vertical * (overview_focal * unit) / depth };
}

struct CameraPoint
{
    int x;
    int y;
    int depth;
};

constexpr int move_pocket_call(int pocket, int horizontal, int vertical)
{
    constexpr int clockwise[] = { 0, 1, 2, 5, 4, 3 };
    if(horizontal)
    {
        int index = pocket < 3 ? pocket : 8 - pocket;
        return clockwise[(index + horizontal + 6) % 6];
    }
    return vertical ? (pocket + 3) % 6 : pocket;
}

constexpr int view_top = 0;
constexpr int view_bottom = 160;
constexpr int near_depth = 9 * unit;
constexpr int sphere_sprite_size = 32;
constexpr int sphere_words = sphere_sprite_size * sphere_sprite_size / 8;

struct SphereProjection
{
    Vec center;
    int distance_squared;
    int radius_x;
    int radius_y;
    int skew;

    Vec pixel_center() const { return { (center.x + unit / 2) >> 8, (center.y + unit / 2) >> 8 }; }
    bool visible() const;
    bool covers(Vec pixel_center) const;
};

SphereProjection project_sphere(Vec point);
bool shadow_fits_on_cloth(Vec point);
bool clip_view_segment(Vec& start, Vec& end);

class AimCamera
{
public:
    AimCamera() = default;
    AimCamera(Vec cue, int angle);

    CameraPitch pitch() const { return tilt; }

    Vec position() const { return eye; }
    CameraPoint local(Vec point, int height = 0) const;
    static CameraPoint clip_near(CameraPoint outside, CameraPoint inside);
    Vec screen(CameraPoint point) const;
    unsigned outside_sides(CameraPoint point) const
    {
        int projected_x = point.x * focal;
        int limit = 122 * point.depth;
        return unsigned(projected_x < -limit) | (unsigned(projected_x > limit) << 1);
    }
    SphereProjection sphere(Vec point) const;
    int cue_tip_row(int setback) const;
    bool segment(Vec& start, Vec& end, int height = 0) const;

private:
    Vec eye;
    Vec forward{ direction_unit, 0 };
    CameraPitch tilt = aim_pitch;
    int elevation = 43 * unit;
    int focal = 166;
    int screen_center_y = 96;
};

struct RollingBall
{
    int x = 0;
    int y = 0;
    int z = direction_unit;

    void roll(Vec displacement);
    // Returns the mask of tiles written to the scratch buffer.
    uint16_t draw(int number, const SphereProjection& projection, Vec screen_right,
                  CameraPitch pitch, uint32_t* tiles) const;
};
}

#endif
