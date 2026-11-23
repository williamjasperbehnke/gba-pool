#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bn_array.h"
#include "bn_core.h"
#include "bn_format.h"
#include "bn_keypad.h"
#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sound_items.h"
#include "bn_sprite_font.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_affine_mat_ptr.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_tile.h"
#include "bn_sprite_text_generator.h"
#include "bn_sram.h"
#include "bn_string.h"
#include "bn_vector.h"

#include "bn_regular_bg_items_lobby.h"
#include "bn_regular_bg_items_menu.h"
#include "bn_regular_bg_items_panel.h"
#include "bn_regular_bg_items_pause_panel.h"
#include "bn_regular_bg_items_room.h"
#include "bn_regular_bg_items_match_hud.h"
#include "bn_regular_bg_items_match_hud_compact.h"
#include "bn_regular_bg_items_aim_stage.h"
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
#include "bn_sprite_items_close_cue.h"
#include "bn_sprite_items_hud_font.h"
#include "bn_sprite_items_marks.h"
#include "bn_sprite_items_meter.h"
#include "bn_sprite_items_spin_ball.h"
#include "bn_sprite_items_spin_dot.h"
#include "bn_sprite_items_active_player.h"
#include "bn_sprite_items_called_pockets.h"

#include "font_widths.h"
#include "pool.h"
#include "pocket_art.h"
#include "pool_cable.h"
#include "pool_view.h"
#include "aim_table.h"
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

enum class CueBallTint { ivory, red, bright_red };

constexpr auto placement_colors(bool bright)
{
    bn::array<bn::color, 16> colors{};
    for(int shade = 0; shade < 5; ++shade)
    {
        colors[8 + shade] = bright ? bn::color(27 + shade, 6 + 2 * shade, 7 + shade) :
                                     bn::color(13 + 2 * shade, 1 + shade, 2 + shade);
    }
    return colors;
}
constexpr auto placement_flash_colors = placement_colors(true);
constexpr auto placement_red_colors = placement_colors(false);
constexpr bn::sprite_palette_item placement_flash_palette(placement_flash_colors, bn::bpp_mode::BPP_4);
constexpr bn::sprite_palette_item placement_red_palette(placement_red_colors, bn::bpp_mode::BPP_4);

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
constexpr auto shadow_text_colors = text_colors(bn::color(2, 4, 5));
constexpr bn::sprite_palette_item mint_text(mint_colors, bn::bpp_mode::BPP_4);
constexpr bn::sprite_palette_item gold_text(gold_colors, bn::bpp_mode::BPP_4);
constexpr bn::sprite_palette_item muted_text(muted_colors, bn::bpp_mode::BPP_4);
constexpr bn::sprite_palette_item shadow_text(shadow_text_colors, bn::bpp_mode::BPP_4);
constexpr auto& ivory_text = bn::sprite_items::hud_font.palette_item();
using Align = bn::sprite_text_generator::alignment_type;

constexpr int meter_sections = 5;
constexpr int cue_length = 54;
constexpr int break_power = 340;
constexpr int rating_magic = 0x38425231;

constexpr const char* pocket_names[] = {
    "TOP LEFT", "TOP MIDDLE", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM MIDDLE", "BOTTOM RIGHT"
};
enum class Screen { lobby, match, help, pause, pause_help, concede, result, link_wait, link_lost, link_leaving };
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

int aim_step(int held_frames)
{
    if(held_frames == 0)
    {
        return 1;
    }
    if(held_frames < 9)
    {
        return 0;
    }
    return std::min(4, 1 + (held_frames - 9) / 11);
}

constexpr int angle_delta(int target, int current)
{
    constexpr int half_turn = angle_count / 2;
    return ((target - current + half_turn) & (angle_count - 1)) - half_turn;
}

BN_DATA_EWRAM_BSS bn::tile sphere_tiles[16][sphere_words / 8];

struct BallVisual
{
    RollingBall spin;
    Vec previous;
    SphereProjection projection{};
    unsigned camera_revision = 0;
    int zoom = -1;
    uint16_t drawn_tiles = 0;
    bool shadow_visible = false;
    bool centered = false;
    bool shown = false;
};

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
    Vec center = project(cue, ball_height);
    Vec rear = project(cue - travel(heading, gap + cue_length * unit), ball_height);
    Vec front = project(cue - travel(heading, gap), ball_height);
    Vec clipped_center = center;
    if(! clip_view_segment(rear, clipped_center))
    {
        return bn::nullopt;
    }

    Vec axis = center - rear;
    int available = length(axis);
    if(available < 2 * unit)
    {
        return bn::nullopt;
    }
    int setback = std::min(length(center - front), available - std::min(8 * unit, available / 2));
    front = center - travel(normalized(axis), setback);
    Vec shaft = front - rear;
    // Butano's sprite rotation uses the opposite sign from screen coordinates.
    return CueSprite{ { (rear.x + front.x) / 2, (rear.y + front.y) / 2 },
                      (-direction_angle(shaft)) & (angle_count - 1), length(shaft) };
}

class View
{
public:
    View()
    {
        stick.set_bg_priority(1);
        close_stick.set_bg_priority(1);
        close_tail.set_bg_priority(1);
        for(bn::sprite_ptr& edge : active_border)
        {
            edge.set_bg_priority(0);
        }
        spin_ball.set_bg_priority(0);
        spin_dot.set_bg_priority(0);
        called.set_bg_priority(3);
        for(int i = 0; i < 8; ++i)
        {
            bn::sprite_affine_mat_ptr scale = bn::sprite_affine_mat_ptr::create();
            scale.set_scale(bn::fixed(2 + i) / 5);
            scales.push_back(std::move(scale));
        }
        for(int id = 0; id < 16; ++id)
        {
            bn::sprite_tiles_item tiles(sphere_tiles[id], bn::bpp_mode::BPP_4);
            bn::sprite_item sphere(bn::sprite_shape_size(sphere_sprite_size, sphere_sprite_size),
                                   tiles, ball_items[id]->palette_item());
            balls.push_back(make_sprite(sphere, 0));
            shadows.push_back(make_sprite(bn::sprite_items::marks, 6));
        }
        for(int id = 0; id < 14; ++id)
        {
            group_balls.push_back(make_sprite(*ball_items[1], 0));
            group_balls.back().set_bg_priority(0);
        }
        for(int i = 0; i < 2; ++i)
        {
            avatars.push_back(make_sprite(bn::sprite_items::avatars, 0, i));
            avatars.back().set_bg_priority(0);
        }
        for(int index = 0; index < meter_sections; ++index)
        {
            bar.push_back(make_sprite(bn::sprite_items::meter, 1));
            bar.back().set_bg_priority(0);
        }
    }

    void show_lobby(const Profile& profile)
    {
        clear_panel();
        line.close();
        aim_table.close();
        aim_backdrop.reset();
        close_view = false;
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
        aim_table.close();
        aim_backdrop.reset();
        close_view = false;
        background.reset();
        background = bn::regular_bg_items::menu.create_bg(8, 48);
        background->set_priority(3);
        hide_sprites();
        hud.clear();
        footer.clear();
    }

    void show_room(bool start_close)
    {
        clear_panel();
        hide_match_hud();
        line.close();
        aim_table.close();
        aim_backdrop.reset();
        close_view = false;
        background.reset();
        background = bn::regular_bg_items::room.create_bg(8, 48);
        background->set_priority(3);
        background->set_visible(! start_close);
        aim_backdrop = bn::regular_bg_items::aim_stage.create_bg(8, 48);
        aim_backdrop->set_priority(3);
        aim_backdrop->set_visible(start_close);
        configure_projection(false);
        for(int id = 0; id < 16; ++id)
        {
            uint16_t drawn_tiles = sphere_pose[id].drawn_tiles;
            sphere_pose[id] = {};
            sphere_pose[id].drawn_tiles = drawn_tiles;
            if(id >= 9)
            {
                sphere_pose[id].spin = { id & 1 ? 320 : -320, 800, -552 };
            }
        }
        hud.clear();
        footer.clear();
    }

    void controls(bool paused)
    {
        if(! paused)
        {
            show_menu();
        }
        open_panel();
        label(panel_text, 120, 34, "HOW TO PLAY", gold_text);
        constexpr const char* buttons[] = {
            "L / R", "D-PAD", "PAUSE / UP", "PAUSE / L / R", "SELECT", "HOLD A", "B", "D-PAD / A"
        };
        constexpr const char* actions[] = {
            "AIM", "SET SPIN", "CHANGE HUD SIZE", "CYCLE POCKET LINES", "SWITCH CAMERAS",
            "RELEASE TO SHOOT", "CANCEL / RECALL 8", "MOVE / PLACE CUE"
        };
        for(int row = 0; row < 8; ++row)
        {
            label(panel_text, 39, 51 + row * 9, buttons[row], mint_text, Align::LEFT);
            label(panel_text, 112, 51 + row * 9, actions[row], ivory_text, Align::LEFT);
        }
        label(panel_text, 120, 130, "A / B  BACK", gold_text);
    }

    void pause(bool linked, bool full_hud_enabled, int lines, int selected)
    {
        open_panel(true);
        label(panel_text, 120, 47, "MATCH PAUSED", gold_text);
        label(panel_text, 120, 68, "A  RESUME", ivory_text);
        label(panel_text, 120, 92, linked ? "B  LEAVE MATCH" : "B  CONCEDE", ivory_text);
        label(panel_text, 120, 110, full_hud_enabled ? "UP / DOWN  FULL HUD" : "UP / DOWN  COMPACT HUD", mint_text);
        if(lines)
        {
            label(panel_text, 120, 120, selected >= 0 ? bn::format<24>("L / R  LINE {} / {}", selected + 1, lines) :
                  bn::string<24>("L / R  POCKET LINES"), mint_text);
        }
        label(panel_text, 120, 131, "SELECT  CONTROLS", muted_text);
    }

    void concession(bool linked)
    {
        open_panel();
        label(panel_text, 120, 34, linked ? "LEAVE LINK MATCH" : "CONCEDE MATCH", gold_text);
        label(panel_text, 120, 80, linked ? "THIS ENDS THE MATCH FOR BOTH PLAYERS" : "THIS COUNTS AS A RANKED LOSS");
        panel_actions(linked ? "A  LEAVE" : "A  CONCEDE", "B  BACK");
    }

    void link_status(const char* heading, const char* first, const char* second, const char* action)
    {
        open_panel();
        label(panel_text, 120, 34, heading, gold_text);
        bool has_detail = second[0] != '\0';
        label(panel_text, 120, has_detail ? 70 : 80, first);
        if(has_detail)
        {
            label(panel_text, 120, 86, second, muted_text);
        }
        if(action[0] != '\0')
        {
            label(panel_text, 120, 130, action, mint_text);
        }
    }

    void link_result(bool won, const char* reason)
    {
        link_status(won ? "YOU WIN" : "YOU LOSE", reason, "LINK CABLE MATCH", "A / B  LOBBY");
    }

    void result(bool won, const char* reason, const Profile& profile, int change, int new_rival)
    {
        open_panel();
        label(panel_text, 120, 34, won ? "YOU WIN" : "YOU LOSE", gold_text);
        label(panel_text, 120, 66, reason, muted_text);
        label(panel_text, 120, 81,
              bn::format<48>("RATING {}  {}{}", profile.rating, change >= 0 ? "+" : "-", change >= 0 ? change : -change),
              mint_text);
        if(new_rival >= 0)
        {
            label(panel_text, 120, 97,
                  bn::format<48>("UNLOCKED  {} / {}", rivals[new_rival].name, rivals[new_rival].rank));
        }
        else
        {
            label(panel_text, 120, 97, bn::format<48>("{} W / {} L   BEST {}", profile.wins, profile.losses, profile.best));
        }
        panel_actions("A  NEXT MATCH", "B  LOBBY");
    }

    void clear_panel()
    {
        panel_text.clear();
        panel.reset();
    }

    void scoreboard(const EightBall& game, const Profile& profile, const Rival& rival, Phase phase,
                    int choices, int selected, int link_player, int call, int cursor_pocket, bool full, CueSpin spin)
    {
        if(! match_hud || full != full_hud)
        {
            match_hud.reset();
            const auto& item = full ? bn::regular_bg_items::match_hud : bn::regular_bg_items::match_hud_compact;
            match_hud = item.create_bg(8, 48);
            match_hud->set_priority(0);
            full_hud = full;
        }
        hud.clear();
        footer.clear();
        bool linked = link_player >= 0;
        int local = linked ? link_player : 0;
        int ball_y = full ? 20 : 9;
        int card_left = game.player ? 148 : 3;
        for(int side = 0; side < 2; ++side)
        {
            bn::sprite_ptr& edge = active_border[side];
            edge.set_tiles(bn::sprite_items::active_player.tiles_item(), (full ? 0 : 2) + side);
            position(edge, card_left + 32 + 45 * side, 18);
            edge.set_visible(true);
        }

        for(int player = 0; player < 2; ++player)
        {
            int center = player ? 192 : 47;
            if(full)
            {
                int left = center - 44;
                avatars[player].set_tiles(bn::sprite_items::avatars.tiles_item(), linked ? player : player ? 2 : 0);
                position(avatars[player], left + 8, 8);
                const auto& name_color = player == game.player ? gold_text : ivory_text;
                const char* name = player == local ? "YOU" : linked ? "FRIEND" : rival.name;
                label(hud, left + 15, 8, name, name_color, Align::LEFT);
                label(hud, center + 39, 8, linked ? bn::format<12>("GBA {}", player + 1) :
                      bn::format<12>("{}", player ? rival.rating : profile.rating), mint_text, Align::RIGHT);
            }
            avatars[player].set_visible(full);

            // Before assignment the two rows show the two groups still on the table.
            Group group = game.groups[player];
            bool assigned = group != Group::open;
            if(! assigned)
            {
                group = player ? Group::stripes : Group::solids;
            }
            int count = game.remaining(group);
            bool on_eight = assigned && count == 0;
            int first = group == Group::stripes ? 9 : 1;
            int visible = 0;
            for(int index = 0; index < 7; ++index)
            {
                bn::sprite_ptr& icon = group_balls[player * 7 + index];
                int id = first + index;
                bool show = on_eight ? index == 0 : game.table.balls[id].live;
                if(show)
                {
                    icon.set_item(*ball_items[on_eight ? 8 : id]);
                    int offset = on_eight ? 0 : (2 * visible - (count - 1)) * 6;
                    position(icon, center + offset, ball_y);
                    ++visible;
                }
                icon.set_visible(show);
            }
        }

        for(int i = 0; i < meter_sections; ++i)
        {
            position(bar[i], 104 + i * 8, ball_y);
        }
        show_power(true);
        bool show_spin = phase == Phase::aim || phase == Phase::charge;
        constexpr int spin_x = 226;
        constexpr int spin_y = 148;
        position(spin_ball, spin_x, spin_y);
        position(spin_dot, spin_x + spin.side, spin_y - spin.vertical);
        spin_ball.set_visible(show_spin);
        spin_dot.set_visible(show_spin);
        if(full)
        {
            label(hud, 120, 8, "POWER", muted_text);
        }

        if(phase == Phase::call)
        {
            status(bn::format<32>("CALL: {}", pocket_names[cursor_pocket]), "D-PAD CYCLE / A CALL");
        }
        else if(phase == Phase::place)
        {
            status(foul_label(game.notice), "D-PAD MOVE / A PLACE");
        }
        else if(full)
        {
            if(phase == Phase::remote || phase == Phase::link_send)
            {
                status(phase == Phase::link_send ? "SYNCING SHOT" :
                       game.ball_in_hand ? "FRIEND HAS BALL IN HAND" : "FRIEND IS AIMING");
            }
            else if(phase == Phase::cpu_wait || phase == Phase::cpu_aim)
            {
                status(bn::format<32>("{} {}", rival.name, game.breaking ? "BREAKS" : "IS AIMING"),
                       game.ball_in_hand ? "BALL IN HAND" : "");
            }
            else if(phase == Phase::rolling)
            {
                status(game.player == local ? bn::string<32>("YOUR SHOT") :
                       bn::format<32>("{} SHOT", linked ? "FRIEND" : rival.name));
            }
            else if(call >= 0)
            {
                status(bn::format<32>("CALLED {}", pocket_names[call]),
                       phase == Phase::charge ? "RELEASE A / B BACK" : "B CHANGE POCKET");
            }
            else if(selected >= 0)
            {
                status(bn::format<24>("POCKET LINE {} / {}", selected + 1, choices),
                       phase == Phase::charge ? "RELEASE A / B BACK" : "PAUSE  POCKET LINES");
            }
            else
            {
                const char* message = game.notice == Notice::spotted ? "8 BALL RESPOTTED" :
                                      game.breaking ? "YOUR BREAK" :
                                      game.legal_target(8) ? "SINK THE 8 BALL" :
                                      game.groups[local] == Group::open ? "OPEN TABLE" : "YOUR TURN";
                status(message, phase == Phase::charge ? "RELEASE A / B BACK" : "");
            }
        }
    }

    bool update_camera(Vec cue, int angle, bool use_close_view, int pocket, bool hold_camera)
    {
        if(use_close_view != close_view)
        {
            close_view = use_close_view;
            camera_angle = -1;
            staged_angle = -1;
            camera_held = false;
            staged_settle = false;
            settle_steps = 0;
            line.clear();
            configure_projection(use_close_view);
            if(! use_close_view)
            {
                aim_table.hide();
                aim_backdrop->set_visible(false);
                background->set_visible(true);
            }
            else
            {
                line.close();
                aim_table.show();
                stick.set_visible(false);
                close_stick.set_visible(false);
                close_tail.set_visible(false);
                called.set_visible(false);
            }
        }
        if(! use_close_view)
        {
            highlight_overhead_pocket(pocket);
            return true;
        }

        if(hold_camera && camera_angle >= 0)
        {
            // Keep the last displayed pose throughout the shot and opponent's turn.
            staged_angle = -1;
            staged_settle = false;
            settle_steps = 0;
            camera_held = true;
            return true;
        }
        if(! hold_camera && camera_held && camera_angle >= 0)
        {
            settle_steps = 8;
        }
        camera_held = hold_camera;

        // Stage the deck and its details on separate ticks, then commit the complete frame.
        if(staged_angle >= 0)
        {
            if(! staged_finished)
            {
                aim_table.finish(staged_camera);
                staged_finished = true;
                return false;
            }
            camera = staged_camera;
            camera_anchor = staged_anchor;
            camera_angle = staged_angle;
            ++camera_revision;
            staged_angle = -1;
            staged_settle = false;
            background->set_visible(false);
            aim_backdrop->set_visible(true);
            aim_table.commit();
            return true;
        }

        if(angle == camera_angle && cue.x == camera_anchor.x && cue.y == camera_anchor.y)
        {
            settle_steps = 0;
            return true;
        }
        staged_settle = settle_steps > 0;
        if(staged_settle)
        {
            int delta = angle_delta(angle, camera_angle);
            angle = (camera_angle + delta / settle_steps) & (angle_count - 1);
            cue = camera_anchor + Vec{ (cue.x - camera_anchor.x) / settle_steps,
                                       (cue.y - camera_anchor.y) / settle_steps };
            --settle_steps;
        }
        staged_finished = false;
        staged_camera = AimCamera(cue, angle);
        staged_angle = angle;
        staged_anchor = cue;
        aim_table.draw(staged_camera);
        return false;
    }

    int displayed_angle() const { return camera_angle; }
    bool camera_settling() const { return staged_settle || settle_steps > 0; }

    int limit_aim_angle(int target) const
    {
        if(! close_view || camera_angle < 0 || camera_settling())
        {
            return target;
        }
        // Keep at most one small turn queued ahead of the pose currently being rendered.
        constexpr int max_pose_turn = 7;
        int base = staged_angle >= 0 ? staged_angle : camera_angle;
        return (base + std::clamp(angle_delta(target, base), -max_pose_turn, max_pose_turn)) & (angle_count - 1);
    }

    BN_CODE_IWRAM void show_balls(const Table& table, bool rolling, const Vec* placement, CueBallTint tint)
    {
        bool invalid = tint != CueBallTint::ivory;
        if(tint != cue_tint)
        {
            if(invalid != (cue_tint != CueBallTint::ivory))
            {
                balls[0].set_z_order(invalid ? -1 : 0);
                balls[0].set_palette(invalid ? placement_flash_palette : ball_items[0]->palette_item());
                depth_order_count = -1;
            }
            if(invalid)
            {
                bn::sprite_palette_ptr palette = balls[0].palette();
                palette.set_colors(tint == CueBallTint::bright_red ? placement_flash_palette : placement_red_palette);
            }
            cue_tint = tint;
        }

        int angle = close_view ? camera_angle : -1;
        Vec heading = close_view ? direction(angle) : Vec{ 0, -direction_unit };
        Vec screen_right{ -heading.y, heading.x };
        CameraPitch pitch = close_view ? camera.pitch() : overview_pitch;
        int far_to_near[16];
        int visible_count = 0;
        uint32_t pixels[sphere_words];
        for(int id = 0; id < 16; ++id)
        {
            const Ball& ball = table.balls[id];
            BallVisual& visual = sphere_pose[id];
            bool show = false;
            bool on_table = ball.live || (id == 0 && placement);
            if(on_table)
            {
                Vec point = id == 0 && placement ? *placement : ball.pos;
                Vec displacement = point - visual.previous;
                bool changed = displacement.x || displacement.y;
                if(rolling && visual.shown && changed)
                {
                    visual.spin.roll(displacement);
                }
                bool centered = close_view && id == 0 &&
                                point.x == camera_anchor.x && point.y == camera_anchor.y;
                bool camera_changed = camera_revision != visual.camera_revision && (! centered || ! visual.centered);
                if(! visual.shown || changed || camera_changed || centered != visual.centered)
                {
                    visual.centered = centered;
                    visual.projection = close_view ? camera.sphere(point) : project_sphere(point);
                    visual.previous = point;
                    visual.camera_revision = camera_revision;
                    if(visual.projection.visible())
                    {
                        Vec pixel_center = visual.projection.pixel_center();
                        position(balls[id], pixel_center.x, pixel_center.y);
                        uint16_t next_tiles = visual.spin.draw(id, visual.projection, screen_right, pitch, pixels);
                        unsigned tiles_to_update = next_tiles | visual.drawn_tiles;
                        bool reload = false;
                        while(tiles_to_update)
                        {
                            int tile = __builtin_ctz(tiles_to_update);
                            tiles_to_update &= tiles_to_update - 1;
                            bn::tile& destination = sphere_tiles[id][tile];
                            const uint32_t* source = pixels + tile * 8;
                            bool used = next_tiles & (1u << tile);
                            for(int row = 0; row < 8; ++row)
                            {
                                uint32_t word = used ? source[row] : 0;
                                if(destination.data[row] != word)
                                {
                                    destination.data[row] = word;
                                    reload = true;
                                }
                            }
                        }
                        visual.drawn_tiles = next_tiles;
                        if(reload)
                        {
                            bn::sprite_tiles_ptr tiles = balls[id].tiles();
                            tiles.reload_tiles_ref();
                        }

                        if(close_view)
                        {
                            int zoom = std::clamp((visual.projection.radius_x * 5 + 3 * unit / 2) / (3 * unit) - 2, 0, 7);
                            if(zoom != visual.zoom)
                            {
                                shadows[id].set_affine_mat(scales[zoom]);
                                visual.zoom = zoom;
                            }
                        }
                        // Keep the contact patch under the sphere's lower edge from either camera.
                        Vec ground = close_view ? camera.screen(camera.local(point)) : project(point);
                        const auto& sphere = visual.projection;
                        ground.y = std::clamp(ground.y, sphere.center.y + sphere.radius_y * 2 / 3,
                                             sphere.center.y + sphere.radius_y);
                        visual.shadow_visible = shadow_fits_on_cloth(point) && inside_view(ground, 2);
                        position(shadows[id], ground);
                    }
                }
                show = visual.projection.visible();
                if(show)
                {
                    int rank = visible_count++;
                    while(rank && sphere_pose[far_to_near[rank - 1]].projection.distance_squared < visual.projection.distance_squared)
                    {
                        far_to_near[rank] = far_to_near[rank - 1];
                        --rank;
                    }
                    far_to_near[rank] = id;
                }
            }
            visual.shown = on_table;
            balls[id].set_visible(show);
            shadows[id].set_visible(show && visual.shadow_visible && (id != 0 || ! invalid));
        }

        if(visible_count != depth_order_count ||
           std::memcmp(depth_order, far_to_near, visible_count * sizeof(int)) != 0)
        {
            for(int rank = 0; rank < visible_count; ++rank)
            {
                balls[far_to_near[rank]].put_above();
            }
            std::copy_n(far_to_near, visible_count, depth_order);
            depth_order_count = visible_count;
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

    void clear_aim()
    {
        stick.set_visible(false);
        close_stick.set_visible(false);
        close_tail.set_visible(false);
        line.clear();
    }

    void draw_cue(const Table& table, int angle, int pullback)
    {
        if(close_view)
        {
            stick.set_visible(false);
            int tip_row = camera.cue_tip_row((7 + pullback) * unit);
            int length = std::clamp(view_bottom - tip_row, 1, 64);
            if(length != close_stick_length)
            {
                close_stick_length = length;
                close_stick.set_tiles(bn::sprite_items::close_cue.tiles_item(), std::min(length, 32) - 1);
                if(length > 32)
                {
                    close_tail.set_tiles(bn::sprite_items::close_cue.tiles_item(), length - 1);
                }
            }
            position(close_stick, 120, tip_row + 16);
            position(close_tail, 120, tip_row + 48);
            close_stick.set_visible(true);
            close_tail.set_visible(length > 32);
            return;
        }

        close_stick.set_visible(false);
        close_tail.set_visible(false);
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

    void trajectory(const EightBall& game, int angle)
    {
        const Table& table = game.table;
        Vec cue = table.balls[0].pos;
        Vec heading = direction(angle);
        Ray first = table.trace(cue, heading, 0);
        SphereProjection cue_ball = project_sphere(cue);
        Vec ghost_center = project_sphere(first.end).center;
        ghost_center = { (ghost_center.x & -unit) + unit / 2, (ghost_center.y & -unit) + unit / 2 };
        bool show_ghost = first.pocket < 0 && (first.ball >= 0 || first.rail) &&
                          length(ghost_center - cue_ball.center) >= cue_ball.radius_x + ghost_ball_radius - unit / 2 &&
                          ghost_center.x >= ghost_ball_radius && ghost_center.x <= 240 * unit - ghost_ball_radius &&
                          ghost_center.y >= ghost_ball_radius && ghost_center.y <= view_bottom * unit - ghost_ball_radius;
        if(show_ghost)
        {
            SphereProjection nearby[15];
            int count = 0;
            for(int id = 1; id < 16; ++id)
            {
                const Ball& ball = table.balls[id];
                if(ball.live && length(ball.pos - first.end) <= 12 * unit)
                {
                    nearby[count++] = project_sphere(ball.pos);
                }
            }
            // The ball sprites can occlude a little of the ring and still leave it readable.
            show_ghost = ! count || ShotLine::ghost_visible(ghost_center, nearby, count);
        }

        Vec end = first.end;
        if(! show_ghost && first.ball >= 0)
        {
            Vec offset = table.balls[first.ball].pos - end;
            int across = (offset.y * heading.x - offset.x * heading.y) / direction_unit;
            if(std::abs(across) < ball_height)
            {
                // Keep the line on the shot axis and let the sprite cover it at contact.
                int along = (offset.x * heading.x + offset.y * heading.y) / direction_unit;
                end += travel(heading, along);
            }
        }
        LineInk ink = first.ball >= 0 && ! game.legal_target(first.ball) ? LineInk::warning : LineInk::cue;
        aim_table.close();
        line.show();
        line.begin();
        if(show_ghost)
        {
            line.draw_to_ghost(cue_ball.center, ghost_center, ink);
        }
        else
        {
            line.draw(cue_ball.center, project(end, ball_height), ink);
        }
        line.commit();
    }

private:
    bn::optional<bn::regular_bg_ptr> background;
    bn::optional<bn::regular_bg_ptr> panel;
    bool pause_panel_open = false;
    bn::optional<bn::regular_bg_ptr> match_hud;
    bn::optional<bn::regular_bg_ptr> aim_backdrop;
    bn::sprite_text_generator text{ hud_font };
    bn::vector<bn::sprite_ptr, 16> hud;
    bn::vector<bn::sprite_ptr, 16> footer;
    bn::vector<bn::sprite_ptr, 48> panel_text;
    bn::vector<bn::sprite_ptr, 16> balls;
    bn::vector<bn::sprite_ptr, 16> shadows;
    BallVisual sphere_pose[16];
    int depth_order[16];
    int depth_order_count = -1;
    bn::vector<bn::sprite_ptr, 14> group_balls;
    ShotLine line;
    AimTable aim_table;
    AimCamera camera;
    AimCamera staged_camera;
    Vec camera_anchor;
    Vec staged_anchor;
    int camera_angle = -1;
    int staged_angle = -1;
    int settle_steps = 0;
    bool camera_held = false;
    bool staged_settle = false;
    bool full_hud = true;
    bool close_view = false;
    bool staged_finished = false;
    unsigned camera_revision = 0;
    bn::vector<bn::sprite_affine_mat_ptr, 8> scales;
    bn::vector<bn::sprite_ptr, meter_sections> bar;
    bn::vector<bn::sprite_ptr, 2> avatars;
    bn::sprite_ptr stick = make_sprite(bn::sprite_items::cue, -2);
    bn::sprite_ptr close_stick = make_sprite(bn::sprite_items::close_cue, -2);
    bn::sprite_ptr close_tail = make_sprite(bn::sprite_items::close_cue, -2);
    int close_stick_length = 1;
    int overhead_pocket = -1;
    bn::sprite_ptr called = make_sprite(bn::sprite_items::called_pockets, 4);
    CueBallTint cue_tint = CueBallTint::ivory;
    bn::sprite_ptr active_border[2] = {
        make_sprite(bn::sprite_items::active_player, 0), make_sprite(bn::sprite_items::active_player, 0, 1)
    };
    bn::sprite_ptr spin_ball = make_sprite(bn::sprite_items::spin_ball, 0);
    bn::sprite_ptr spin_dot = make_sprite(bn::sprite_items::spin_dot, -1);
    int bar_filled = 0;


    static bn::sprite_ptr make_sprite(const bn::sprite_item& item, int z, int index = 0)
    {
        bn::sprite_ptr result = item.create_sprite(0, 0, index);
        result.set_bg_priority(2);
        result.set_z_order(z);
        result.set_visible(false);
        return result;
    }

    void label(bn::ivector<bn::sprite_ptr>& target, int x, int y, const bn::string_view& value,
               const bn::sprite_palette_item& palette = ivory_text, Align align = Align::CENTER)
    {
        text.set_palette_item(palette);
        text.set_alignment(align);
        bool on_panel = &target == &panel_text || bool(match_hud);
        text.set_bg_priority(on_panel ? 0 : 2);
        text.generate(x - 120, y - 80, value, target);
    }

    void panel_actions(const char* primary, const char* secondary)
    {
        label(panel_text, 75, 130, primary, gold_text);
        label(panel_text, 165, 130, secondary, mint_text);
    }

    void status(const bn::string_view& message, const bn::string_view& action = {})
    {
        int action_x = spin_ball.visible() ? 211 : 232;
        text.set_z_order(-3);
        label(footer, 9, 152, message, shadow_text, Align::LEFT);
        if(! action.empty())
        {
            label(footer, action_x + 1, 152, action, shadow_text, Align::RIGHT);
        }
        text.set_z_order(-4);
        label(footer, 8, 151, message, gold_text, Align::LEFT);
        if(! action.empty())
        {
            label(footer, action_x, 151, action, mint_text, Align::RIGHT);
        }
        text.set_z_order(0);
    }

    void open_panel(bool pause_layout = false)
    {
        panel_text.clear();
        if(aim_backdrop)
        {
            hide_match_hud();
        }
        hide_playfield();
        if(! panel || pause_panel_open != pause_layout)
        {
            panel.reset();
            if(aim_backdrop)
            {
                line.close();
            }
            const auto& item = pause_layout ? bn::regular_bg_items::pause_panel : bn::regular_bg_items::panel;
            panel = item.create_bg(8, 48);
            panel->set_priority(1);
            pause_panel_open = pause_layout;
        }
    }

    void configure_projection(bool aiming)
    {
        ++camera_revision;
        depth_order_count = -1;
        for(int id = 0; id < 16; ++id)
        {
            sphere_pose[id].centered = false;
            balls[id].set_bg_priority(aiming ? 1 : 2);
            shadows[id].set_bg_priority(aiming ? 1 : 2);
            sphere_pose[id].zoom = -1;
            if(! aiming)
            {
                shadows[id].remove_affine_mat();
            }
        }
    }

    static bool inside_view(Vec point, int margin)
    {
        return point.x >= -margin * unit && point.x <= (239 + margin) * unit &&
               point.y >= (view_top + margin) * unit && point.y <= (view_bottom - margin) * unit;
    }

    void highlight_overhead_pocket(int pocket)
    {
        if(pocket >= 0)
        {
            if(pocket != overhead_pocket)
            {
                called.set_tiles(bn::sprite_items::called_pockets.tiles_item(), pocket);
                position(called, overview_pocket_centers[pocket][0], overview_pocket_centers[pocket][1]);
                overhead_pocket = pocket;
            }
        }
        called.set_visible(pocket >= 0);
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
        called.set_visible(false);
        depth_order_count = -1;
        for(bn::sprite_ptr& ball : balls)
        {
            ball.set_visible(false);
        }
        for(bn::sprite_ptr& shadow : shadows)
        {
            shadow.set_visible(false);
        }
    }

    void hide_sprites()
    {
        hide_playfield();
        hide_match_hud();
    }

    void hide_match_hud()
    {
        match_hud.reset();
        hud.clear();
        footer.clear();
        for(bn::sprite_ptr& ball : group_balls)
        {
            ball.set_visible(false);
        }
        for(bn::sprite_ptr& avatar : avatars)
        {
            avatar.set_visible(false);
        }
        for(bn::sprite_ptr& edge : active_border)
        {
            edge.set_visible(false);
        }
        spin_ball.set_visible(false);
        spin_dot.set_visible(false);
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
                    show_pause();
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
                else if(bn::keypad::up_pressed() || bn::keypad::down_pressed())
                {
                    full_match_hud = ! full_match_hud;
                    show_pause();
                }
                else if(phase == Phase::aim && ! finding_lines && suggestions.size() &&
                        (bn::keypad::l_pressed() || bn::keypad::r_pressed()))
                {
                    int change = int(bn::keypad::r_pressed()) - int(bn::keypad::l_pressed());
                    if(change)
                    {
                        cycle_pocket_line(change);
                        show_pause();
                    }
                }
                else if(bn::keypad::select_pressed())
                {
                    screen = Screen::pause_help;
                    view.controls(true);
                }
                break;
            case Screen::pause_help:
                if(bn::keypad::a_pressed() || bn::keypad::b_pressed() || bn::keypad::select_pressed() ||
                   bn::keypad::start_pressed())
                {
                    screen = Screen::pause;
                    show_pause();
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
                    show_pause();
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
    Vec shot_origin;
    ShotPlanner planner;
    PocketSuggestions suggestions;
    Plan cpu_plan;
    uint32_t seed = 0x51482031u;
    unsigned frame = 0;
    int angle = 0;
    int last_guide = -1;
    bool prefer_close_view = true;
    bool full_match_hud = true;
    int last_pullback = 0;
    int aim_power = min_speed;
    int selected_line = -1;
    int called_pocket = -1;
    int call_cursor = 0;
    CueSpin spin;
    bool finding_lines = false;
    int aim_hold = 0;
    int previous_aim_input = 0;
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
        view.show_room(prefer_close_view);
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
            view.result(won, reason, records.profile(), change, unlocked > match_rival ? unlocked : -1);
        }
        bn::sound_items::confirm.play(bn::fixed(0.55));
    }

    void show_pause()
    {
        int lines = phase == Phase::aim && ! finding_lines ? suggestions.size() : 0;
        view.pause(linked(), full_match_hud, lines, selected_line);
    }

    void score()
    {
        view.scoreboard(game, records.profile(), rival(), phase, finding_lines ? 0 : suggestions.size(), selected_line,
                        linked() ? cable.player() : -1, called_pocket, call_cursor, full_match_hud, spin);
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
        spin = {};
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
        finding_lines = ! game.breaking;
        suggestions.start(called_pocket);
        set_phase(Phase::aim);
    }

    void call_input()
    {
        int horizontal = int(bn::keypad::right_pressed()) - int(bn::keypad::left_pressed());
        int vertical = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());
        int next = move_pocket_call(call_cursor, horizontal, vertical);
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

    void update_suggestions()
    {
        if(finding_lines && suggestions.advance(game))
        {
            finding_lines = false;
            score();
        }
    }

    void cycle_pocket_line(int change)
    {
        int count = suggestions.size();
        selected_line = selected_line < 0 ? 0 : (selected_line + change + count) % count;
        const Plan& choice = suggestions[selected_line];
        angle = choice.angle;
        aim_power = choice.speed;
        spin = {};
    }

    void spin_input()
    {
        int horizontal = int(bn::keypad::right_pressed()) - int(bn::keypad::left_pressed());
        int vertical = int(bn::keypad::up_pressed()) - int(bn::keypad::down_pressed());
        CueSpin next{ std::clamp(spin.side + horizontal, -max_spin, max_spin),
                      std::clamp(spin.vertical + vertical, -max_spin, max_spin) };
        if(next.side != spin.side || next.vertical != spin.vertical)
        {
            spin = next;
            selected_line = -1;
            score();
        }
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
            cable.send_shot(turn_index, { angle, speed, point.x / unit, point.y / unit, called_pocket, spin });
        }
        strike(speed, called_pocket, spin);
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
        if(shot.speed < min_speed || shot.speed > max_speed || ! valid_call || std::abs(shot.spin.side) > max_spin ||
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
        strike(shot.speed, shot.pocket, shot.spin);
    }

    void strike(int speed, int pocket, CueSpin shot_spin = {})
    {
        shot_origin = game.table.balls[0].pos;
        game.table.strike(direction(angle), speed, pocket, shot_spin);
        charge = speed;
        last_pots = 0;
        sound_cooldown = 0;
        set_phase(Phase::rolling);
        bn::sound_items::strike.play(bn::fixed(0.65));
    }

    void steer()
    {
        int input = int(bn::keypad::r_held()) - int(bn::keypad::l_held());
        if(input != previous_aim_input)
        {
            aim_hold = 0;
            previous_aim_input = input;
        }
        if(input)
        {
            int amount = aim_step(aim_hold);
            angle = view.limit_aim_angle((angle + input * amount) & (angle_count - 1));
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
        if(bn::keypad::a_pressed())
        {
            if(! game.table.can_place(placement))
            {
                bn::sound_items::reject.play(bn::fixed(0.45));
                return;
            }
            // Link packets use whole-pixel positions. The placement preview moves on that grid.
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
        if(phase != Phase::place && phase != Phase::call && bn::keypad::select_pressed())
        {
            prefer_close_view = ! prefer_close_view;
            last_guide = -1;
        }
        switch(phase)
        {
            case Phase::aim:
                update_suggestions();
                steer();
                spin_input();
                if(called_pocket >= 0 && bn::keypad::b_pressed())
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
                    if(prefer_close_view && last_guide >= 0)
                    {
                        angle = last_guide;
                    }
                    local_shot(charge);
                }
                else
                {
                    steer();
                    spin_input();
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
        bool use_close_view = ! placing && phase != Phase::call && prefer_close_view;
        Vec focus = game.table.balls[0].pos;
        int heading = angle;
        if(placing)
        {
            view.clear_aim();
        }
        else if(use_close_view)
        {
            switch(phase)
            {
                case Phase::rolling:
                    focus = shot_origin;
                    break;
                case Phase::cpu_wait:
                case Phase::remote:
                    heading = direction_angle(px(120, 84) - focus);
                    break;
                default:
                    break;
            }
        }
        bool show_charge = phase == Phase::charge || (phase == Phase::rolling && game.player == local_player());
        view.power(show_charge ? charge : 0);
        int pocket = phase == Phase::call ? call_cursor :
                     phase == Phase::cpu_aim ? cpu_plan.pocket :
                     phase == Phase::rolling ? game.table.shot.called_pocket :
                     phase == Phase::aim || phase == Phase::charge ? called_pocket : -1;
        bool hold_camera = phase == Phase::rolling || phase == Phase::cpu_wait || phase == Phase::cpu_aim ||
                           phase == Phase::remote || phase == Phase::link_send;
        if(! view.update_camera(focus, heading, use_close_view, pocket, hold_camera))
        {
            return;
        }
        CueBallTint tint = CueBallTint::ivory;
        if(placing && ! game.table.can_place(placement))
        {
            tint = (timer / 10) % 2 ? CueBallTint::red : CueBallTint::bright_red;
        }
        view.show_balls(game.table, phase == Phase::rolling, placing ? &placement : nullptr, tint);
        switch(phase)
        {
            case Phase::aim:
            case Phase::charge:
            {
                if(view.camera_settling())
                {
                    view.clear_aim();
                    break;
                }
                int pullback = 0;
                if(phase == Phase::charge)
                {
                    int travel_range = use_close_view ? 11 : 26;
                    int remaining = max_speed - aim_power;
                    pullback = 1 + (remaining ? (charge - aim_power) * travel_range / remaining :
                                                std::min(timer, 32) * travel_range / 32);
                }
                int displayed = use_close_view ? view.displayed_angle() : angle;
                bool aim_changed = displayed != last_guide;
                if(aim_changed)
                {
                    if(! use_close_view)
                    {
                        view.trajectory(game, displayed);
                    }
                    last_guide = displayed;
                }
                if(aim_changed || pullback != last_pullback)
                {
                    view.draw_cue(game.table, displayed, pullback);
                    last_pullback = pullback;
                }
                break;
            }
            case Phase::call:
                view.clear_aim();
                break;
            case Phase::cpu_aim:
                view.clear_aim();
                if(! use_close_view)
                {
                    view.draw_cue(game.table, angle, std::clamp((timer - 12) / 2, 0, 12));
                }
                break;
            case Phase::rolling:
                view.clear_aim();
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
    BN_DATA_EWRAM_BSS static Game game;
    while(true)
    {
        game.tick();
        bn::core::update();
    }
}
