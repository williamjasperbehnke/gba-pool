#include "pool.h"

#include <algorithm>

namespace pool
{
namespace
{
constexpr int velocity_scale = 256;
constexpr int object_mask = 0xfefe;

constexpr int abs_value(int value) { return value < 0 ? -value : value; }

int root(unsigned value)
{
    if(! value)
    {
        return 0;
    }
    unsigned bit = 1u << ((31 - __builtin_clz(value)) & ~1);
    unsigned result = 0;
    while(bit)
    {
        if(value >= result + bit)
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }
        bit >>= 2;
    }
    return int(result);
}

int dot(Vec value, Vec heading)
{
    return (value.x * heading.x + value.y * heading.y) / direction_unit;
}

constexpr double sine(double x)
{
    double square = x * x;
    return x * (1 - square / 6 * (1 - square / 20 * (1 - square / 42 *
               (1 - square / 72 * (1 - square / 110)))));
}

constexpr auto make_sines()
{
    struct Lookup { int values[257]; };
    Lookup lookup{};
    for(int i = 0; i <= 256; ++i)
    {
        lookup.values[i] = int(sine(i * 1.5707963267948966 / 256) * direction_unit + 0.5);
    }
    return lookup;
}

constexpr auto sines = make_sines();

int sin_direction(int angle)
{
    angle &= 1023;
    int quadrant = angle / 256;
    int offset = angle & 255;
    int value = sines.values[(quadrant & 1) ? 256 - offset : offset];
    return quadrant >= 2 ? -value : value;
}

int pocket_radius(int id)
{
    return (id == 1 || id == 4) ? 8 * unit : 9 * unit;
}

// Returns the first distance at which a moving center intersects a circle.
int entry(Vec offset, Vec heading, int radius)
{
    int along = dot(offset, heading);
    int across = dot(offset, { -heading.y, heading.x });
    if(abs_value(across) >= radius || along + radius < 0)
    {
        return -1;
    }
    int half_chord = root(unsigned(radius * radius - across * across));
    if(along + half_chord < 0)
    {
        return -1;
    }
    return std::max(0, along - half_chord);
}

Group group_of(int ball)
{
    if(ball >= 1 && ball <= 7)
    {
        return Group::solids;
    }
    return ball >= 9 ? Group::stripes : Group::open;
}

int group_mask(Group group)
{
    return group == Group::solids ? 0xfe : 0xfe00;
}

int random_below(uint32_t& seed, int max)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return int((seed >> 8) % unsigned(max));
}
}

int closest_pocket(Vec point)
{
    int nearest = 0;
    int distance = length(point - pockets[0]);
    for(int pocket = 1; pocket < 6; ++pocket)
    {
        int range = length(point - pockets[pocket]);
        if(range < distance)
        {
            nearest = pocket;
            distance = range;
        }
    }
    return nearest;
}

Vec direction(int angle)
{
    return { sin_direction(angle + 256), sin_direction(angle) };
}

int direction_angle(Vec value)
{
    int x = abs_value(value.x);
    int y = abs_value(value.y);
    int low = 0;
    int high = 256;
    while(high - low > 1)
    {
        int mid = (low + high) / 2;
        if(y * sines.values[256 - mid] > x * sines.values[mid])
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }
    int low_error = abs_value(y * sines.values[256 - low] - x * sines.values[low]);
    int high_error = abs_value(y * sines.values[256 - high] - x * sines.values[high]);
    int result = high_error < low_error ? high : low;
    if(value.x < 0)
    {
        result = 512 - result;
    }
    return (value.y < 0 ? -result : result) & (angle_count - 1);
}

int length(Vec value)
{
    // Scale wide vectors before squaring to keep both products and their sum in range.
    int scale = 1;
    while(abs_value(value.x) > 32000 || abs_value(value.y) > 32000)
    {
        value.x /= 2;
        value.y /= 2;
        scale *= 2;
    }
    return root(unsigned(value.x * value.x + value.y * value.y)) * scale;
}

Vec normalized(Vec value)
{
    int magnitude = length(value);
    return magnitude ? Vec{ value.x * direction_unit / magnitude, value.y * direction_unit / magnitude } :
                       Vec{ direction_unit, 0 };
}

Vec travel(Vec heading, int distance)
{
    return { heading.x * distance / direction_unit, heading.y * distance / direction_unit };
}

void Table::rack()
{
    balls[0] = { px(63, 84), {}, true };
    constexpr int order[] = { 1, 12, 6, 10, 8, 3, 7, 14, 4, 11, 5, 9, 2, 13, 15 };
    int index = 0;
    for(int column = 0; column < 5; ++column)
    {
        for(int row = 0; row <= column; ++row)
        {
            balls[order[index++]] = { { 154 * unit + column * 8 * unit,
                                         84 * unit + (row * 2 - column) * (9 * unit / 2) }, {}, true };
        }
    }
    shot = {};
}

void Table::strike(Vec heading, int speed, int called_pocket, CueSpin spin)
{
    shot = {};
    shot.called_pocket = int8_t(called_pocket);
    shot.cue_spin = encode_spin(spin);
    balls[0].velocity = travel(heading, speed * velocity_scale);
}

void Table::cushion_or_pocket(int id)
{
    Ball& ball = balls[id];
    if(ball.pos.x < left + 7 * unit || ball.pos.x > right - 7 * unit ||
       ball.pos.y < top + 6 * unit || ball.pos.y > bottom - 6 * unit)
    {
        for(int pocket = 0; pocket < 6; ++pocket)
        {
            Vec delta = ball.pos - pockets[pocket];
            int radius = pocket_radius(pocket);
            if(abs_value(delta.x) < radius && abs_value(delta.y) < radius &&
               delta.x * delta.x + delta.y * delta.y < radius * radius)
            {
                ball.live = false;
                ball.velocity = {};
                shot.pocketed |= uint16_t(1u << id);
                if(id == 8)
                {
                    shot.eight_pocket = int8_t(pocket);
                }
                if(id && shot.first_pot < 0)
                {
                    shot.first_pot = id;
                }
                return;
            }
        }
    }

    bool hit_x = ball.pos.x < left || ball.pos.x > right;
    bool hit_y = ball.pos.y < top || ball.pos.y > bottom;
    Vec incoming = ball.velocity;
    if(hit_x)
    {
        ball.pos.x = std::clamp(ball.pos.x, left, right);
        ball.velocity.x = -ball.velocity.x * 218 / 256;
        shot.rail_after_contact |= shot.first >= 0;
    }
    if(hit_y)
    {
        ball.pos.y = std::clamp(ball.pos.y, top, bottom);
        ball.velocity.y = -ball.velocity.y * 218 / 256;
        shot.rail_after_contact |= shot.first >= 0;
    }
    if(id == 0 && (hit_x || hit_y) && shot.cue_spin != encode_spin({}))
    {
        CueSpin spin = decode_spin(shot.cue_spin);
        if(hit_x && ! hit_y)
        {
            ball.velocity.y += incoming.x * spin.side / 24;
        }
        if(hit_y && ! hit_x)
        {
            ball.velocity.x -= incoming.y * spin.side / 24;
        }
        spin.side -= (spin.side > 0) - (spin.side < 0);
        shot.cue_spin = encode_spin(spin);
    }
}

void Table::collide(int a, int b)
{
    Ball& first = balls[a];
    Ball& second = balls[b];
    Vec delta = second.pos - first.pos;
    if(abs_value(delta.x) >= diameter || abs_value(delta.y) >= diameter ||
       delta.x * delta.x + delta.y * delta.y >= diameter * diameter)
    {
        return;
    }

    int distance = length(delta);
    Vec axis = distance ? Vec{ delta.x * direction_unit / distance, delta.y * direction_unit / distance } :
                          Vec{ direction_unit, 0 };
    Vec correction = travel(axis, (diameter - distance) / 2 + 1);
    first.pos = first.pos - correction;
    second.pos += correction;

    Vec relative = first.velocity - second.velocity;
    Vec advance{ relative.x / velocity_scale, relative.y / velocity_scale };
    Vec previous = delta + advance;
    int closing = dot(relative, axis);
    if(closing > 0 && advance.x * advance.x + advance.y * advance.y > unit * unit / 16 &&
       previous.x * previous.x + previous.y * previous.y >= diameter * diameter)
    {
        // Use the contact angle at the edge of the balls, before they began to overlap.
        int range = length(advance);
        Vec path{ advance.x * direction_unit / range, advance.y * direction_unit / range };
        int hit = entry(previous, path, diameter);
        if(hit >= 0 && hit <= range)
        {
            axis = normalized(previous - travel(path, hit));
            closing = dot(relative, axis);
        }
    }
    if(closing > 0)
    {
        impact = std::max(impact, closing / velocity_scale);
        Vec impulse = travel(axis, closing * 248 / 256);
        Vec incoming = first.velocity;
        first.velocity = first.velocity - impulse;
        second.velocity += impulse;
        if(a == 0 && shot.first < 0)
        {
            shot.first = b;
            CueSpin spin = decode_spin(shot.cue_spin);
            if(spin.vertical && (incoming.x || incoming.y))
            {
                // Follow and draw act along the cue ball's approach after contact.
                Vec heading = normalized({ incoming.x / velocity_scale, incoming.y / velocity_scale });
                first.velocity += travel(heading, closing * spin.vertical / 11);
                spin.vertical = 0;
                shot.cue_spin = encode_spin(spin);
            }
        }
    }
}

void Table::resolve_positions(bool moving)
{
    // A collision or cushion can push a ball back into a neighbour in the same frame.
    // Separate the resulting clusters independently of the velocity impulses.
    int passes = moving ? 3 : 8;
    for(int pass = 0; pass < passes; ++pass)
    {
        bool changed = false;
        for(int a = 0; a < 15; ++a)
        {
            if(! balls[a].live)
            {
                continue;
            }
            for(int b = a + 1; b < 16; ++b)
            {
                if(! balls[b].live)
                {
                    continue;
                }
                Vec delta = balls[b].pos - balls[a].pos;
                if(abs_value(delta.x) >= diameter || abs_value(delta.y) >= diameter ||
                   delta.x * delta.x + delta.y * delta.y >= diameter * diameter)
                {
                    continue;
                }
                int gap = diameter + 4 - length(delta);
                Vec axis = normalized(delta);
                Vec correction = travel(axis, (gap + 1) / 2);
                Vec first = balls[a].pos;
                Vec second = balls[b].pos;
                auto clamp_position = [](Vec pos) {
                    return Vec{ std::clamp(pos.x, left, right), std::clamp(pos.y, top, bottom) };
                };
                balls[a].pos = clamp_position(first - correction);
                balls[b].pos = clamp_position(second + correction);
                // If a cushion blocks one ball, let the other absorb its share.
                Vec first_blocked = balls[a].pos - (first - correction);
                Vec second_blocked = (second + correction) - balls[b].pos;
                balls[a].pos = clamp_position(balls[a].pos - second_blocked);
                balls[b].pos = clamp_position(balls[b].pos + first_blocked);
                changed = true;
            }
        }
        if(! changed)
        {
            break;
        }
    }
}

bool Table::step()
{
    impact = 0;
    bool moving = false;
    for(int substep = 0; substep < 4; ++substep)
    {
        for(int i = 0; i < 16; ++i)
        {
            Ball& ball = balls[i];
            if(ball.live && (ball.velocity.x || ball.velocity.y))
            {
                ball.pos += { ball.velocity.x / velocity_scale, ball.velocity.y / velocity_scale };
                cushion_or_pocket(i);
            }
        }

        for(int a = 0; a < 15; ++a)
        {
            if(! balls[a].live)
            {
                continue;
            }
            for(int b = a + 1; b < 16; ++b)
            {
                if(balls[b].live && (balls[a].velocity.x || balls[a].velocity.y ||
                                     balls[b].velocity.x || balls[b].velocity.y))
                {
                    collide(a, b);
                }
            }
        }

        moving = false;
        for(int i = 0; i < 16; ++i)
        {
            Ball& ball = balls[i];
            if(ball.live && (ball.velocity.x || ball.velocity.y))
            {
                cushion_or_pocket(i);
            }
            int x_speed = abs_value(ball.velocity.x);
            int y_speed = abs_value(ball.velocity.y);
            int speed = std::max(x_speed, y_speed) + std::min(x_speed, y_speed) * 3 / 8;
            int drag = velocity_scale * (1 + speed / (unit * velocity_scale));
            if(speed <= 5 * velocity_scale)
            {
                ball.velocity = {};
            }
            else
            {
                // Retain enough velocity precision to preserve shallow shot angles.
                ball.velocity.x -= ball.velocity.x * drag / speed;
                ball.velocity.y -= ball.velocity.y * drag / speed;
                moving = true;
            }
        }
        if(! moving)
        {
            break;
        }
    }
    resolve_positions(moving);
    return moving;
}

bool Table::can_place(Vec pos, int ignore) const
{
    if(pos.x < left || pos.x > right || pos.y < top || pos.y > bottom)
    {
        return false;
    }
    for(int i = 0; i < 6; ++i)
    {
        Vec delta = pos - pockets[i];
        int clearance = pocket_radius(i) + unit;
        if(abs_value(delta.x) <= clearance && abs_value(delta.y) <= clearance &&
           delta.x * delta.x + delta.y * delta.y <= clearance * clearance)
        {
            return false;
        }
    }
    for(int i = 0; i < 16; ++i)
    {
        if(i != ignore && balls[i].live)
        {
            Vec delta = pos - balls[i].pos;
            constexpr int spacing = diameter + unit / 2;
            if(abs_value(delta.x) < spacing && abs_value(delta.y) < spacing &&
               delta.x * delta.x + delta.y * delta.y < spacing * spacing)
            {
                return false;
            }
        }
    }
    return true;
}

void Table::place_cue(Vec pos)
{
    balls[0] = { pos, {}, true };
}

Vec Table::free_spot(int ignore) const
{
    if(can_place(px(63, 84), ignore))
    {
        return px(63, 84);
    }
    for(int y = 55; y <= 115; y += 10)
    {
        for(int x = 30; x <= 210; x += 10)
        {
            if(can_place(px(x, y), ignore))
            {
                return px(x, y);
            }
        }
    }
    return px(63, 84);
}

void Table::spot_eight()
{
    for(int offset = 0; offset <= 66; ++offset)
    {
        for(int sign : { 1, -1 })
        {
            Vec pos = px(154 + sign * offset, 84);
            if(can_place(pos, 8))
            {
                balls[8] = { pos, {}, true };
                return;
            }
        }
    }
    balls[8] = { free_spot(8), {}, true };
}

Ray Table::trace(Vec origin, Vec heading, int ignore, int max_distance) const
{
    int distance = max_distance;
    Ray ray;
    if(heading.x)
    {
        int edge = heading.x > 0 ? right : left;
        int x_distance = (edge - origin.x) * direction_unit / heading.x;
        if(x_distance <= distance)
        {
            distance = std::max(0, x_distance);
            ray.rail = true;
        }
    }
    if(heading.y)
    {
        int edge = heading.y > 0 ? bottom : top;
        int y_distance = (edge - origin.y) * direction_unit / heading.y;
        if(y_distance <= distance)
        {
            distance = std::max(0, y_distance);
            ray.rail = true;
        }
    }

    for(int i = 0; i < 6; ++i)
    {
        int hit = entry(pockets[i] - origin, heading, pocket_radius(i));
        if(hit >= 0 && hit < distance)
        {
            distance = hit;
            ray.rail = false;
            ray.pocket = i;
        }
    }

    for(int i = 0; i < 16; ++i)
    {
        if(i == ignore || ! balls[i].live)
        {
            continue;
        }
        Vec offset = balls[i].pos - origin;
        if(i == 0 && length(offset) < diameter + unit / 4)
        {
            continue;
        }
        int hit = entry(offset, heading, diameter);
        if(hit >= 0 && hit < distance)
        {
            distance = hit;
            ray.ball = i;
            ray.pocket = -1;
            ray.rail = false;
        }
    }
    ray.end = origin + travel(heading, distance);
    return ray;
}

namespace
{
Vec cushion_rebound(Vec vector, Vec contact)
{
    if(contact.x <= left + 2 || contact.x >= right - 2)
    {
        vector.x = -vector.x * 218 / 256;
    }
    if(contact.y <= top + 2 || contact.y >= bottom - 2)
    {
        vector.y = -vector.y * 218 / 256;
    }
    return vector;
}

int adjusted_speed(Vec heading, int speed)
{
    int major = std::max(abs_value(heading.x), abs_value(heading.y));
    int minor = std::min(abs_value(heading.x), abs_value(heading.y));
    return speed * (major + minor * 3 / 8) / direction_unit;
}

int roll_range(Vec heading, int speed)
{
    int effective = adjusted_speed(heading, speed);
    if(effective <= 5)
    {
        return 0;
    }
    // Integrate the same two drag rates used by the rolling simulation.
    int slow = std::min(effective, unit - 1);
    int quick = effective > slow ? (effective * effective - slow * slow) / 4 : 0;
    int distance = (slow * slow - 25) / 2 + quick + effective / 4;
    return distance * speed / effective;
}

int speed_at(Vec heading, int start, int distance)
{
    int remaining = roll_range(heading, start) - distance;
    if(remaining <= 0)
    {
        return 0;
    }
    int low = 0;
    int high = start;
    while(high - low > 1)
    {
        int next = (low + high) / 2;
        if(roll_range(heading, next) < remaining)
        {
            low = next;
        }
        else
        {
            high = next;
        }
    }
    return high;
}
}

Preview trace_aim(const Table& table, Vec heading)
{
    Preview result;
    result.first = table.trace(table.balls[0].pos, heading, 0);
    Ray contact = result.first;
    if(contact.rail)
    {
        heading = normalized(cushion_rebound(heading, contact.end));
        result.bounced = true;
        result.bank = table.trace(contact.end, heading, 0);
        contact = result.bank;
    }
    if(contact.ball >= 0)
    {
        result.hit = true;
        Vec target = table.balls[contact.ball].pos;
        Vec object_heading = normalized(target - contact.end);
        if(dot(heading, object_heading) > 0)
        {
            result.object_moves = true;
            result.object = table.trace(target, object_heading, contact.ball);
        }
    }
    return result;
}

Preview preview_shot(const Table& table, Vec heading, int speed)
{
    Preview result;
    Vec cue = table.balls[0].pos;
    int range = roll_range(heading, speed);
    result.first = table.trace(cue, heading, 0, range);
    Ray contact = result.first;
    speed = speed_at(heading, speed, length(contact.end - cue));

    if(contact.rail && speed)
    {
        Vec rebound = cushion_rebound(travel(heading, speed * unit), contact.end);
        speed = length(rebound) / unit;
        heading = normalized(rebound);
        result.bounced = true;
        result.bank = table.trace(contact.end, heading, 0, roll_range(heading, speed));
        speed = speed_at(heading, speed, length(result.bank.end - contact.end));
        contact = result.bank;
    }

    if(contact.ball >= 0)
    {
        result.hit = true;
        Vec target = table.balls[contact.ball].pos;
        Vec object_heading = normalized(target - contact.end);
        int transfer = std::max(0, dot(heading, object_heading));
        int object_speed = speed * transfer * 248 / (direction_unit * 256);
        result.object = table.trace(target, object_heading, contact.ball, roll_range(object_heading, object_speed));
        result.object_moves = object_speed > 5;
    }
    return result;
}

void EightBall::start(int breaker)
{
    *this = {};
    table.rack();
    player = breaker;
}

int EightBall::remaining(Group group) const
{
    if(group == Group::open)
    {
        return 0;
    }
    int total = 0;
    for(int i = 1; i < 16; ++i)
    {
        total += table.balls[i].live && group_of(i) == group;
    }
    return total;
}

bool EightBall::legal_target(int ball) const
{
    if(ball <= 0)
    {
        return false;
    }
    Group group = groups[player];
    if(group == Group::open)
    {
        return ball != 8 || (! remaining(Group::solids) && ! remaining(Group::stripes));
    }
    return remaining(group) ? group_of(ball) == group : ball == 8;
}

bool EightBall::requires_call() const
{
    return ! breaking && table.balls[8].live && legal_target(8);
}

void EightBall::finish_shot()
{
    const Shot& shot = table.shot;
    Group group = groups[player];
    int own_mask = group == Group::open ? 0 : group_mask(group);
    bool on_eight = group == Group::open ?
        ! remaining(Group::solids) && ! remaining(Group::stripes) && !(shot.pocketed & object_mask) :
        ! remaining(group) && !(shot.pocketed & own_mask);
    bool foul = true;
    bool scratched = shot.pocketed & 1;
    if(scratched)
    {
        notice = Notice::scratch;
    }
    else if(shot.first < 0)
    {
        notice = Notice::no_contact;
    }
    else if((on_eight && shot.first != 8) || (! on_eight &&
            (shot.first == 8 || (own_mask && !(own_mask & (1u << shot.first))))))
    {
        notice = Notice::wrong_ball;
    }
    else if(! breaking && ! shot.pocketed && ! shot.rail_after_contact)
    {
        notice = Notice::no_rail;
    }
    else
    {
        foul = false;
    }

    bool sunk_eight = shot.pocketed & (1 << 8);
    if(sunk_eight && ! breaking)
    {
        bool made_call = shot.called_pocket >= 0 && shot.called_pocket == shot.eight_pocket;
        outcome = ! on_eight ? Outcome::early_eight : foul ? Outcome::foul_eight :
                  made_call ? Outcome::cleared : Outcome::wrong_pocket;
        winner = outcome == Outcome::cleared ? player : 1 - player;
        return;
    }
    if(sunk_eight)
    {
        table.spot_eight();
    }

    bool potted = shot.pocketed & object_mask;
    if(! breaking && ! foul && potted && group == Group::open)
    {
        group = group_of(shot.first_pot);
        if(group != Group::open)
        {
            groups[player] = group;
            groups[1 - player] = group == Group::solids ? Group::stripes : Group::solids;
            own_mask = group_mask(group);
        }
    }

    bool continues = ! foul && (own_mask ? (shot.pocketed & own_mask) != 0 : potted);
    if(! continues)
    {
        player = 1 - player;
    }
    ball_in_hand = foul;
    if(foul)
    {
        table.balls[0].live = false;
    }
    else
    {
        notice = sunk_eight ? Notice::spotted : continues ? Notice::pot :
                 groups[player] == Group::open ? Notice::open_table : Notice::turn;
    }
    breaking = false;
}

int Profile::rival_index() const
{
    int index = 0;
    while(index + 1 < rival_count && best >= rivals[index + 1].unlock)
    {
        ++index;
    }
    return index;
}

int Profile::record(bool won, int opponent_rating)
{
    // Elo win probabilities in thousandths at 100-point intervals.
    constexpr int expected[] = { 10, 17, 31, 53, 91, 151, 240, 360, 500,
                                 640, 760, 849, 909, 947, 969, 983, 990 };
    int difference = std::clamp(rating - opponent_rating, -800, 799) + 800;
    int index = difference / 100;
    int fraction = difference % 100;
    int chance = expected[index] + (expected[index + 1] - expected[index]) * fraction / 100;
    int change = won ? std::max(1, (48 * (1000 - chance) + 500) / 1000) :
                       -std::max(1, (48 * chance + 500) / 1000);
    int previous = rating;
    rating = std::clamp(rating + change, 100, 2400);
    best = std::max(rating, best);
    if(won)
    {
        wins = std::min(9999, wins + 1);
    }
    else
    {
        losses = std::min(9999, losses + 1);
    }
    return rating - previous;
}

namespace
{
constexpr int no_candidate = 1000000;
constexpr Vec candidate_positions[] = { px(164, 84), px(120, 70), px(120, 101),
                                        px(52, 62), px(53, 106), px(182, 62),
                                        px(184, 107), px(88, 84) };
int pot_candidate(const Table& table, int id, int pocket, Vec cue, int min_cut, Plan& plan)
{
    Vec target = table.balls[id].pos;
    Vec object_path = pockets[pocket] - target;
    Vec object_heading = normalized(object_path);
    if(table.trace(target, object_heading, id).pocket != pocket)
    {
        return no_candidate;
    }
    Vec ghost = target - travel(object_heading, diameter - unit / 16);
    Vec cue_path = ghost - cue;
    Vec cue_heading = normalized(cue_path);
    int cut = dot(cue_heading, object_heading);
    if(cut < min_cut || ghost.x < left || ghost.x > right || ghost.y < top || ghost.y > bottom ||
       table.trace(cue, cue_heading, 0).ball != id)
    {
        return no_candidate;
    }
    int approach = length(cue_path) / unit;
    int run = length(object_path) / unit;
    int cost = approach + run * direction_unit / cut;
    int speed = std::clamp(142 + cost * 12 / 10, 190, max_speed);
    plan = { direction_angle(cue_path), speed, cue, pocket };
    return cost + (direction_unit - cut) / 8;
}

}

void PocketSuggestions::start(int pocket)
{
    count = 0;
    next_ball = 1;
    called = pocket;
}

bool PocketSuggestions::advance(const EightBall& game)
{
    if(next_ball == 16)
    {
        return true;
    }
    int id = next_ball++;
    const Table& table = game.table;
    if(game.breaking || ! game.legal_target(id) || ! table.balls[id].live)
    {
        return next_ball == 16;
    }
    for(int pocket = 0; pocket < 6; ++pocket)
    {
        if(called >= 0 && pocket != called)
        {
            continue;
        }
        Plan choice;
        int score = pot_candidate(table, id, pocket, table.balls[0].pos, 410, choice);
        if(score == no_candidate || (count == capacity && score >= scores[count - 1]))
        {
            continue;
        }
        // Check the quantized aim and power the player will actually use.
        Preview prediction = preview_shot(table, direction(choice.angle), choice.speed);
        if(prediction.first.ball != id || ! prediction.object_moves || prediction.object.pocket != pocket)
        {
            continue;
        }
        int insert = std::min(count, capacity - 1);
        while(insert > 0 && scores[insert - 1] > score)
        {
            choices[insert] = choices[insert - 1];
            scores[insert] = scores[insert - 1];
            --insert;
        }
        choices[insert] = choice;
        scores[insert] = score;
        count = std::min(count + 1, capacity);
    }
    return next_ball == 16;
}

void ShotPlanner::start(const EightBall& game, int difficulty)
{
    *this = {};
    level = difficulty;
    starting_cue = game.ball_in_hand ? game.table.free_spot() : game.table.balls[0].pos;
    cue = starting_cue;
    best = { 0, max_speed, starting_cue };
    best_score = no_candidate;
    nearest_distance = no_candidate;
    search = game.breaking ? Search::break_shot : Search::pots;
}

bool ShotPlanner::advance(const EightBall& game, uint32_t& seed)
{
    if(! seed)
    {
        seed = 0xc19472abu;
    }

    switch(search)
    {
        case Search::break_shot:
            best.angle = (random_below(seed, 5) - 2) & (angle_count - 1);
            search = Search::done;
            return true;
        case Search::pots:
            find_pots(game, seed);
            if(search == Search::pots || best_score == no_candidate)
            {
                return false;
            }
            break;
        case Search::direct:
            find_direct(game);
            if(search == Search::direct || best_score == no_candidate)
            {
                return false;
            }
            break;
        case Search::banks:
            find_banks(game);
            if(bank_angle < angle_count)
            {
                return false;
            }
            break;
        default:
            return true;
    }
    finish(game, seed);
    return true;
}

void ShotPlanner::find_pots(const EightBall& game, uint32_t& seed)
{
    const Table& table = game.table;
    // Split the search into small batches to leave time for a GBA display refresh.
    int limit = std::min(ball_index + 3, 16);
    for(; ball_index < limit; ++ball_index)
    {
        int id = ball_index;
        if(! table.balls[id].live || ! game.legal_target(id))
        {
            continue;
        }
        for(int pocket = 0; pocket < 6; ++pocket)
        {
            Plan plan;
            int score = pot_candidate(table, id, pocket, cue, 350 + (4 - level) * 45, plan);
            if(score == no_candidate)
            {
                continue;
            }
            if(level < 2)
            {
                score += random_below(seed, 40 - level * 10);
            }
            if(score < best_score)
            {
                best = plan;
                best_score = score;
            }
        }
    }

    if(ball_index == 16)
    {
        while(game.ball_in_hand && position_index < level * 2)
        {
            Vec alternative = candidate_positions[position_index++];
            if(table.can_place(alternative))
            {
                cue = alternative;
                ball_index = 1;
                return;
            }
        }
        search = Search::direct;
        cue = starting_cue;
        ball_index = 1;
    }
}

void ShotPlanner::find_direct(const EightBall& game)
{
    const Table& table = game.table;
    int limit = std::min(ball_index + 6, 16);
    for(; ball_index < limit; ++ball_index)
    {
        int id = ball_index;
        if(! table.balls[id].live || ! game.legal_target(id))
        {
            continue;
        }
        Vec path = table.balls[id].pos - cue;
        int distance = length(path);
        int angle = direction_angle(path);
        if(distance < nearest_distance)
        {
            nearest_distance = distance;
            nearest_angle = angle;
        }
        if(distance < best_score && table.trace(cue, direction(angle), 0).ball == id)
        {
            best = { angle, std::min(max_speed, 255 + distance / (2 * unit)), cue };
            best_score = distance;
        }
    }
    if(ball_index == 16)
    {
        search = Search::banks;
    }
}

void ShotPlanner::find_banks(const EightBall& game)
{
    constexpr int stride = 16;
    const Table& table = game.table;
    int limit = std::min(bank_angle + 8 * stride, angle_count);
    for(; bank_angle < limit; bank_angle += stride)
    {
        Vec heading = direction(bank_angle);
        Ray first = table.trace(cue, heading, 0);
        if(! first.rail)
        {
            continue;
        }
        if(first.end.x <= left + 2 || first.end.x >= right - 2)
        {
            heading.x = -heading.x;
        }
        if(first.end.y <= top + 2 || first.end.y >= bottom - 2)
        {
            heading.y = -heading.y;
        }
        Ray rebound = table.trace(first.end + travel(heading, unit), heading, 0);
        if(game.legal_target(rebound.ball))
        {
            int distance = length(first.end - cue) + length(rebound.end - first.end);
            if(distance < best_score)
            {
                best = { bank_angle, max_speed, cue };
                best_score = distance;
            }
        }
    }
    if(bank_angle == angle_count && best_score == no_candidate)
    {
        best = { nearest_angle, max_speed, cue };
    }
}

void ShotPlanner::finish(const EightBall& game, uint32_t& seed)
{
    if(game.requires_call())
    {
        if(best.pocket < 0)
        {
            best.pocket = closest_pocket(game.table.balls[8].pos);
        }
    }
    else
    {
        best.pocket = -1;
    }
    int error = rivals[level].error;
    int jitter = random_below(seed, error + 1) - random_below(seed, error + 1);
    best.angle = (best.angle + jitter) & (angle_count - 1);
    best.speed = std::clamp(best.speed + random_below(seed, 5) * error - 2 * error,
                            min_speed, max_speed);
    search = Search::done;
}

}
