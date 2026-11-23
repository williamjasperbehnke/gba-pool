#include "shot_line.h"

#include <cstdlib>
#include <cstring>

#include "bn_bg_palette_item.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_map_item.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_tile.h"

namespace pool
{
namespace
{
constexpr int columns = 30;
constexpr int rows = view_bottom / 8;
// A single line visits at most one new tile for each column or row it crosses.
// The contact preview fits within three tiles per axis.
constexpr int tile_capacity = columns + rows + 9;

alignas(4) bn::tile pixels[1 + tile_capacity];
alignas(4) bn::regular_bg_map_cell cells[32 * 32];
int next_tile = 1;
constexpr bn::color ink[16] = {
    { 0, 0, 0 }, { 18, 26, 22 }, { 31, 18, 15 },
    { 12, 21, 17 }, { 17, 25, 21 }, { 17, 17, 13 }, { 27, 18, 14 }
};

constexpr int ghost_extent = (ghost_ball_radius + unit / 2) / unit;
constexpr int ghost_size = 2 * ghost_extent + 1;
constexpr int visible_coverage = 12;

constexpr auto make_ghost_mask()
{
    struct Mask { uint8_t coverage[ghost_size][ghost_size]{}; } mask;
    constexpr int inner_radius = 3 * unit + 7 * unit / 16;
    for(int y = -ghost_extent; y <= ghost_extent; ++y)
    {
        for(int x = -ghost_extent; x <= ghost_extent; ++x)
        {
            for(int dy = -7; dy <= 7; dy += 2)
            {
                for(int dx = -7; dx <= 7; dx += 2)
                {
                    int sx = x * unit + dx * (unit / 16);
                    int sy = y * unit + dy * (unit / 16);
                    int squared = sx * sx + sy * sy;
                    mask.coverage[y + ghost_extent][x + ghost_extent] +=
                            squared >= inner_radius * inner_radius && squared <= ghost_ball_radius * ghost_ball_radius;
                }
            }
        }
    }
    return mask;
}

constexpr auto ghost_mask = make_ghost_mask();

int screen_pixel(int value)
{
    // The sphere raster samples pixel centers, so select the pixel containing the projected point.
    return value >> 8;
}

void plot(int x, int y, unsigned color)
{
    int column = x >> 3;
    int row = y >> 3;
    if(unsigned(column) >= columns || unsigned(row) >= rows)
    {
        return;
    }
    auto& cell = cells[row * 32 + column];
    if(! cell)
    {
        cell = bn::regular_bg_map_cell(next_tile++);
        pixels[cell] = {};
    }
    uint32_t& bits = pixels[cell].data[y & 7];
    int shift = (x & 7) * 4;
    if(! (bits & (15u << shift)))
    {
        bits |= uint32_t(color) << shift;
    }
}
void raster_line(Vec start, Vec end, LineInk color, bool stop_at_ghost)
{
    int x = screen_pixel(start.x);
    int y = screen_pixel(start.y);
    int target_x = screen_pixel(end.x);
    int target_y = screen_pixel(end.y);
    int dx = std::abs(target_x - x);
    int dy = std::abs(target_y - y);
    int step_x = x < target_x ? 1 : -1;
    int step_y = y < target_y ? 1 : -1;
    int error = dx - dy;

    for(;;)
    {
        // Join the first visible pixel of the same mask used to draw the ring.
        if(stop_at_ghost)
        {
            unsigned column = unsigned(x - target_x + ghost_extent);
            unsigned row = unsigned(y - target_y + ghost_extent);
            if(column < ghost_size && row < ghost_size && ghost_mask.coverage[row][column] >= visible_coverage)
            {
                break;
            }
        }
        plot(x, y, unsigned(color));
        if(x == target_x && y == target_y)
        {
            break;
        }
        int twice = error * 2;
        if(twice > -dy)
        {
            error -= dy;
            x += step_x;
        }
        if(twice < dx)
        {
            error += dx;
            y += step_y;
        }
    }
}
}

void ShotLine::show()
{
    if(background)
    {
        return;
    }
    begin();
    bn::regular_bg_item item(pixels, ink, bn::bpp_mode::BPP_4, cells[0], bn::size(32, 32));
    background = item.create_bg(8, 48);
    background->set_priority(2);
    background->set_visible(false);
}

void ShotLine::close()
{
    background.reset();
}

void ShotLine::clear()
{
    if(background)
    {
        background->set_visible(false);
    }
}

void ShotLine::begin()
{
    std::memset(cells, 0, sizeof(cells));
    next_tile = 1;
}

void ShotLine::commit()
{
    bn::regular_bg_tiles_ptr tiles = background->tiles();
    tiles.reload_tiles_ref();
    bn::regular_bg_map_ptr map = background->map();
    map.reload_cells_ref();
    background->set_visible(true);
}

void ShotLine::draw(Vec screen_start, Vec screen_end, LineInk color)
{
    raster_line(screen_start, screen_end, color, false);
}

bool ShotLine::ghost_visible(Vec center, const SphereProjection* nearby, int count)
{
    int shown = 0;
    int samples = 0;
    for(int y = 0; y < ghost_size; ++y)
    {
        for(int x = 0; x < ghost_size; ++x)
        {
            if(ghost_mask.coverage[y][x] < visible_coverage)
            {
                continue;
            }
            ++samples;
            Vec point = center + px(x - ghost_extent, y - ghost_extent);
            int index = 0;
            while(index < count && ! nearby[index].covers(point))
            {
                ++index;
            }
            shown += index == count;
        }
    }
    return 3 * shown >= 2 * samples;
}

void ShotLine::draw_to_ghost(Vec screen_start, Vec ghost_center, LineInk color)
{
    int x0 = screen_pixel(ghost_center.x) - ghost_extent;
    int y0 = screen_pixel(ghost_center.y) - ghost_extent;
    unsigned soft_ink = 3 + 2 * (unsigned(color) - 1);
    for(int y = 0; y < ghost_size; ++y)
    {
        for(int x = 0; x < ghost_size; ++x)
        {
            int coverage = ghost_mask.coverage[y][x];
            if(coverage >= visible_coverage)
            {
                unsigned shade = coverage >= 32 ? unsigned(color) : soft_ink + unsigned(coverage >= 24);
                plot(x0 + x, y0 + y, shade);
            }
        }
    }
    raster_line(screen_start, ghost_center, color, true);
}

}
