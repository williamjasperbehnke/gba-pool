#include "pool.h"

#include <cassert>
#include <cstdio>
#include <initializer_list>

using namespace pool;

namespace
{
void settle(Table& table)
{
    for(int frame = 0; frame < 180; ++frame)
    {
        bool moving = table.step();
        for(int a = 0; a < 15; ++a)
        {
            for(int b = a + 1; b < 16; ++b)
            {
                if(table.balls[a].live && table.balls[b].live)
                {
                    assert(length(table.balls[b].pos - table.balls[a].pos) >= diameter - unit / 8);
                }
            }
        }
        if(! moving)
        {
            return;
        }
    }
    assert(false && "shot never settled");
}

void empty(Table& table)
{
    for(Ball& ball : table.balls)
    {
        ball = { {}, {}, false };
    }
    table.shot = {};
}

void aiming_and_physics()
{
    Table table;
    table.rack();
    for(int a = 1; a < 16; ++a)
    {
        for(int b = a + 1; b < 16; ++b)
        {
            assert(length(table.balls[b].pos - table.balls[a].pos) >= 9 * unit);
        }
    }
    Vec cue = table.balls[0].pos;
    Ray guide = table.trace(cue, direction(0), 0);
    assert(guide.ball == 1);
    assert(guide.end.x == table.balls[1].pos.x - diameter);
    table.strike(direction(0), max_speed);
    settle(table);
    assert(table.shot.first == guide.ball);
    assert(length(table.balls[1].pos - px(154, 84)) > unit);

    empty(table);
    table.place_cue(px(120, 85));
    table.balls[1] = { px(120, 65), {}, true };
    assert(! table.can_place(px(120, 67)));
    assert(! table.can_place(px(120, 46)));
    assert(table.can_place(px(110, 67)));
    guide = table.trace(table.balls[0].pos, direction(768), 0);
    assert(guide.ball == 1);
    Ray object = table.trace(table.balls[1].pos, direction(768), 1);
    assert(object.pocket == 1);
    table.strike(direction(768), 270);
    settle(table);
    assert(table.shot.first == 1);
    assert(table.shot.pocketed == 1 << 1);
    assert(! table.balls[1].live && table.balls[0].live);

    empty(table);
    table.place_cue(px(120, 75));
    assert(table.trace(table.balls[0].pos, direction(768), 0).pocket == 1);
    table.strike(direction(768), max_speed);
    settle(table);
    assert(table.shot.pocketed == 1);
}

void preview_and_suggestions()
{
    EightBall game;
    game.start(0);
    game.breaking = false;
    empty(game.table);
    game.table.place_cue(px(120, 105));
    game.table.balls[1] = { px(120, 65), {}, true };
    Vec heading = direction(768);
    Preview soft = preview_shot(game.table, heading, min_speed);
    assert(! soft.hit && ! soft.bounced && soft.first.end.y > game.table.balls[1].pos.y);
    Preview guide = trace_aim(game.table, heading);
    assert(guide.hit && guide.first.ball == 1 && guide.object_moves && guide.object.pocket == 1);
    Preview hard = preview_shot(game.table, heading, max_speed);
    assert(hard.hit && hard.first.ball == 1 && hard.object_moves && hard.object.pocket == 1);
    PocketSuggestions suggestions;
    suggestions.start();
    while(! suggestions.advance(game))
    {
    }
    assert(suggestions.size() > 0);
    const Plan& choice = suggestions[0];
    game.table.strike(direction(choice.angle), choice.speed);
    settle(game.table);
    assert(game.table.shot.first == 1 && game.table.shot.pocketed == (1 << 1));

    empty(game.table);
    game.table.place_cue(px(120, 84));
    Preview stop = preview_shot(game.table, direction(0), min_speed);
    game.table.strike(direction(0), min_speed);
    settle(game.table);
    assert(length(game.table.balls[0].pos - stop.first.end) < unit);

    game.table.place_cue(px(120, 84));
    assert(preview_shot(game.table, direction(768), max_speed).first.pocket == 1);
    Preview bank = preview_shot(game.table, direction(832), max_speed);
    assert(bank.first.rail && bank.bounced && bank.bank.end.y > bank.first.end.y);
    Preview bank_guide = trace_aim(game.table, direction(832));
    assert(bank_guide.bounced && bank_guide.first.end.y == top && bank_guide.bank.end.y == bottom);
}

void rules()
{
    EightBall game;
    game.start(0);
    game.table.balls[1].live = false;
    game.table.balls[8].live = false;
    game.table.shot = { uint16_t((1 << 1) | (1 << 8)), 1, 1, false };
    game.finish_shot();
    assert(game.table.balls[8].live && game.notice == Notice::spotted);
    assert(game.player == 0 && ! game.breaking && game.groups[0] == Group::open);

    game.table.balls[10].live = false;
    game.table.shot = { 1 << 10, 10, 10, false };
    game.finish_shot();
    assert(game.groups[0] == Group::stripes && game.groups[1] == Group::solids);
    assert(game.player == 0 && game.legal_target(9) && ! game.legal_target(2));

    game.table.shot = { 0, 9, -1, false };
    game.finish_shot();
    assert(game.player == 1 && game.ball_in_hand && game.notice == Notice::no_rail);
    assert(! game.table.balls[0].live && game.table.can_place(game.table.free_spot()));

    game.table.shot = { 0, 9, -1, true };
    game.finish_shot();
    assert(game.player == 0 && game.ball_in_hand && game.notice == Notice::wrong_ball);

    game.table.shot = { 1, 9, -1, false };
    game.finish_shot();
    assert(game.player == 1 && game.ball_in_hand && game.notice == Notice::scratch);

    game.table.balls[8].live = false;
    game.table.shot = { 1 << 8, 8, 8, false };
    game.finish_shot();
    assert(game.outcome == Outcome::early_eight && game.winner == 0);

    game.start(1);
    game.player = 1;
    game.breaking = false;
    game.groups[1] = Group::solids;
    for(int i = 1; i <= 8; ++i)
    {
        game.table.balls[i].live = false;
    }
    assert(game.legal_target(8));
    const EightBall cleared_table = game;
    game.table.shot = { uint16_t((1 << 7) | (1 << 8)), 7, 7, false };
    game.finish_shot();
    assert(game.outcome == Outcome::early_eight && game.winner == 0);

    game = cleared_table;
    game.table.shot = { uint16_t(1 | (1 << 8)), 8, 8, false, 2, 2 };
    game.finish_shot();
    assert(game.outcome == Outcome::foul_eight && game.winner == 0);

    for(int called : { -1, 3 })
    {
        game = cleared_table;
        game.table.shot = { 1 << 8, 8, 8, false, 2, int8_t(called) };
        game.finish_shot();
        assert(game.outcome == Outcome::wrong_pocket && game.winner == 0);
    }

    game = cleared_table;
    game.table.shot = { 1 << 8, 8, 8, false, 2, 2 };
    game.finish_shot();
    assert(game.outcome == Outcome::cleared && game.winner == 1);

    game.start(0);
    game.breaking = false;
    empty(game.table);
    game.table.place_cue(px(63, 84));
    game.table.place_cue(px(120, 85));
    game.table.balls[8] = { px(120, 65), {}, true };
    assert(game.requires_call());
    PocketSuggestions suggestions;
    suggestions.start(4);
    while(! suggestions.advance(game))
    {
    }
    assert(suggestions.size() == 0);
    game.table.strike(direction(768), 270, 1);
    settle(game.table);
    assert(game.table.shot.pocketed == (1 << 8) && game.table.shot.eight_pocket == 1);
    game.finish_shot();
    assert(game.outcome == Outcome::cleared && game.winner == 0);
}

void rating()
{
    Profile profile;
    assert(profile.rival_index() == 0);
    assert(profile.record(true, 850) == 24);
    profile.record(true, 850);
    profile.record(true, 850);
    assert(profile.rival_index() == 1 && profile.wins == 3);
    int highest = profile.best;
    profile.record(false, 950);
    profile.record(false, 950);
    assert(profile.rating < rivals[1].unlock);
    assert(profile.rival_index() == 1 && profile.best == highest && profile.losses == 2);
}

void bot_matches()
{
    int pot_rate[rival_count] = {};
    for(int difficulty = 0; difficulty < rival_count; ++difficulty)
    {
        int shots = 0;
        int pots = 0;
        for(uint32_t match = 1; match <= 30; ++match)
        {
            uint32_t seed = match * 1713231;
            EightBall game;
            game.start(int(match & 1));
            for(int turn = 0; turn < 170 && game.winner < 0; ++turn)
            {
                ShotPlanner planner;
                planner.start(game, difficulty);
                int work_frames = 0;
                while(! planner.advance(game, seed))
                {
                    assert(++work_frames <= 60);
                }
                const Plan& plan = planner.result();
                if(game.ball_in_hand)
                {
                    assert(game.table.can_place(plan.placement));
                    game.table.place_cue(plan.placement);
                    game.ball_in_hand = false;
                }
                assert(! game.requires_call() || (plan.pocket >= 0 && plan.pocket < 6));
                game.table.strike(direction(plan.angle), plan.speed, plan.pocket);
                settle(game.table);
                ++shots;
                if(game.table.shot.pocketed & 0xfefe)
                {
                    ++pots;
                }
                game.finish_shot();
            }
            assert(game.winner >= 0 && game.outcome != Outcome::playing);
        }
        pot_rate[difficulty] = pots * 1000 / shots;
        std::printf("%s completed 30 matches. Pot rate %d.%d%%.\n", rivals[difficulty].name,
                    pot_rate[difficulty] / 10, pot_rate[difficulty] % 10);
    }
    assert(pot_rate[rival_count - 1] > pot_rate[0] + 100);
}
}

int main()
{
    aiming_and_physics();
    preview_and_suggestions();
    rules();
    rating();
    bot_matches();
}
