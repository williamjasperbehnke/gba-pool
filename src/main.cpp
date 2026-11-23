#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "bn_array.h"
#include "bn_core.h"
#include "bn_format.h"
#include "bn_keypad.h"
#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sound_items.h"
#include "bn_sprite_font.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_sram.h"
#include "bn_string.h"
#include "bn_vector.h"

#include "bn_regular_bg_items_lobby.h"
#include "bn_regular_bg_items_menu.h"
#include "bn_regular_bg_items_panel.h"
#include "bn_regular_bg_items_room.h"
#include "bn_sprite_items_ball_0.h"
#include "bn_sprite_items_ball_1.h"
#include "bn_sprite_items_ball_2.h"
#include "bn_sprite_items_ball_3.h"
#include "bn_sprite_items_ball_4.h"
#include "bn_sprite_items_ball_5.h"
#include "bn_sprite_items_ball_6.h"
#include "bn_sprite_items_ball_7.h"
#include "bn_sprite_items_ball_8.h"
#include "bn_sprite_items_ball_9.h"
#include "bn_sprite_items_ball_10.h"
#include "bn_sprite_items_ball_11.h"
#include "bn_sprite_items_ball_12.h"
#include "bn_sprite_items_ball_13.h"
#include "bn_sprite_items_ball_14.h"
#include "bn_sprite_items_ball_15.h"
#include "bn_sprite_items_avatars.h"
#include "bn_sprite_items_cue.h"
#include "bn_sprite_items_hud_font.h"
#include "bn_sprite_items_marks.h"
#include "bn_sprite_items_meter.h"
#include "bn_sprite_items_placement.h"
#include "bn_sprite_items_pocket_marker.h"

#include "font_widths.h"
#include "pool.h"
#include "pool_cable.h"
#include "shot_line.h"

namespace
{
using namespace pool;

constexpr const bn::sprite_item* ball_items[] = {
    &bn::sprite_items::ball_0, &bn::sprite_items::ball_1, &bn::sprite_items::ball_2,
    &bn::sprite_items::ball_3, &bn::sprite_items::ball_4, &bn::sprite_items::ball_5,
    &bn::sprite_items::ball_6, &bn::sprite_items::ball_7, &bn::sprite_items::ball_8,
    &bn::sprite_items::ball_9, &bn::sprite_items::ball_10, &bn::sprite_items::ball_11,
    &bn::sprite_items::ball_12, &bn::sprite_items::ball_13, &bn::sprite_items::ball_14,
    &bn::sprite_items::ball_15
};

constexpr bn::sprite_font hud_font(bn::sprite_items::hud_font, {}, font_widths, 2);
constexpr auto text_colors(bn::color ink)
{
    bn::array<bn::color, 16> colors{};
    colors[6] = ink;
    return colors;
}

constexpr auto mint_colors = text_colors(bn::color(20, 29, 25));
constexpr auto gold_colors = text_colors(bn::color(31, 26, 14));
constexpr auto muted_colors = text_colors(bn::color(18, 23, 23));
constexpr bn::sprite_palette_item mint_text(mint_colors, bn::bpp_mode::BPP_4);
constexpr bn::sprite_palette_item gold_text(gold_colors, bn::bpp_mode::BPP_4);
constexpr bn::sprite_palette_item muted_text(muted_colors, bn::bpp_mode::BPP_4);
constexpr auto& ivory_text = bn::sprite_items::hud_font.palette_item();
using Align = bn::sprite_text_generator::alignment_type;

constexpr int meter_sections = 8;
constexpr int cue_length = 48;
constexpr int break_power = 340;
constexpr int rating_magic = 0x38425231;

enum Mark { circle, foul_circle, turn_diamond };
constexpr const char* pocket_names[] = {
    "TOP LEFT", "TOP MIDDLE", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM MIDDLE", "BOTTOM RIGHT"
};
enum class Screen { lobby, match, help, pause, concede, result, link_wait, link_lost, link_leaving };
enum class Phase { aim, charge, call, place, rolling, cpu_wait, cpu_aim, remote, link_send };

struct SaveSlot
{
    uint32_t magic = rating_magic;
    uint32_t sequence = 0;
    Profile profile;
    uint32_t checksum = 0;

    uint32_t calculate() const
    {
        uint32_t hash = 0x811c9dc5u;
        for(uint32_t value : { magic, sequence, uint32_t(profile.rating), uint32_t(profile.best),
                               uint32_t(profile.wins), uint32_t(profile.losses) })
        {
            hash = (hash ^ value) * 0x01000193u;
        }
        return hash;
    }

    bool valid() const
    {
        return magic == rating_magic && checksum == calculate() &&
               profile.rating >= 100 && profile.rating <= profile.best && profile.best <= 2400 &&
               profile.wins >= 0 && profile.wins <= 9999 && profile.losses >= 0 && profile.losses <= 9999;
    }
};

class Records
{
public:
    Records()
    {
        SaveSlot slots[2];
        for(int i = 0; i < 2; ++i)
        {
            bn::sram::read_offset(slots[i], i * int(sizeof(SaveSlot)));
            if(slots[i].valid() && (active < 0 || slots[i].sequence > record.sequence))
            {
                active = i;
                record = slots[i];
            }
        }
    }

    const Profile& profile() const { return record.profile; }

    int add_result(bool won, int opponent_rating)
    {
        int change = record.profile.record(won, opponent_rating);
        ++record.sequence;
        record.checksum = record.calculate();
        active = active == 0 ? 1 : 0;
        bn::sram::write_offset(record, active * int(sizeof(SaveSlot)));
        return change;
    }

private:
    SaveSlot record;
    int active = -1;
};

void position(bn::sprite_ptr& sprite, int x, int y)
{
    sprite.set_position(x - 120, y - 80);
}

void position(bn::sprite_ptr& sprite, Vec coord)
{
    position(sprite, (coord.x + unit / 2) / unit, (coord.y + unit / 2) / unit);
}

const char* group_label(Group group)
{
    switch(group)
    {
        case Group::solids: return "SOLIDS";
        case Group::stripes: return "STRIPES";
        default: return "OPEN";
    }
}

const char* foul_label(Notice notice)
{
    switch(notice)
    {
        case Notice::scratch: return "SCRATCH";
        case Notice::no_contact: return "NO CONTACT";
        case Notice::wrong_ball: return "WRONG BALL";
        case Notice::no_rail: return "NO RAIL";
        default: return "FOUL";
    }
}

int aim_step(int held_frames, bool fine)
{
    if(held_frames == 0)
    {
        return 1;
    }
    if(held_frames < 9)
    {
        return 0;
    }
    if(fine)
    {
        return (held_frames - 9) % 3 == 0 ? 1 : 0;
    }
    if(held_frames < 20)
    {
        return held_frames & 1;
    }
    return std::min(6, 2 + (held_frames - 20) / 7);
}

struct CueSprite
{
    Vec center;
    int rotation;
    int size;
};

bn::optional<CueSprite> cue_sprite(const Table& table, int angle, int pullback)
{
    Vec cue = table.balls[0].pos;
    Vec heading = direction(angle);
    int gap = (7 + pullback) * unit;
    int reach = gap + cue_length * unit;

    // Keep the rear end inside the table frame, including during pullback.
    if(heading.x)
    {
        int edge = heading.x > 0 ? 12 * unit : 227 * unit;
        reach = std::min(reach, (cue.x - edge) * direction_unit / heading.x);
    }
    if(heading.y)
    {
        int edge = heading.y > 0 ? 37 * unit : 130 * unit;
        reach = std::min(reach, (cue.y - edge) * direction_unit / heading.y);
    }
    gap = std::clamp(reach - 5 * unit, 4 * unit, gap);
    int size = reach - gap;
    if(size < 3 * unit)
    {
        return bn::nullopt;
    }

    // Table angles grow clockwise in screen coordinates. Butano uses the opposite sign.
    return CueSprite{ cue - travel(heading, gap + size / 2), (-angle) & (angle_count - 1), size };
}

class View
{
public:
    View()
    {
        stick.set_bg_priority(1);
        for(int id = 0; id < 16; ++id)
        {
            balls.push_back(make_sprite(*ball_items[id], 0));
        }
        for(int id = 0; id < 14; ++id)
        {
            group_balls.push_back(make_sprite(*ball_items[1], 0, 1));
        }
        for(int i = 0; i < 2; ++i)
        {
            avatars.push_back(make_sprite(bn::sprite_items::avatars, 0, i));
            position(avatars.back(), 15 + i * 120, 12);
        }
        for(int index = 0; index < meter_sections; ++index)
        {
            bar.push_back(make_sprite(bn::sprite_items::meter, 1));
            position(bar.back(), 47 + index * 8, 151);
        }
    }

    void show_lobby(const Profile& profile)
    {
        clear_panel();
        line.close();
        background.reset();
        background = bn::regular_bg_items::lobby.create_bg(8, 48);
        background->set_priority(3);
        hide_sprites();
        hud.clear();
        footer.clear();
        const Rival& rival = rivals[profile.rival_index()];
        label(hud, 52, 104, bn::format<16>("{}", profile.rating), mint_text, Align::LEFT);
        label(hud, 116, 104, "VS", muted_text);
        label(hud, 134, 104, rival.name);
        label(hud, 193, 104, bn::format<24>("{} W / {} L", profile.wins, profile.losses), muted_text);
    }

    void show_menu()
    {
        clear_panel();
        line.close();
        background.reset();
        background = bn::regular_bg_items::menu.create_bg(8, 48);
        background->set_priority(3);
        hide_sprites();
        hud.clear();
        footer.clear();
    }

    void show_room()
    {
        clear_panel();
        line.close();
        background.reset();
        background = bn::regular_bg_items::room.create_bg(8, 48);
        background->set_priority(3);
        line.show();
        hud.clear();
        footer.clear();
    }

    void controls(bool paused, bool linked = false)
    {
        if(! paused)
        {
            show_menu();
        }
        open_panel();
        label(panel_text, 120, 49, paused ? "MATCH PAUSED" : "HOW TO PLAY", gold_text);
        label(panel_text, 36, 66, "LEFT / RIGHT", mint_text, Align::LEFT);
        label(panel_text, 112, 66, "AIM", ivory_text, Align::LEFT);
        label(panel_text, 36, 73, "L / R", mint_text, Align::LEFT);
        label(panel_text, 112, 73, "FINE AIM", ivory_text, Align::LEFT);
        label(panel_text, 36, 80, "UP / DOWN", mint_text, Align::LEFT);
        label(panel_text, 112, 80, "CYCLE POCKET LINES", ivory_text, Align::LEFT);
        label(panel_text, 36, 87, "HOLD A", mint_text, Align::LEFT);
        label(panel_text, 112, 87, "RELEASE TO SHOOT", ivory_text, Align::LEFT);
        label(panel_text, 36, 94, "B", mint_text, Align::LEFT);
        label(panel_text, 112, 94, "CANCEL / RESET POWER", ivory_text, Align::LEFT);
        label(panel_text, 36, 101, "D-PAD / A", mint_text, Align::LEFT);
        label(panel_text, 112, 101, "MOVE / PLACE CUE", ivory_text, Align::LEFT);
        if(paused)
        {
            panel_actions("A  RESUME", linked ? "B  LEAVE" : "B  CONCEDE");
        }
        else
        {
            label(panel_text, 120, 113, "A / B  BACK", gold_text);
        }
    }

    void concession(bool linked)
    {
        open_panel();
        label(panel_text, 120, 49, linked ? "LEAVE LINK MATCH" : "CONCEDE MATCH", gold_text);
        label(panel_text, 120, 80, linked ? "THIS ENDS THE MATCH FOR BOTH PLAYERS" : "THIS COUNTS AS A RANKED LOSS");
        panel_actions(linked ? "A  LEAVE" : "A  CONCEDE", "B  BACK");
    }

    void link_status(const char* heading, const char* first, const char* second, const char* action)
    {
        open_panel();
        label(panel_text, 120, 49, heading, gold_text);
        label(panel_text, 120, 75, first);
        label(panel_text, 120, 88, second, muted_text);
        label(panel_text, 120, 113, action, mint_text);
    }

    void link_result(bool won, const char* reason)
    {
        link_status(won ? "YOU WIN" : "YOU LOSE", reason, "LINK CABLE MATCH", "A / B  LOBBY");
    }

    void result(bool won, const char* reason, const Profile& profile, int change, int new_rival)
    {
        open_panel();
        label(panel_text, 120, 49, won ? "YOU WIN" : "YOU LOSE", gold_text);
        label(panel_text, 120, 70, reason, muted_text);
        label(panel_text, 120, 81,
              bn::format<48>("RATING {}  {}{}", profile.rating, change >= 0 ? "+" : "-", change >= 0 ? change : -change),
              mint_text);
        if(new_rival >= 0)
        {
            label(panel_text, 120, 92,
                  bn::format<48>("UNLOCKED  {} / {}", rivals[new_rival].name, rivals[new_rival].rank));
        }
        else
        {
            label(panel_text, 120, 92, bn::format<48>("{} W / {} L   BEST {}", profile.wins, profile.losses, profile.best));
        }
        panel_actions("A  NEXT MATCH", "B  LOBBY");
    }

    void clear_panel()
    {
        panel_text.clear();
        panel.reset();
    }

    void scoreboard(const EightBall& game, const Profile& profile, const Rival& rival, Phase phase, int choices, int selected, int link_player, int call, int cursor_pocket)
    {
        hud.clear();
        footer.clear();
        bool linked = link_player >= 0;
        int local = linked ? link_player : 0;
        position(turn, game.player ? 183 : 63, 12);
        turn.set_visible(true);

        for(int player = 0; player < 2; ++player)
        {
            avatars[player].set_tiles(bn::sprite_items::avatars.tiles_item(), linked ? player : player ? 2 : 0);
            avatars[player].set_visible(true);
            Group group = game.groups[player];
            int x = player * 120;
            int start = group == Group::stripes ? 9 : 1;
            bool on_eight = group != Group::open && game.remaining(group) == 0;
            const auto& name_color = player == game.player ? gold_text : ivory_text;
            const char* name = player == local ? "YOU" : linked ? "FRIEND" : rival.name;
            label(hud, x + 23, 12, name, name_color, Align::LEFT);
            label(hud, x + 104, 12, linked ? bn::format<12>("GBA {}", player + 1) :
                  bn::format<12>("{}", player ? rival.rating : profile.rating), mint_text, Align::RIGHT);
            label(hud, x + 15, 26, on_eight ? "8 BALL" : group_label(group), muted_text, Align::LEFT);

            int visible = 0;
            int count = on_eight ? 1 : game.remaining(group);
            for(int index = 0; index < 7; ++index)
            {
                bn::sprite_ptr& icon = group_balls[player * 7 + index];
                int id = start + index;
                bool show = group != Group::open && (on_eight ? index == 0 : game.table.balls[id].live);
                if(show)
                {
                    icon.set_item(*ball_items[on_eight ? 8 : id], 1);
                    position(icon, x + 76 + (2 * visible - (count - 1)) * 4, 26);
                    ++visible;
                }
                icon.set_visible(show);
            }
        }

        bool human_aim = phase == Phase::aim || phase == Phase::charge;
        bool human_shot = phase == Phase::rolling && game.player == local;
        show_power(human_aim || human_shot);
        if(phase == Phase::call)
        {
            label(footer, 120, 139, bn::format<48>("CALL THE 8 / {}", pocket_names[cursor_pocket]), gold_text);
            label(footer, 120, 151, call < 0 ? "D-PAD  CHOOSE    A  CALL" :
                  "D-PAD  CHOOSE   A  CALL   B  BACK", mint_text);
        }
        else if(phase == Phase::place)
        {
            label(footer, 120, 139, bn::format<48>("{}  /  BALL IN HAND", foul_label(game.notice)), gold_text);
            label(footer, 120, 151, "D-PAD  MOVE     A  PLACE", mint_text);
        }
        else if(phase == Phase::remote || phase == Phase::link_send)
        {
            label(footer, 120, 139, phase == Phase::remote ?
                  (game.ball_in_hand ? "FRIEND HAS BALL IN HAND" : "FRIEND IS AIMING") : "SYNCING SHOT");
            label(footer, 120, 151, "LINK CABLE", muted_text);
        }
        else if(phase == Phase::cpu_wait || phase == Phase::cpu_aim)
        {
            if(game.ball_in_hand)
            {
                label(footer, 120, 139, bn::format<48>("{}  /  {} HAS BALL IN HAND", foul_label(game.notice), rival.name));
            }
            else
            {
                label(footer, 120, 139, bn::format<32>("{} {}", rival.name, game.breaking ? "BREAKS" : "IS AIMING"));
            }
            label(footer, 120, 151, rival.rank, muted_text);
        }
        else if(phase == Phase::rolling)
        {
            label(footer, 120, 139, game.player == local ? bn::string<32>("YOUR SHOT") :
                  bn::format<32>("{} SHOT", linked ? "FRIEND" : rival.name));
            label(footer, 224, 151, "START  PAUSE", muted_text, Align::RIGHT);
        }
        else
        {
            const char* status = "YOUR TURN";
            if(game.notice == Notice::spotted)
            {
                status = "8 BALL RESPOTTED";
            }
            else if(game.breaking)
            {
                status = "YOUR BREAK";
            }
            else if(game.legal_target(8))
            {
                status = "SINK THE 8 BALL";
            }
            else if(game.groups[0] == Group::open)
            {
                status = "YOUR TURN  /  OPEN TABLE";
            }
            if(call >= 0)
            {
                label(footer, 120, 139, bn::format<48>("CALLED {}{}", pocket_names[call],
                      choices ? " / UP-DOWN LINE" : ""), gold_text);
            }
            else if(selected >= 0)
            {
                label(footer, 120, 139, bn::format<24>("POCKET LINE {} / {}", selected + 1, choices), gold_text);
            }
            else
            {
                label(footer, 120, 139, status, gold_text);
            }
            const char* controls = phase == Phase::charge ? "RELEASE A / B BACK" :
                                   call >= 0 ? "SELECT POCKET / HOLD A" :
                                   choices ? "UP/DN SHOTS  A POWER" : "HOLD A / L R FINE";
            label(footer, 224, 151, controls, mint_text, Align::RIGHT);
        }
        if(human_aim || human_shot)
        {
            label(footer, 15, 151, "POWER", muted_text, Align::LEFT);
        }
    }

    void show_balls(const Table& table, bool placing, Vec placement, bool valid)
    {
        for(int id = 0; id < 16; ++id)
        {
            const Ball& ball = table.balls[id];
            if(ball.live || (! id && placing && valid))
            {
                position(balls[id], ! id && placing ? placement : ball.pos);
                balls[id].set_visible(true);
            }
            else
            {
                balls[id].set_visible(false);
            }
        }
    }

    void power(int speed)
    {
        int filled = (std::clamp(speed, 0, max_speed) * meter_sections * 8 + max_speed / 2) / max_speed;
        if(filled == bar_filled)
        {
            return;
        }
        bar_filled = filled;
        for(int i = 0; i < meter_sections; ++i)
        {
            bar[i].set_tiles(bn::sprite_items::meter.tiles_item(), std::clamp(filled - 8 * i, 0, 8));
        }
    }

    void placement_marker(Vec placement, bool valid)
    {
        clear_aim();
        cursor.set_tiles(bn::sprite_items::placement.tiles_item(), valid ? 0 : 1);
        position(cursor, placement);
        cursor.set_visible(true);
    }

    void clear_aim()
    {
        stick.set_visible(false);
        ring.set_visible(false);
        called.set_visible(false);
        cursor.set_visible(false);
        line.clear();
    }

    void draw_cue(const Table& table, int angle, int pullback)
    {
        if(bn::optional<CueSprite> pose = cue_sprite(table, angle, pullback))
        {
            position(stick, pose->center);
            stick.set_rotation_angle(bn::fixed(pose->rotation * 360) / angle_count);
            stick.set_horizontal_scale(bn::fixed(pose->size) / (64 * unit));
            stick.set_visible(true);
        }
        else
        {
            stick.set_visible(false);
        }
    }

    void highlight_call(int pocket)
    {
        if(pocket < 0)
        {
            called.set_visible(false);
            return;
        }
        position(called, pockets[pocket]);
        called.set_visible(true);
    }

    void trajectory(const EightBall& game, int angle, int call)
    {
        ring.set_visible(false);
        highlight_call(call);
        cursor.set_visible(false);
        const Table& table = game.table;
        Vec cue = table.balls[0].pos;
        Preview path = trace_aim(table, direction(angle));
        Ray first = path.first;
        if(length(first.end - cue) < 9 * unit)
        {
            line.clear();
            return;
        }
        line.begin();
        Ray contact = path.bounced ? path.bank : first;
        bool legal = ! path.hit || game.legal_target(contact.ball);
        LineInk cue_ink = ! legal && ! path.bounced ? LineInk::warning : LineInk::cue;
        line.draw(cue, first.end, cue_ink, 5, first.ball >= 0 ? 4 : 0);
        if(path.bounced)
        {
            line.draw(first.end, contact.end, legal ? LineInk::bank : LineInk::warning,
                      0, contact.ball >= 0 ? 4 : 0);
        }
        if(path.hit)
        {
            if(length(contact.end - cue) >= 9 * unit)
            {
                move_ring(contact.end, legal);
            }
            if(path.object_moves)
            {
                LineSpan incoming{ cue, first.end };
                Vec object = table.balls[contact.ball].pos;
                line.draw(object, path.object.end, legal ? LineInk::object : LineInk::warning,
                          5, 2, &incoming);
            }
        }
        line.commit();
    }

private:
    bn::optional<bn::regular_bg_ptr> background;
    bn::optional<bn::regular_bg_ptr> panel;
    bn::sprite_text_generator text{ hud_font };
    bn::vector<bn::sprite_ptr, 16> hud;
    bn::vector<bn::sprite_ptr, 16> footer;
    bn::vector<bn::sprite_ptr, 32> panel_text;
    bn::vector<bn::sprite_ptr, 16> balls;
    bn::vector<bn::sprite_ptr, 14> group_balls;
    ShotLine line;
    bn::vector<bn::sprite_ptr, meter_sections> bar;
    bn::vector<bn::sprite_ptr, 2> avatars;
    bn::sprite_ptr stick = make_sprite(bn::sprite_items::cue, -2);
    bn::sprite_ptr ring = make_mark(circle, 1);
    bn::sprite_ptr called = make_sprite(bn::sprite_items::pocket_marker, -1);
    bn::sprite_ptr cursor = make_sprite(bn::sprite_items::placement, 1);
    bn::sprite_ptr turn = make_mark(turn_diamond, 0);
    int bar_filled = 0;


    static bn::sprite_ptr make_sprite(const bn::sprite_item& item, int z, int index = 0)
    {
        bn::sprite_ptr result = item.create_sprite(0, 0, index);
        result.set_bg_priority(2);
        result.set_z_order(z);
        result.set_visible(false);
        return result;
    }

    static bn::sprite_ptr make_mark(Mark mark, int z)
    {
        return make_sprite(bn::sprite_items::marks, z, mark);
    }

    void label(bn::ivector<bn::sprite_ptr>& target, int x, int y, const bn::string_view& value,
               const bn::sprite_palette_item& palette = ivory_text, Align align = Align::CENTER)
    {
        text.set_palette_item(palette);
        text.set_alignment(align);
        bool on_panel = &target == &panel_text;
        text.set_bg_priority(on_panel ? 0 : 2);
        text.generate(x - 120, y - 80, value, target);
    }

    void panel_actions(const char* primary, const char* secondary)
    {
        label(panel_text, 75, 113, primary, gold_text);
        label(panel_text, 165, 113, secondary, mint_text);
    }

    void open_panel()
    {
        panel_text.clear();
        footer.clear();
        show_power(false);
        hide_playfield();
        if(! panel)
        {
            panel = bn::regular_bg_items::panel.create_bg(8, 48);
            panel->set_priority(1);
        }
    }

    void move_ring(Vec point, bool legal)
    {
        ring.set_tiles(bn::sprite_items::marks.tiles_item(), legal ? circle : foul_circle);
        position(ring, point);
        ring.set_visible(true);
    }

    void show_power(bool visible)
    {
        for(bn::sprite_ptr& segment : bar)
        {
            segment.set_visible(visible);
        }
    }

    void hide_playfield()
    {
        clear_aim();
        for(bn::sprite_ptr& ball : balls)
        {
            ball.set_visible(false);
        }
    }

    void hide_sprites()
    {
        hide_playfield();
        for(bn::sprite_ptr& ball : group_balls)
        {
            ball.set_visible(false);
        }
        for(bn::sprite_ptr& avatar : avatars)
        {
            avatar.set_visible(false);
        }
        turn.set_visible(false);
        show_power(false);
    }
};

class Game
{
public:
    Game()
    {
        view.show_lobby(records.profile());
    }

    void tick()
    {
        ++frame;
        cable.update();
        if(cable.lost() && screen != Screen::result && screen != Screen::link_leaving)
        {
            disconnect();
            return;
        }
        switch(screen)
        {
            case Screen::lobby:
                if(bn::keypad::a_pressed() || bn::keypad::start_pressed())
                {
                    start_match();
                }
                else if(bn::keypad::b_pressed())
                {
                    cable.open();
                    screen = Screen::link_wait;
                    view.show_menu();
                    view.link_status("LINK CABLE", "WAITING FOR A FRIEND", "OPEN LINK ON BOTH GBAS", "B  BACK");
                }
                else if(bn::keypad::select_pressed())
                {
                    screen = Screen::help;
                    view.controls(false);
                }
                break;
            case Screen::help:
                if(bn::keypad::a_pressed() || bn::keypad::b_pressed() || bn::keypad::select_pressed())
                {
                    lobby();
                }
                break;
            case Screen::match:
                if(bn::keypad::start_pressed())
                {
                    if(phase == Phase::charge)
                    {
                        set_phase(Phase::aim);
                    }
                    screen = Screen::pause;
                    view.controls(true, linked());
                    break;
                }
                play();
                break;
            case Screen::pause:
                if(bn::keypad::a_pressed() || bn::keypad::start_pressed())
                {
                    screen = Screen::match;
                    view.clear_panel();
                    score();
                    last_guide = -1;
                    draw_table();
                }
                else if(bn::keypad::b_pressed())
                {
                    screen = Screen::concede;
                    view.concession(linked());
                }
                break;
            case Screen::concede:
                if(bn::keypad::a_pressed())
                {
                    if(linked())
                    {
                        cable.leave();
                        leave_at = frame + 24;
                        screen = Screen::link_leaving;
                        view.link_status("LINK CABLE", "LEAVING MATCH", "", "");
                    }
                    else
                    {
                        complete(false, "MATCH CONCEDED");
                    }
                }
                else if(bn::keypad::b_pressed())
                {
                    screen = Screen::pause;
                    view.controls(true, linked());
                }
                break;
            case Screen::result:
                if(linked() && cable.sending() && ! cable.lost())
                {
                    break;
                }
                if(bn::keypad::b_pressed() || (linked() && bn::keypad::a_pressed()))
                {
                    lobby();
                }
                else if(bn::keypad::a_pressed())
                {
                    start_match();
                }
                break;
            case Screen::link_wait:
                if(bn::keypad::b_pressed())
                {
                    lobby();
                }
                else if(cable.paired())
                {
                    start_match();
                }
                break;
            case Screen::link_lost:
                if(bn::keypad::a_pressed() || bn::keypad::b_pressed())
                {
                    lobby();
                }
                break;
            case Screen::link_leaving:
                if(frame == leave_at)
                {
                    lobby();
                }
                break;
            default: break;
        }
    }

private:
    Records records;
    View view;
    EightBall game;
    Screen screen = Screen::lobby;
    Phase phase = Phase::aim;
    Vec placement;
    ShotPlanner planner;
    PocketSuggestions suggestions;
    Plan cpu_plan;
    uint32_t seed = 0x51482031u;
    unsigned frame = 0;
    int angle = 0;
    int last_guide = -1;
    int last_pullback = 0;
    int aim_power = min_speed;
    int selected_line = -1;
    int called_pocket = -1;
    int call_cursor = 0;
    int pending_line = 0;
    bool finding_lines = false;
    int aim_hold = 0;
    int previous_aim_input = 0;
    bool previous_fine_aim = false;
    int charge = 0;
    int hold = 0;
    int timer = 0;
    int match_rival = 0;
    int sound_cooldown = 0;
    uint16_t last_pots = 0;
    CableMatch cable;
    int turn_index = 0;
    unsigned leave_at = 0;
    Vec link_placement;
    bool placed_for_link = false;
    int pending_speed = 0;

    bool linked() const { return cable.paired(); }
    int local_player() const { return linked() ? cable.player() : 0; }

    const Rival& rival() const { return rivals[match_rival]; }

    void lobby()
    {
        cable.close();
        screen = Screen::lobby;
        view.show_lobby(records.profile());
    }

    void disconnect()
    {
        cable.close();
        screen = Screen::link_lost;
        view.link_status("LINK ENDED", "FRIEND DISCONNECTED", "", "A / B  LOBBY");
    }

    void start_match()
    {
        screen = Screen::match;
        match_rival = records.profile().rival_index();
        seed ^= frame * 0x9e3779b9u;
        int breaker = linked() ? 0 : (records.profile().wins + records.profile().losses) & 1;
        turn_index = 0;
        game.start(breaker);
        angle = 0;
        view.show_room();
        prepare_turn();
        draw_table();
        bn::sound_items::confirm.play(bn::fixed(0.45));
    }

    void complete(bool won, const char* reason)
    {
        screen = Screen::result;
        view.clear_aim();
        if(linked())
        {
            view.link_result(won, reason);
        }
        else
        {
            int change = records.add_result(won, rival().rating);
            int unlocked = records.profile().rival_index();
            score();
            view.result(won, reason, records.profile(), change, unlocked > match_rival ? unlocked : -1);
        }
        bn::sound_items::confirm.play(bn::fixed(0.55));
    }

    void score()
    {
        view.scoreboard(game, records.profile(), rival(), phase, finding_lines ? 0 : suggestions.size(), selected_line,
                        linked() ? cable.player() : -1, called_pocket, call_cursor);
    }

    void set_phase(Phase value)
    {
        phase = value;
        timer = 0;
        last_guide = -1;
        aim_hold = 0;
        previous_aim_input = 0;
        hold = 0;
        if(phase != Phase::charge && phase != Phase::rolling)
        {
            charge = 0;
        }
        score();
    }

    int suggest_aim() const
    {
        Vec cue = game.table.balls[0].pos;
        int closest = 1000000;
        int choice = angle;
        for(int id = 1; id < 16; ++id)
        {
            const Ball& object = game.table.balls[id];
            if(object.live && game.legal_target(id))
            {
                int distance = length(object.pos - cue);
                if(distance < closest)
                {
                    closest = distance;
                    choice = direction_angle(object.pos - cue);
                }
            }
        }
        return choice;
    }

    void prepare_turn()
    {
        placed_for_link = false;
        called_pocket = -1;
        if(linked())
        {
            cable.new_turn(turn_index, game.player != local_player());
            if(game.player != local_player())
            {
                set_phase(Phase::remote);
                return;
            }
        }
        else if(game.player)
        {
            planner.start(game, match_rival);
            set_phase(Phase::cpu_wait);
            return;
        }
        if(game.ball_in_hand)
        {
            placement = game.table.free_spot();
            set_phase(Phase::place);
        }
        else
        {
            prepare_aim();
        }
    }

    void prepare_aim()
    {
        aim_power = game.breaking ? break_power : min_speed;
        angle = game.breaking ? 0 : suggest_aim();
        if(game.requires_call())
        {
            Preview path = preview_shot(game.table, direction(angle), max_speed);
            Ray contact = path.bounced ? path.bank : path.first;
            call_cursor = contact.ball == 8 && path.object.pocket >= 0 ? path.object.pocket :
                          closest_pocket(game.table.balls[8].pos);
            set_phase(Phase::call);
        }
        else
        {
            start_aiming();
        }
    }

    void start_aiming()
    {
        selected_line = -1;
        pending_line = 0;
        finding_lines = ! game.breaking;
        suggestions.start(called_pocket);
        set_phase(Phase::aim);
    }

    void call_input()
    {
        int column = call_cursor % 3;
        int row = call_cursor / 3;
        column = std::clamp(column + int(bn::keypad::right_pressed()) - int(bn::keypad::left_pressed()), 0, 2);
        row = std::clamp(row + int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed()), 0, 1);
        int next = row * 3 + column;
        if(next != call_cursor)
        {
            call_cursor = next;
            score();
        }
        if(bn::keypad::a_pressed())
        {
            called_pocket = call_cursor;
            aim_power = min_speed;
            start_aiming();
        }
        else if(bn::keypad::b_pressed() && called_pocket >= 0)
        {
            set_phase(Phase::aim);
        }
    }

    void guide_input()
    {
        int change = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());
        if(change)
        {
            pending_line = change;
        }
        if(finding_lines)
        {
            finding_lines = ! suggestions.advance(game);
            if(finding_lines)
            {
                return;
            }
            if(! pending_line || ! suggestions.size())
            {
                score();
            }
        }
        if(pending_line && suggestions.size())
        {
            int count = suggestions.size();
            selected_line = selected_line < 0 ? 0 :
                            (selected_line + pending_line + count) % count;
            const Plan& choice = suggestions[selected_line];
            angle = choice.angle;
            aim_power = choice.speed;
            score();
        }
        pending_line = 0;
    }

    void local_shot(int speed)
    {
        if(linked())
        {
            if(cable.sending())
            {
                pending_speed = speed;
                set_phase(Phase::link_send);
                return;
            }
            Vec point = placed_for_link ? link_placement : Vec{};
            cable.send_shot(turn_index, { angle, speed, point.x / unit, point.y / unit, called_pocket });
        }
        strike(speed, called_pocket);
    }

    void remote_shot()
    {
        CableShot shot;
        if(! cable.receive_shot(shot))
        {
            return;
        }
        Vec point = px(shot.x, shot.y);
        bool valid_call = game.requires_call() ? shot.pocket >= 0 && shot.pocket < 6 : shot.pocket == -1;
        if(shot.speed < min_speed || shot.speed > max_speed || ! valid_call ||
           (game.ball_in_hand ? ! game.table.can_place(point) : (point.x || point.y)))
        {
            disconnect();
            return;
        }
        if(game.ball_in_hand)
        {
            game.table.place_cue(point);
            game.ball_in_hand = false;
        }
        cable.accept_shot();
        angle = shot.angle;
        strike(shot.speed, shot.pocket);
    }

    void strike(int speed, int pocket)
    {
        game.table.strike(direction(angle), speed, pocket);
        charge = speed;
        last_pots = 0;
        sound_cooldown = 0;
        set_phase(Phase::rolling);
        bn::sound_items::strike.play(bn::fixed(0.65));
    }

    void steer()
    {
        int input = int(bn::keypad::right_held()) - int(bn::keypad::left_held());
        bool fine = bn::keypad::l_held() || bn::keypad::r_held();
        if(! input && fine)
        {
            input = int(bn::keypad::r_held()) - int(bn::keypad::l_held());
        }
        if(input != previous_aim_input || fine != previous_fine_aim)
        {
            aim_hold = 0;
            previous_aim_input = input;
            previous_fine_aim = fine;
        }
        if(input)
        {
            int amount = aim_step(aim_hold, fine);
            angle = (angle + input * amount) & (angle_count - 1);
            ++aim_hold;
            if(amount && selected_line >= 0)
            {
                selected_line = -1;
                score();
            }
        }
    }

    void place_ball()
    {
        int x = int(bn::keypad::right_held()) - int(bn::keypad::left_held());
        int y = int(bn::keypad::down_held()) - int(bn::keypad::up_held());
        hold = (x || y) ? hold + 1 : 0;
        int pace = hold > 16 ? 2 * unit : unit;
        placement.x = std::clamp(placement.x + x * pace, left, right);
        placement.y = std::clamp(placement.y + y * pace, top, bottom);
        if(bn::keypad::a_pressed() && game.table.can_place(placement))
        {
            // Link packets use whole-pixel positions. The placement cursor moves on that grid.
            game.table.place_cue(placement);
            link_placement = placement;
            placed_for_link = true;
            game.ball_in_hand = false;
            prepare_aim();
            bn::sound_items::confirm.play(bn::fixed(0.32));
        }
    }

    void roll()
    {
        bool moving = game.table.step();
        if(sound_cooldown > 0)
        {
            --sound_cooldown;
        }
        if(game.table.shot.pocketed != last_pots)
        {
            last_pots = game.table.shot.pocketed;
            bn::sound_items::drop.play(bn::fixed(0.7));
            score();
        }
        else if(! sound_cooldown && game.table.impact > 17)
        {
            bn::sound_items::click.play(bn::fixed(std::min(game.table.impact, 250)) / 360 + bn::fixed(0.12));
            sound_cooldown = 4;
        }
        if(! moving)
        {
            game.finish_shot();
            ++turn_index;
            if(game.outcome == Outcome::playing)
            {
                prepare_turn();
            }
            else
            {
                const char* reason = game.outcome == Outcome::cleared ? "8 BALL CLEARED" :
                                     game.outcome == Outcome::early_eight ? "EARLY 8 BALL" :
                                     game.outcome == Outcome::wrong_pocket ? "WRONG POCKET ON THE 8" : "FOUL ON THE 8 BALL";
                complete(game.winner == local_player(), reason);
            }
        }
    }

    void play()
    {
        ++timer;
        switch(phase)
        {
            case Phase::aim:
                guide_input();
                steer();
                if(called_pocket >= 0 && bn::keypad::select_pressed())
                {
                    call_cursor = called_pocket;
                    set_phase(Phase::call);
                }
                else if(bn::keypad::b_pressed())
                {
                    selected_line = -1;
                    aim_power = game.breaking ? break_power : min_speed;
                    score();
                }
                else if(bn::keypad::a_pressed())
                {
                    charge = aim_power;
                    set_phase(Phase::charge);
                }
                break;
            case Phase::charge:
                if(bn::keypad::b_pressed())
                {
                    set_phase(Phase::aim);
                }
                else if(bn::keypad::a_released())
                {
                    local_shot(charge);
                }
                else
                {
                    steer();
                    if(timer > 6)
                    {
                        charge = std::min(max_speed, charge + (timer < 24 ? 5 : 8));
                    }
                }
                break;
            case Phase::call:
                call_input();
                break;
            case Phase::place:
                place_ball();
                break;
            case Phase::rolling:
                roll();
                break;
            case Phase::cpu_wait:
                if(planner.advance(game, seed) && timer >= 32)
                {
                    cpu_plan = planner.result();
                    if(game.ball_in_hand)
                    {
                        game.table.place_cue(cpu_plan.placement);
                        game.ball_in_hand = false;
                    }
                    set_phase(Phase::cpu_aim);
                }
                break;
            case Phase::remote:
                remote_shot();
                break;
            case Phase::link_send:
                if(! cable.sending())
                {
                    local_shot(pending_speed);
                }
                break;
            case Phase::cpu_aim:
                angle = (cpu_plan.angle - std::max(0, 24 - timer * 2)) & (angle_count - 1);
                if(timer >= 29)
                {
                    strike(cpu_plan.speed, cpu_plan.pocket);
                }
                break;
            default: break;
        }
        if(screen == Screen::match)
        {
            draw_table();
        }
    }

    void draw_table()
    {
        bool placing = phase == Phase::place;
        bool valid_placement = placing && game.table.can_place(placement);
        view.show_balls(game.table, placing, placement, valid_placement);
        view.power(phase == Phase::aim ? 0 : charge);
        switch(phase)
        {
            case Phase::aim:
            case Phase::charge:
            {
                int pullback = phase == Phase::charge ? 1 + (charge - aim_power) * 10 / (max_speed - min_speed) : 0;
                bool aim_changed = angle != last_guide;
                if(aim_changed)
                {
                    view.trajectory(game, angle, called_pocket);
                    last_guide = angle;
                }
                if(aim_changed || pullback != last_pullback)
                {
                    view.draw_cue(game.table, angle, pullback);
                    last_pullback = pullback;
                }
                break;
            }
            case Phase::call:
                view.clear_aim();
                view.highlight_call(call_cursor);
                break;
            case Phase::place:
                view.placement_marker(placement, valid_placement);
                break;
            case Phase::cpu_aim:
                view.clear_aim();
                view.draw_cue(game.table, angle, std::max(0, timer - 12) / 2);
                view.highlight_call(cpu_plan.pocket);
                break;
            case Phase::rolling:
                view.clear_aim();
                view.highlight_call(game.table.shot.called_pocket);
                break;
            case Phase::cpu_wait:
            case Phase::remote:
            case Phase::link_send:
                view.clear_aim();
                break;
            default: break;
        }
    }
};
}

int main()
{
    bn::core::init();
    Game game;
    while(true)
    {
        game.tick();
        bn::core::update();
    }
}
