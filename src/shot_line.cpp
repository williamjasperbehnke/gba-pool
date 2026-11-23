#include "shot_line.h"

#include <cstdlib>
#include <cstring>

#include "bn_bg_palette_item.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_map_item.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_tile.h"

namespace pool
{
namespace
{
constexpr int first_column = 1;
constexpr int first_row = 4;
constexpr int columns = 28;
constexpr int rows = 13;

alignas(4) bn::tile pixels[1 + columns * rows];
alignas(4) bn::regular_bg_map_cell cells[32 * 32];
constexpr bn::color ink[] = {
    bn::color(0, 0, 0), bn::color(23, 29, 25), bn::color(31, 26, 15), bn::color(15, 24, 20),
    bn::color(31, 18, 15), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
    bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
    bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0)
};

int screen_pixel(int value)
{
    return (value + unit / 2) / unit;
}

void plot(int x, int y, LineInk color)
{
    int column = (x >> 3) - first_column;
    int row = (y >> 3) - first_row;
    if(column >= 0 && column < columns && row >= 0 && row < rows)
    {
        uint32_t& bits = pixels[1 + row * columns + column].data[y & 7];
        int shift = (x & 7) * 4;
        if(!(bits & (15u << shift)))
        {
            bits |= uint32_t(color) << shift;
        }
    }
}
}

void ShotLine::show()
{
    for(int y = 0; y < rows; ++y)
    {
        for(int x = 0; x < columns; ++x)
        {
            cells[(first_row + y) * 32 + first_column + x] = bn::regular_bg_map_cell(1 + y * columns + x);
        }
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
    std::memset(pixels, 0, sizeof(pixels));
}

void ShotLine::commit()
{
    bn::regular_bg_tiles_ptr tiles = background->tiles();
    tiles.reload_tiles_ref();
    background->set_visible(true);
}

void ShotLine::draw(Vec start, Vec end, LineInk color, int lead, int tail, const LineSpan* avoid)
{
    Vec delta = end - start;
    int distance = length(delta);
    if(distance <= (lead + tail) * unit)
    {
        return;
    }
    Vec axis = normalized(delta);
    Vec from = start + travel(axis, lead * unit);
    Vec to = end - travel(axis, tail * unit);
    int x = screen_pixel(from.x);
    int y = screen_pixel(from.y);
    int target_x = screen_pixel(to.x);
    int target_y = screen_pixel(to.y);
    int dx = std::abs(target_x - x);
    int dy = std::abs(target_y - y);
    int step_x = x < target_x ? 1 : -1;
    int step_y = y < target_y ? 1 : -1;
    int error = dx - dy;

    Vec other_delta = avoid ? avoid->end - avoid->start : Vec{};
    int other_length = length(other_delta);
    Vec other_axis = normalized(other_delta);
    for(;;)
    {
        bool near_first = false;
        if(avoid)
        {
            Vec relative = px(x, y) - avoid->start;
            int along = (relative.x * other_axis.x + relative.y * other_axis.y) / direction_unit;
            int across = (relative.y * other_axis.x - relative.x * other_axis.y) / direction_unit;
            near_first = along > -6 * unit && along < other_length + unit && std::abs(across) < 3 * unit;
        }
        if(! near_first)
        {
            plot(x, y, color);
        }
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
