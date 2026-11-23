#ifndef SHOT_LINE_H
#define SHOT_LINE_H

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "pool.h"

namespace pool
{
enum class LineInk { cue = 1, object, bank, warning };

struct LineSpan
{
    Vec start;
    Vec end;
};

class ShotLine
{
public:
    void show();
    void close();
    void clear();
    void begin();
    void commit();
    void draw(Vec start, Vec end, LineInk ink, int lead, int tail, const LineSpan* avoid = nullptr);

private:
    bn::optional<bn::regular_bg_ptr> background;
};
}

#endif
