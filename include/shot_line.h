#ifndef SHOT_LINE_H
#define SHOT_LINE_H

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "pool_view.h"

namespace pool
{
enum class LineInk { cue = 1, warning, bank = 7 };
constexpr int ghost_ball_radius = 4 * unit;

class ShotLine
{
public:
    void show();
    void close();
    void clear();
    void begin();
    void commit();
    void draw(Vec screen_start, Vec screen_end, LineInk ink);
    void draw_to_ghost(Vec screen_start, Vec ghost_center, LineInk ink);
    static bool ghost_visible(Vec center, const SphereProjection* nearby, int count);

private:
    bn::optional<bn::regular_bg_ptr> background;
};
}

#endif
