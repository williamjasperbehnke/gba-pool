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
    uint8_t spin_strength = 100;
    int first = -1;
    int first_pot = -1;
    bool rail_after_contact = false;
    int8_t eight_pocket = -1;
    int8_t called_pocket = -1;
    uint8_t cue_spin = encode_spin({});

    constexpr Shot() = default;
    constexpr Shot(uint16_t potted, int first_hit, int first_ball_potted, bool reached_rail,
                   int8_t eight = -1, int8_t called = -1, uint8_t spin = encode_spin({})) :
        pocketed(potted), first(first_hit), first_pot(first_ball_potted), rail_after_contact(reached_rail),
        eight_pocket(eight), called_pocket(called), cue_spin(spin)
    {
    }
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
    void strike(Vec heading, int speed, int called_pocket = -1, CueSpin spin = {}, int spin_strength = 100);
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

class PotCombo
{
public:
    void arm() { state = State::waiting; }
    bool active() const { return state != State::inactive; }
    bool ready() const { return state == State::ready; }

    bool resolve(bool scored, bool foul)
    {
        if(! active())
        {
            return false;
        }
        if(scored)
        {
            state = State::ready;
            return false;
        }
        bool saved_turn = ! foul && ready();
        state = State::inactive;
        return saved_turn;
    }

private:
    enum class State { inactive, waiting, ready };
    State state = State::inactive;
};

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
    bool finish_shot(PotCombo* combo = nullptr);
    bool legal_target(int ball) const;
    bool requires_call() const;
    bool potted_group(Group group) const;
    int remaining(Group group) const;
};

struct Rival
{
    const char* name;
    const char* short_name;
    const char* rank;
    int rating;
    int error;
};

inline constexpr Rival rivals[] = {
    { "PIP THE OTTER", "PIP", "ROOKIE", 850, 18 },
    { "NORI THE PENGUIN", "NORI", "REGULAR", 925, 12 },
    { "EMBER THE FOX", "EMBER", "CLUB PRO", 1010, 7 },
    { "INK THE OCTOPUS", "INK", "EXPERT", 1100, 1 },
    { "FINN THE SHARK", "FINN", "ACE", 1210, 0 }
};
inline constexpr int rival_count = sizeof(rivals) / sizeof(rivals[0]);

struct Profile
{
    int rating = 850;
    int best = 850;
    int wins = 0;
    int losses = 0;

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
    void start(const EightBall& game, int difficulty, int speed_limit = max_speed);
    bool advance(const EightBall& game, uint32_t& seed);
    const Plan& result() const { return best; }
    int planned_angle() const { return precise_angle; }
    bool has_verified_pot() const { return verified_pot; }

private:
    enum class Search { break_shot, pots, object_banks, verify, direct, banks, done };
    Search search = Search::done;
    int level = 0;
    int power_limit = max_speed;
    int position_index = 0;
    int ball_index = 1;
    int bank_angle = 0;
    int nearest_distance = 0;
    int nearest_angle = 0;
    int best_score = 0;
    int precise_angle = 0;
    Vec starting_cue;
    Vec cue;
    Plan best;
    static constexpr int trial_capacity = 3;
    Plan trials[trial_capacity];
    int trial_scores[trial_capacity];
    int trial_count = 0;
    int trial_index = 0;
    int trial_frames = 0;
    bool searched_object_banks = false;
    bool verified_pot = false;
    Table trial_table;


    void find_pots(const EightBall& game, uint32_t& seed);
    void add_trial(const Plan& plan, int score);
    void verify_pots(const EightBall& game);
    void find_object_banks(const EightBall& game);
    void start_fallback();
    void find_direct(const EightBall& game);
    void find_banks(const EightBall& game);
    void finish(const EightBall& game, uint32_t& seed);
};

}

#endif
