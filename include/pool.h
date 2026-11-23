#ifndef POOL_H
#define POOL_H

#include <cstdint>

namespace pool
{
constexpr int unit = 256;
constexpr int direction_unit = 1024;
constexpr int diameter = 8 * unit - unit / 4;
constexpr int left = 20 * unit;
constexpr int right = 220 * unit;
constexpr int top = 45 * unit;
constexpr int bottom = 123 * unit;
constexpr int min_speed = 120;
constexpr int max_speed = 475;
constexpr int angle_count = 1024;
constexpr int max_spin = 4;

struct CueSpin
{
    int side = 0;
    int vertical = 0;
};

constexpr int spin_steps = 2 * max_spin + 1;
constexpr uint8_t encode_spin(CueSpin spin)
{
    return uint8_t((spin.side + max_spin) * spin_steps + spin.vertical + max_spin);
}

constexpr CueSpin decode_spin(unsigned value)
{
    return { int(value / spin_steps) - max_spin, int(value % spin_steps) - max_spin };
}

struct Vec
{
    int x = 0;
    int y = 0;

    constexpr Vec operator+(Vec b) const { return { x + b.x, y + b.y }; }
    constexpr Vec operator-(Vec b) const { return { x - b.x, y - b.y }; }
    Vec& operator+=(Vec b) { x += b.x; y += b.y; return *this; }
};

constexpr Vec px(int x, int y) { return { x * unit, y * unit }; }
inline constexpr Vec pockets[] = { px(17, 42), px(120, 42), px(223, 42),
                                   px(17, 126), px(120, 126), px(223, 126) };

int closest_pocket(Vec point);
Vec direction(int angle);
int direction_angle(Vec value);
Vec normalized(Vec value);
Vec travel(Vec heading, int distance);
int length(Vec value);

struct Ball
{
    Vec pos;
    Vec velocity;
    bool live = true;
};

struct Shot
{
    uint16_t pocketed = 0;
    int first = -1;
    int first_pot = -1;
    bool rail_after_contact = false;
    int8_t eight_pocket = -1;
    int8_t called_pocket = -1;
    uint8_t cue_spin = encode_spin({});
};

struct Ray
{
    Vec end;
    int ball = -1;
    int pocket = -1;
    bool rail = false;
};

class Table
{
public:
    Ball balls[16];
    Shot shot;
    int impact = 0;

    void rack();
    void strike(Vec heading, int speed, int called_pocket = -1, CueSpin spin = {});
    bool step();
    bool can_place(Vec pos, int ignore = 0) const;
    void place_cue(Vec pos);
    Vec free_spot(int ignore = 0) const;
    void spot_eight();

    Ray trace(Vec origin, Vec heading, int ignore, int max_distance = 240 * unit) const;

private:
    void cushion_or_pocket(int id);
    void collide(int a, int b);
    void resolve_positions(bool moving);
};

struct Preview
{
    Ray first;
    Ray bank;
    Ray object;
    bool bounced = false;
    bool hit = false;
    bool object_moves = false;
};

Preview trace_aim(const Table& table, Vec heading);
Preview preview_shot(const Table& table, Vec heading, int speed);

enum class Group { open, solids, stripes };
enum class Notice { break_shot, turn, open_table, pot, scratch, no_contact, wrong_ball,
                    no_rail, spotted };
enum class Outcome { playing, cleared, early_eight, foul_eight, wrong_pocket };

class EightBall
{
public:
    Table table;
    Group groups[2] = { Group::open, Group::open };
    int player = 0;
    int winner = -1;
    bool breaking = true;
    bool ball_in_hand = false;
    Notice notice = Notice::break_shot;
    Outcome outcome = Outcome::playing;

    void start(int breaker);
    void finish_shot();
    bool legal_target(int ball) const;
    bool requires_call() const;
    int remaining(Group group) const;
};

struct Rival
{
    const char* name;
    const char* rank;
    int unlock;
    int rating;
    int error;
};

inline constexpr Rival rivals[] = {
    { "LOU", "ROOKIE", 850, 850, 15 },
    { "IZZY", "REGULAR", 900, 950, 9 },
    { "JULES", "CLUB PRO", 975, 1050, 5 },
    { "NIKO", "SHARK", 1060, 1150, 3 },
    { "VERA", "ACE", 1160, 1300, 1 }
};
inline constexpr int rival_count = sizeof(rivals) / sizeof(rivals[0]);

struct Profile
{
    int rating = 850;
    int best = 850;
    int wins = 0;
    int losses = 0;

    int rival_index() const;
    int record(bool won, int opponent_rating);
};

struct Plan
{
    int angle = 0;
    int speed = max_speed;
    Vec placement;
    int pocket = -1;
};

class PocketSuggestions
{
public:
    void start(int pocket = -1);
    bool advance(const EightBall& game);
    int size() const { return count; }
    const Plan& operator[](int index) const { return choices[index]; }

private:
    static constexpr int capacity = 6;
    Plan choices[capacity];
    int scores[capacity];
    int count = 0;
    int next_ball = 16;
    int called = -1;
};

class ShotPlanner
{
public:
    void start(const EightBall& game, int difficulty);
    bool advance(const EightBall& game, uint32_t& seed);
    const Plan& result() const { return best; }

private:
    enum class Search { break_shot, pots, direct, banks, done };
    Search search = Search::done;
    int level = 0;
    int position_index = 0;
    int ball_index = 1;
    int bank_angle = 0;
    int nearest_distance = 0;
    int nearest_angle = 0;
    int best_score = 0;
    Vec starting_cue;
    Vec cue;
    Plan best;

    void find_pots(const EightBall& game, uint32_t& seed);
    void find_direct(const EightBall& game);
    void find_banks(const EightBall& game);
    void finish(const EightBall& game, uint32_t& seed);
};

}

#endif
