#include "aim_table.h"

#include <algorithm>
#include <cstdlib>

#include "bn_memory.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_tile.h"

#if defined(__GBA__) && defined(__GNUC__)
#pragma GCC target("arm")
#endif

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
constexpr int screen_subpixel_bits = 4;
constexpr int edge_precision_bits = 12;
constexpr int screen_subpixels = 1 << screen_subpixel_bits;
constexpr int edge_precision = 1 << edge_precision_bits;
constexpr int side_lip_x[] = { 113, 117, 123, 127 };
constexpr int side_lip_y[] = { 3, 1, 1, 3 };

BN_DATA_EWRAM_BSS bn::tile pixels[1 + columns * rows];
BN_DATA_EWRAM_BSS bn::regular_bg_map_cell cells[32 * 32];
BN_DATA_EWRAM_BSS uint32_t pocket_rows[height];
constexpr bn::color colors[16] = {
    { 0, 0, 0 }, { 10, 6, 5 }, { 16, 9, 6 }, { 22, 14, 9 },
    { 24, 16, 10 }, { 13, 7, 5 }, { 7, 16, 12 }, { 9, 20, 14 },
    { 9, 7, 7 }, { 2, 3, 5 }, { 12, 8, 6 }, { 20, 11, 7 },
    { 26, 21, 14 }, { 19, 12, 8 }, { 7, 17, 13 }, { 6, 4, 5 }
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

inline unsigned pixel_color(int x, int y)
{
    const bn::tile& tile = pixels[1 + ((y >> 3) - first_row) * columns + (x >> 3)];
    return (tile.data[y & 7] >> ((x & 7) * 4)) & 15u;
}

constexpr unsigned cloth_mask = (1u << 6) | (1u << 7) | (1u << 14);
constexpr unsigned leather_mask = (1u << 8) | (1u << 9) | (1u << 10) | (1u << 15);
constexpr unsigned wood_mask = (1u << 3) | (1u << 4);

constexpr uint32_t nibble_bits = 0x11111111u;

inline uint32_t cloth_positions(uint32_t row)
{
    return (row >> 1) & (row >> 2) & (~(row >> 3) | ~row) & nibble_bits;
}

inline uint32_t leather_positions(uint32_t row)
{
    return (row >> 3) & ~((row >> 2) ^ (row & (row >> 1))) & nibble_bits;
}

inline uint32_t wood_positions(uint32_t row)
{
    // These are the contiguous rail and apron materials, palette entries 1 through 5.
    return ~(row >> 3) & ((row >> 2) | (row >> 1) | row) &
           ~((row >> 2) & (row >> 1)) & nibble_bits;
}

BN_CODE_IWRAM bool seal_pocket_row(int y, uint32_t tiles)
{
    bool changed = false;
    int tile_row = (y >> 3) - first_row;
    const bn::tile* middle = pixels + 1 + tile_row * columns;
    const bn::tile* above = middle - (y % 8 == 0 ? columns : 0);
    const bn::tile* below = middle + (y % 8 == 7 ? columns : 0);
    while(tiles)
    {
        int column = __builtin_ctz(tiles);
        tiles &= tiles - 1;
        uint32_t row = middle[column].data[y & 7];
        uint32_t cloth = cloth_positions(row);
        uint32_t wood = wood_positions(row);
        uint32_t surface = cloth | wood;
        if(! surface || cloth == nibble_bits || wood == nibble_bits)
        {
            continue;
        }
        uint32_t leather = leather_positions(row);
        uint32_t near_leather = (leather << 4) | (leather >> 4) |
                                leather_positions(above[column].data[(y - 1) & 7]) |
                                leather_positions(below[column].data[(y + 1) & 7]);
        if(column > 0 && (surface & 1))
        {
            near_leather |= leather_positions(middle[column - 1].data[y & 7]) >> 28;
        }
        if(column + 1 < columns && (surface & (1u << 28)))
        {
            near_leather |= (leather_positions(middle[column + 1].data[y & 7]) & 1) << 28;
        }
        uint32_t candidates = surface & near_leather;
        while(candidates)
        {
            unsigned bit = candidates & (0u - candidates);
            int x = column * 8 + (__builtin_ctz(bit) >> 2);
            candidates &= candidates - 1;
            if(x == 0 || x == 239)
            {
                continue;
            }

            unsigned west = 1u << pixel_color(x - 1, y);
            unsigned east = 1u << pixel_color(x + 1, y);
            unsigned north = 1u << pixel_color(x, y - 1);
            unsigned south = 1u << pixel_color(x, y + 1);
            int cloth_neighbors = bool(west & cloth_mask) + bool(east & cloth_mask) +
                                  bool(north & cloth_mask) + bool(south & cloth_mask);
            int leather_neighbors = bool(west & leather_mask) + bool(east & leather_mask) +
                                    bool(north & leather_mask) + bool(south & leather_mask);
            unsigned neighbors = west | east | north | south;
            if(wood & bit)
            {
                if(leather_neighbors >= 2 && !(neighbors & ~(cloth_mask | leather_mask)))
                {
                    pixel(x, y, 15);
                    changed = true;
                }
                continue;
            }
            if(cloth_neighbors == 0 && leather_neighbors >= 2 && !(neighbors & ~(wood_mask | leather_mask)))
            {
                pixel(x, y, 15);
                changed = true;
                continue;
            }
            if(cloth_neighbors > (neighbors & wood_mask ? 2 : 1))
            {
                continue;
            }
            int lining = 0;
            for(int dy = -1; dy <= 1; ++dy)
            {
                for(int dx = -1; dx <= 1; ++dx)
                {
                    lining += (leather_mask >> pixel_color(x + dx, y + dy)) & 1u;
                }
            }
            if(lining >= 4)
            {
                pixel(x, y, 15);
                changed = true;
            }
        }
    }
    return changed;
}

void seal_pocket_seams()
{
    bool retry[height] = {};
    for(int y = canvas_top + 1; y < view_bottom - 1; ++y)
    {
        int row = y - canvas_top;
        if(pocket_rows[row] && seal_pocket_row(y, pocket_rows[row]))
        {
            retry[row - 1] = true;
            retry[row] = true;
            retry[row + 1] = true;
        }
    }
    // A repaired pixel can expose another seam pixel in an adjacent row.
    for(int y = canvas_top + 1; y < view_bottom - 1; ++y)
    {
        int row = y - canvas_top;
        if(retry[row] && pocket_rows[row])
        {
            seal_pocket_row(y, pocket_rows[row]);
        }
    }
}

struct ScanEdges
{
    int top;
    int bottom;
    int first[height];
    int second[height];

    int left(int y) const { return std::min(first[y - canvas_top], second[y - canvas_top]); }
    int right(int y) const { return std::max(first[y - canvas_top], second[y - canvas_top]); }
};

constexpr int first_sample_row(int y)
{
    return (y + screen_subpixels / 2 - 1) >> screen_subpixel_bits;
}

BN_CODE_IWRAM bool scan_edges(const Vec* points, int count, ScanEdges& edges)
{
    int min_y = view_bottom * screen_subpixels;
    int max_y = canvas_top * screen_subpixels;
    int min_x = 240 * screen_subpixels;
    int max_x = -1;
    for(int i = 0; i < count; ++i)
    {
        min_y = std::min(min_y, points[i].y);
        max_y = std::max(max_y, points[i].y);
        min_x = std::min(min_x, points[i].x);
        max_x = std::max(max_x, points[i].x);
    }
    edges.top = std::max(canvas_top, first_sample_row(min_y));
    edges.bottom = std::min(view_bottom, first_sample_row(max_y));
    if(edges.top >= edges.bottom || min_x >= 240 * screen_subpixels || max_x < 0)
    {
        return false;
    }

    // Sample at pixel centers with half-open edges so adjoining faces share a pixel boundary.
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
        int begin = std::max(first_sample_row(a.y), edges.top);
        int end = std::min(first_sample_row(b.y), edges.bottom);
        if(begin >= end)
        {
            continue;
        }
        int slope = (b.x - a.x) * edge_precision / (b.y - a.y);
        int x = a.x * edge_precision + slope * (begin * screen_subpixels + screen_subpixels / 2 - a.y);
        int step = slope * screen_subpixels;
        for(int y = begin; y < end; ++y)
        {
            edge[y - canvas_top] = (x + screen_subpixels * edge_precision / 2 - 1) >>
                                   (screen_subpixel_bits + edge_precision_bits);
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
    for(int y = edges.top; y < edges.bottom; ++y)
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
        constexpr int screen_scale = unit / screen_subpixels;
        constexpr int scale_bits = 8 - screen_subpixel_bits;
        projected[count++] = { (screen.x + screen_scale / 2) >> scale_bits,
                               (screen.y + screen_scale / 2) >> scale_bits };
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

void mark_pocket_rows(const Vec* points, int count)
{
    if(count < 3)
    {
        return;
    }
    Vec low = points[0];
    Vec high = low;
    for(int index = 1; index < count; ++index)
    {
        low.x = std::min(low.x, points[index].x);
        low.y = std::min(low.y, points[index].y);
        high.x = std::max(high.x, points[index].x);
        high.y = std::max(high.y, points[index].y);
    }
    int left = std::max(0, ((low.x >> screen_subpixel_bits) - 13) / 8);
    int right = std::min(columns, ((high.x >> screen_subpixel_bits) + 13) / 8 + 1);
    if(left >= right)
    {
        return;
    }
    uint32_t mask = ((1u << right) - 1) & ~((1u << left) - 1);
    int top = std::max(canvas_top + 1, (low.y >> screen_subpixel_bits) - 20);
    int bottom = std::min(view_bottom - 1, (high.y >> screen_subpixel_bits) + 10);
    for(int y = top; y < bottom; ++y)
    {
        pocket_rows[y - canvas_top] |= mask;
    }
}

template<int Mouth, int Floor>
void pocket_well(const AimCamera& camera, const Vertex (&mouth)[Mouth], const Vertex (&floor)[Floor])
{
    Vec projected_mouth[Mouth + 1];
    ScanEdges opening;
    int count = project_face(camera, mouth, projected_mouth);
    mark_pocket_rows(projected_mouth, count);
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
        // A one-pixel leather seam closes subpixel joins with the side facings.
        span(left - 1, right + 1, y, 0xffffffff);
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
            for(Vec pocket : pockets)
            {
                int dx = x - pocket.x / unit;
                int dy = y - pocket.y / unit;
                if(dx * dx + dy * dy < 17 * 17)
                {
                    x = 0;
                    break;
                }
            }
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
        Vec mark = cloth_marks.points[index];
        if(! mark.x)
        {
            continue;
        }
        CameraPoint point = camera.local(mark);
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

void rail_sight(const AimCamera& camera, int x, int y, int across_x, int across_y)
{
    if(camera.local(px(x, y), rail_top * unit).depth > 105 * unit)
    {
        return;
    }
    int along_x = across_y * 2;
    int along_y = across_x * 2;
    face(camera, {{ x - along_x, y - along_y, rail_top }, { x - across_x, y - across_y, rail_top },
                   { x + along_x, y + along_y, rail_top }, { x + across_x, y + across_y, rail_top }}, 12);
}

void long_rail(const AimCamera& camera, int edge, int inward)
{
    int back = edge + inward * 3;
    int inner = edge + inward * 8;
    int nose = edge + inward * 11;
    quad(camera, end_left, std::min(edge, back), side_lip_x[0], std::max(edge, back), rail_top, 4);
    quad(camera, side_lip_x[3], std::min(edge, back), end_right, std::max(edge, back), rail_top, 4);
    for(int index = 0; index < 3; ++index)
    {
        int x0 = side_lip_x[index];
        int x1 = side_lip_x[index + 1];
        int y0 = edge + inward * side_lip_y[index];
        int y1 = edge + inward * side_lip_y[index + 1];
        face(camera, {{ x0, edge, rail_top }, { x1, edge, rail_top },
                       { x1, y1, rail_top }, { x0, y0, rail_top }}, 4);
    }

    face(camera, {{ 20, back, rail_top }, { 114, back, rail_top },
                   { 112, inner, rail_top }, { 25, inner, rail_top }}, 3);
    face(camera, {{ 126, back, rail_top }, { 220, back, rail_top },
                   { 215, inner, rail_top }, { 128, inner, rail_top }}, 3);
    int cushion = inward > 0 ? 7 : 6;
    face(camera, {{ 25, inner, rail_top }, { 112, inner, rail_top }, { 108, nose, 0 }, { 29, nose, 0 }},
         cushion);
    face(camera, {{ 128, inner, rail_top }, { 215, inner, rail_top }, { 211, nose, 0 }, { 132, nose, 0 }},
         cushion);

    for(int first : { 26, 130 })
    {
        for(int x = first + 15; x < first + 74; x += 26)
        {
            rail_sight(camera, x, back + inward * 2, 0, 1);
        }
    }
}

void short_rail(const AimCamera& camera, int edge, int inward)
{
    int back = edge + inward * 5;
    int inner = edge + inward * 8;
    int nose = edge + inward * 11;
    quad(camera, std::min(edge, back), 32, std::max(edge, back), 136, rail_top, 4);
    face(camera, {{ back, 47, rail_top }, { inner, 50, rail_top },
                   { inner, 118, rail_top }, { back, 121, rail_top }}, 3);
    face(camera, {{ inner, 50, rail_top }, { nose, 54, 0 }, { nose, 114, 0 }, { inner, 118, rail_top }}, 6);

    for(int y : { 62, 84, 106 })
    {
        rail_sight(camera, back + inward, y, 1, 0);
    }
}

bool pocket_may_be_visible(const AimCamera& camera, Vec center)
{
    CameraPoint point = camera.local(center);
    // The entire pocket is inside this conservative camera-space margin.
    constexpr int margin = 24 * unit;
    point.depth += margin;
    point.x -= std::clamp(point.x, -margin, margin);
    return point.depth >= near_depth && ! camera.outside_sides(point);
}

void side_pocket(const AimCamera& camera, int edge, int inward)
{
    if(! pocket_may_be_visible(camera, px(120, edge + inward * 10)))
    {
        return;
    }
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
    for(int index = 0; index < 3; ++index)
    {
        int x0 = side_lip_x[index];
        int x1 = side_lip_x[index + 1];
        int y0 = edge + inward * side_lip_y[index];
        int y1 = edge + inward * side_lip_y[index + 1];
        face(camera, {{ x0, y0, rail_top }, { x1, y1, rail_top },
                       { x1, y1 + inward * 4, 0 }, { x0, y0 + inward * 4, 0 }}, 8);
    }

    // Leather covers the cut ends of the cushions inside the mouth.
    face(camera, {{ 113, back, rail_top }, { 112, inner, rail_top }, { 108, nose, 0 }}, 8);
    face(camera, {{ 113, back, rail_top }, { 108, nose, 0 }, { 113, back + inward * 4, 0 }}, 8);
    face(camera, {{ 127, back, rail_top }, { 128, inner, rail_top }, { 132, nose, 0 }}, 8);
    face(camera, {{ 127, back, rail_top }, { 132, nose, 0 }, { 127, back + inward * 4, 0 }}, 8);
}

void corner_pocket(const AimCamera& camera, int x, int y)
{
    int dx = x == end_left ? 1 : -1;
    int dy = y == 32 ? 1 : -1;
    if(! pocket_may_be_visible(camera, px(x + dx * 12, y + dy * 12)))
    {
        return;
    }
    auto point = [=](int u, int v, int z) -> Vertex { return { x + dx * u, y + dy * v, z }; };
    pocket_well(camera, { point(5, 3, 0), point(14, 3, 0), point(23, 11, 0), point(20, 15, 0),
                          point(17, 18, 0), point(13, 21, 0), point(9, 22, 0), point(5, 17, 0) },
                         { point(6, 4, -4), point(14, 4, -4), point(20, 11, -4), point(19, 14, -4),
                           point(16, 17, -4), point(12, 19, -4), point(9, 19, -4), point(6, 16, -4) });

    // The rounded collar joins the two wood rails around the back of the drop.
    constexpr int lip[][2] = { { 14, 3 }, { 10, 3 }, { 7, 4 }, { 5, 7 }, { 5, 15 } };
    face(camera, { point(5, 3, rail_top), point(10, 3, rail_top),
                   point(7, 4, rail_top), point(5, 7, rail_top) }, 8);
    for(int index = 0; index < 4; ++index)
    {
        Vertex a = point(lip[index][0], lip[index][1], rail_top);
        Vertex b = point(lip[index + 1][0], lip[index + 1][1], rail_top);
        Vertex c = point(lip[index + 1][0] + 2, lip[index + 1][1] + 2, 0);
        Vertex d = point(lip[index][0] + 2, lip[index][1] + 2, 0);
        face(camera, { a, b, c, d }, 8);
    }

    face(camera, { point(14, 3, rail_top), point(19, 8, rail_top), point(23, 11, 0) }, 8);
    face(camera, { point(14, 3, rail_top), point(23, 11, 0), point(16, 5, 0) }, 8);
    face(camera, { point(5, 15, rail_top), point(8, 18, rail_top), point(11, 22, 0) }, 8);
    face(camera, { point(5, 15, rail_top), point(11, 22, 0), point(7, 17, 0) }, 8);
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
    bn::memory::clear(pocket_rows);
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
    seal_pocket_seams();
}
}
