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
#include "bn_sprite_items_cosmetic_avatars.h"
#include "bn_sprite_items_wardrobe_portraits.h"
#include "bn_sprite_items_cue.h"
#include "bn_sprite_items_cue_previews.h"
#include "bn_sprite_items_gear_frame.h"
#include "bn_sprite_items_close_cue.h"
#include "bn_sprite_items_hud_font.h"
#include "bn_sprite_items_marks.h"
#include "bn_sprite_items_meter.h"
#include "bn_sprite_items_rival_portraits.h"
#include "bn_sprite_items_rival_icons.h"
#include "bn_sprite_items_finn_portrait.h"
#include "bn_sprite_items_finn_icon.h"
#include "bn_sprite_items_spin_ball.h"
#include "bn_sprite_items_spin_dot.h"
#include "bn_sprite_items_taunt_chip.h"
#include "bn_sprite_items_active_player.h"
#include "bn_sprite_items_called_pockets.h"

#include "font_widths.h"
#include "pool.h"
#include "pool_career.h"
#include "pool_banter.h"
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

constexpr int matches_per_page = 5;

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
constexpr int cue_length = 64;
constexpr int break_power = 340;

constexpr const char* pocket_names[] = {
    "TOP LEFT", "TOP MIDDLE", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM MIDDLE", "BOTTOM RIGHT"
};
enum class Screen { lobby, roster, record, gear, collection, match, help, pause, pause_help, concede, result, link_wait, link_lost, link_leaving };
enum class RecordPage { rating, history, badges };

enum class Phase { aim, charge, call, place, rolling, cpu_wait, cpu_aim, remote, link_send };

struct CareerSlot
{
    static constexpr uint32_t id = 0x38435236;
    uint32_t magic = id;
    uint32_t sequence = 0;
    Career career;
    uint32_t checksum = 0;

    uint32_t calculate() const
    {
        uint32_t hash = 0x811c9dc5u;
        auto mix = [&](uint32_t value) { hash = (hash ^ value) * 0x01000193u; };
        const Profile& profile = career.profile;
        for(uint32_t value : { magic, sequence, uint32_t(profile.rating), uint32_t(profile.best),
                               uint32_t(profile.wins), uint32_t(profile.losses) })
        {
            mix(value);
        }
        for(unsigned value : { unsigned(career.defeated), unsigned(career.equipped), unsigned(career.count),
                              unsigned(career.next), unsigned(career.reserved), unsigned(career.outfit),
                              unsigned(career.streak), unsigned(career.best_streak), unsigned(career.challenges) })
        {
            mix(value);
        }
        for(uint8_t wins : career.wins_by_rival)
        {
            mix(wins);
        }
        for(const CareerMatch& match : career.history)
        {
            mix(uint32_t(match.rating));
            mix(uint32_t(match.change));
            mix(match.rival);
            mix(match.won);
        }
        return hash;
    }

    bool valid() const
    {
        const Profile& profile = career.profile;
        if(magic != id || checksum != calculate() || career.reserved ||
           profile.rating < 100 || profile.rating > profile.best || profile.best > 2400 ||
           profile.wins < 0 || profile.wins > 9999 || profile.losses < 0 || profile.losses > 9999 ||
           career.count > career_history_capacity || career.next >= career_history_capacity ||
           (career.count < career_history_capacity && career.next != career.count) ||
           career.defeated >= (1 << rival_count) ||
           career.equipped >= cue_count || ! career.owns_cue(career.equipped) ||
           career.outfit > accessory_count || (career.outfit && ! career.owns_accessory(career.outfit - 1)) ||
           career.streak > career.best_streak || career.best_streak > profile.wins || career.challenges > 9999)
        {
            return false;
        }
        for(int age = 0; age < career.count; ++age)
        {
            const CareerMatch& match = career.recent(age);
            if(match.rating < 100 || match.rating > profile.best || match.rival >= rival_count || match.won > 1 ||
               match.change < -48 || match.change > 48)
            {
                return false;
            }
        }
        return true;
    }
};

static_assert(sizeof(CareerSlot) == 168);

class Records
{
public:
    Records()
    {
        for(int index = 0; index < 2; ++index)
        {
            CareerSlot slot;
            bn::sram::read_offset(slot, offset(index));
            if(slot.valid() && (active < 0 || slot.sequence > record.sequence))
            {
                active = index;
                record = slot;
            }
        }
    }

    const Profile& profile() const { return career().profile; }
    const Career& career() const { return record.career; }

    MatchReward add_result(bool won, int rival, int shots)
    {
        MatchReward reward = record.career.record(won, rival, shots);
        save();
        return reward;
    }

    bool collection_action(int item, bool clothing)
    {
        bool changed = clothing ? record.career.equip_accessory(item) : record.career.equip_cue(item);
        if(changed)
        {
            save();
        }
        return changed;
    }

private:
    CareerSlot record;
    int active = -1;

    static constexpr int offset(int index) { return 2048 + index * int(sizeof(CareerSlot)); }

    void save()
    {
        ++record.sequence;
        record.checksum = record.calculate();
        active = active == 0 ? 1 : 0;
        bn::sram::write_offset(record, offset(active));
    }
};

bn::array<bn::color, 16> cue_colors(int style)
{
    const auto& original = bn::sprite_items::cue.palette_item();
    bn::array<bn::color, 16> colors;
    std::copy_n(original.colors_ref().data(), 16, colors.data());
    if(style)
    {
        constexpr bn::color accents[] = {
            { 16, 21, 8 }, { 7, 22, 27 }, { 31, 13, 6 }, { 24, 11, 21 }, { 8, 24, 22 }
        };
        static_assert(sizeof(accents) / sizeof(*accents) == cue_count - 1);
        bn::color color = accents[style - 1];
        colors[3] = bn::color((color.red() * 2 + 9) / 4, (color.green() * 2 + 5) / 4,
                              (color.blue() * 2 + 3) / 4);
        colors[4] = color;
        colors[6] = bn::color((color.red() + 26) / 2, (color.green() + 20) / 2, (color.blue() + 13) / 2);
        colors[11] = color;
        colors[12] = bn::color(color.red() / 2, color.green() / 2, color.blue() / 2);
        if(style == 5)
        {
            colors[3] = color;
            colors[5] = bn::color(31, 29, 22);
            colors[6] = color;
            colors[7] = bn::color(23, 26, 23);
        }
    }
    return colors;
}

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

constexpr int aim_repeat_delay = 7;
constexpr int aim_ramp_interval = 4;
constexpr int max_aim_step = 8;
constexpr int aim_full_speed = aim_repeat_delay + (max_aim_step - 1) * aim_ramp_interval;

int aim_step(int held_frames)
{
    if(held_frames == 0)
    {
        return 1;
    }
    if(held_frames < aim_repeat_delay)
    {
        return 0;
    }
    return 1 + (held_frames - aim_repeat_delay) / aim_ramp_interval;
}

class DirectionRepeat
{
public:
    int step(int held)
    {
        if(held != direction)
        {
            direction = held;
            countdown = 12;
            return direction;
        }
        if(direction && --countdown == 0)
        {
            countdown = 5;
            return direction;
        }
        return 0;
    }

private:
    int direction = 0;
    int countdown = 0;
};

constexpr int angle_delta(int target, int current)
{
    constexpr int half_turn = angle_count / 2;
    return ((target - current + half_turn) & (angle_count - 1)) - half_turn;
}

BN_DATA_EWRAM_BSS bn::tile sphere_tiles[16][sphere_words / 8];
constexpr int rating_plot_width = 168;
constexpr int rating_plot_left = 36;
constexpr int rating_plot_slices = (rating_plot_width + 63) / 64;
BN_DATA_EWRAM_BSS bn::tile rating_tiles[rating_plot_slices][64];
BN_DATA_EWRAM_BSS bn::tile cue_stat_tiles[32];

void rating_pixel(int x, int y, unsigned color)
{
    bn::tile& tile = rating_tiles[x >> 6][(y >> 3) * 8 + ((x & 63) >> 3)];
    int shift = (x & 7) * 4;
    uint32_t& row = tile.data[y & 7];
    row = (row & ~(15u << shift)) | (color << shift);
}

void rating_segment(Vec from, Vec to)
{
    int width = to.x - from.x;
    int rise = std::abs(to.y - from.y);
    int step = from.y < to.y ? 1 : -1;
    int error = width - rise;
    while(true)
    {
        rating_pixel(from.x, from.y, 8);
        if(from.x == to.x && from.y == to.y)
        {
            return;
        }
        int twice = 2 * error;
        if(twice > -rise)
        {
            error -= rise;
            ++from.x;
        }
        if(twice < width)
        {
            error += width;
            from.y += step;
        }
    }
}


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
        for(int index = 0; index < meter_sections; ++index)
        {
            bar.push_back(make_sprite(bn::sprite_items::meter, 1));
            bar.back().set_bg_priority(0);
        }
    }

    void show_lobby(const Career& career, int focus = 0)
    {
        clear_panel();
        line.close();
        aim_table.close();
        aim_backdrop.reset();
        close_view = false;
        background.reset();
        background = bn::regular_bg_items::lobby.create_bg(8, 48);
        background->set_priority(3);
        release_match_sprites();
        hud.clear();
        footer.clear();
        const Profile& profile = career.profile;
        const Rival& rival = rivals[career.rival_index()];
        label(hud, 52, 99, bn::format<16>("{}", profile.rating), mint_text, Align::LEFT);
        label(hud, 105, 99, "VS", muted_text);
        label(hud, 130, 99, rival.short_name);
        label(hud, 193, 99, bn::format<24>("{} W / {} L", profile.wins, profile.losses), muted_text);
        title_options(focus);
    }

    void title_options(int focus)
    {
        menu_text.clear();
        menu_option(64, 118, "RIVALS", focus == 0);
        menu_option(176, 118, "RECORD", focus == 1);
        menu_option(64, 137, "GEAR", focus == 2);
        menu_option(176, 137, "LINK CABLE", focus == 3);
        label(menu_text, 64, 151, "START  QUICK MATCH", muted_text);
        label(menu_text, 176, 151, "SELECT  CONTROLS", muted_text);
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
        release_match_sprites();
        hud.clear();
        footer.clear();
    }

    void show_roster(const Career& career, int index)
    {
        open_panel();
        const Rival& rival = rivals[index];
        bool unlocked = index <= career.rival_index();
        label(panel_text, 120, 34, "RIVALS", gold_text);
        label(panel_text, 203, 34, bn::format<16>("< {} / {} >", index + 1, rival_count), muted_text, Align::RIGHT);
        portrait = index == boss_rival ? make_sprite(bn::sprite_items::finn_portrait, 0) :
                   make_sprite(bn::sprite_items::rival_portraits, 0, index);
        portrait->set_bg_priority(0);
        position(*portrait, 64, 80);
        portrait->set_visible(true);

        label(panel_text, 104, 51, rival.name, gold_text, Align::LEFT);
        label(panel_text, 104, 63, bn::format<24>("{}  /  ELO {}", rival.rank, rival.rating), mint_text, Align::LEFT);
        bool defeated = career.owns(index);
        label(panel_text, 104, 78, defeated ? "REWARDS EARNED" : "FIRST WIN REWARDS", muted_text, Align::LEFT);
        label(panel_text, 104, 88, cues[index + 1].name, ivory_text, Align::LEFT);
        label(panel_text, 104, 98, accessories[index], ivory_text, Align::LEFT);
        if(defeated)
        {
            int wins = career.wins_by_rival[index];
            label(panel_text, 104, 111, wins >= 10 ? bn::string<24>("MASTERED") :
                  bn::format<24>("{} WINS", wins), mint_text, Align::LEFT);
        }
        else if(! unlocked)
        {
            label(panel_text, 104, 111, bn::format<24>("BEAT {} FIRST", rivals[index - 1].short_name),
                  muted_text, Align::LEFT);
        }
        if(unlocked)
        {
            menu_option(46, 130, "PLAY", true, Align::LEFT);
        }
        else
        {
            label(menu_text, 36, 130, "LOCKED", muted_text, Align::LEFT);
        }
        label(menu_text, 203, 130, "B TITLE", muted_text, Align::RIGHT);
    }

    void show_record(const Career& career, RecordPage page, int offset)
    {
        open_panel();
        label(panel_text, 120, 34, "RECORD", gold_text);
        menu_option(61, 49, "RATING", page == RecordPage::rating);
        menu_option(120, 49, "MATCHES", page == RecordPage::history);
        menu_option(179, 49, "BADGES", page == RecordPage::badges);
        if(page == RecordPage::history)
        {
            label(panel_text, 37, 63, "RIVAL", muted_text, Align::LEFT);
            label(panel_text, 114, 63, "RESULT", muted_text);
            label(panel_text, 157, 63, "ELO", muted_text, Align::RIGHT);
            label(panel_text, 203, 63, "CHANGE", muted_text, Align::RIGHT);
            if(! career.count)
            {
                label(panel_text, 120, 88, "NO MATCHES YET", muted_text);
            }
            for(int row = 0; row < matches_per_page && offset + row < career.count; ++row)
            {
                const CareerMatch& match = career.recent(offset + row);
                int y = 75 + row * 9;
                label(panel_text, 37, y, rivals[match.rival].short_name, ivory_text, Align::LEFT);
                label(panel_text, 114, y, match.won ? "WIN" : "LOSS", match.won ? mint_text : muted_text);
                label(panel_text, 157, y, bn::format<8>("{}", match.rating), ivory_text, Align::RIGHT);
                rating_change(191, 203, y, match.change);
            }
            if(career.count > matches_per_page)
            {
                label(menu_text, 120, 130, bn::format<24>("PAGE {} / {}", offset / matches_per_page + 1,
                      (career.count + matches_per_page - 1) / matches_per_page), muted_text);
            }
        }
        else if(page == RecordPage::rating)
        {
            label(panel_text, 120, 62, bn::format<32>("ELO {}   BEST {}", career.profile.rating,
                  career.profile.best), mint_text);
            show_rating_chart(career);
            label(menu_text, 120, 130, bn::format<20>("{} PLOTTED", career.count), muted_text);
        }
        else
        {
            show_badges(career);
        }
        label(menu_text, 36, 130, "L / R TABS", mint_text, Align::LEFT);
        label(menu_text, 203, 130, "B TITLE", muted_text, Align::RIGHT);
    }

    void show_loadout(const Career& career, int focus)
    {
        open_panel();
        label(panel_text, 120, 34, "YOUR GEAR", gold_text);
        constexpr int portrait_y = 82;
        for(int x : { 70, 170 })
        {
            loadout_frames.push_back(make_sprite(bn::sprite_items::gear_frame, 2));
            bn::sprite_ptr& frame = loadout_frames.back();
            frame.set_bg_priority(0);
            position(frame, x, portrait_y);
            frame.set_visible(true);
        }

        show_gear_icon(career.equipped, false, true, 70, portrait_y, true);

        int outfit = career.outfit;
        panel_detail = make_sprite(bn::sprite_items::wardrobe_portraits, 0,
                                   outfit ? outfit - 1 : accessory_count);
        position(*panel_detail, 170, portrait_y);
        panel_detail->set_bg_priority(0);
        panel_detail->set_visible(true);
        gear_options(focus);
    }

    void gear_options(int focus)
    {
        menu_text.clear();
        menu_option(70, 130, "CUES", focus == 0);
        menu_option(170, 130, "WARDROBE", focus == 1);
    }

    void show_collection(const Career& career, int item, bool clothing, bool details)
    {
        open_panel();
        bool owned = clothing ? career.owns_accessory(item) : career.owns_cue(item);
        bool equipped = clothing ? career.outfit == item + 1 : career.equipped == item;
        label(panel_text, 120, 34, clothing ? "WARDROBE" : "CUES", gold_text);
        label(panel_text, 203, 34, bn::format<16>("< {} / {} >", item + 1,
              clothing ? accessory_count : cue_count), muted_text, Align::RIGHT);
        panel_detail = make_sprite(bn::sprite_items::gear_frame, 2);
        position(*panel_detail, 64, 80);
        panel_detail->set_bg_priority(0);
        panel_detail->set_visible(true);
        show_gear_icon(item, clothing, owned, 64, 80, true);

        label(panel_text, 104, 52, clothing ? accessories[item] : cues[item].name, gold_text, Align::LEFT);
        label(panel_text, 104, 64, equipped ? "EQUIPPED" : owned ? "OWNED" : "LOCKED",
              equipped ? mint_text : muted_text, Align::LEFT);
        if(clothing)
        {
            constexpr const char* descriptions[] = {
                "BRASS-TRIMMED VEST", "FROSTED SATIN BOW", "IVORY-TIPPED EARS", "PLUM CAP WITH INK PIN",
                "STEEL BLUE DORSAL FIN", "SMOKED LENSES", "CLOTH TEAM BAND", "BRUSHED GOLD BRIM"
            };
            label(panel_text, 104, 82, descriptions[item], ivory_text, Align::LEFT);
        }
        else if(details)
        {
            const CueSpec& cue = cues[item];
            label(panel_text, 104, 76, cue.power, mint_text, Align::LEFT);
            label(panel_text, 104, 87, cue.tip, ivory_text, Align::LEFT);
            if(cue.ability == CueAbility::tailspin || cue.ability == CueAbility::surge)
            {
                label(panel_text, 104, 98, bn::format<24>("+{}%  {}", cue.ability == CueAbility::tailspin ?
                      tailspin_bonus : surge_bonus, cue.ability == CueAbility::tailspin ? "SPIN" : "SHOT SPEED"),
                      ivory_text, Align::LEFT);
            }
            else
            {
                label(panel_text, 104, 98, cue.detail, ivory_text, Align::LEFT);
            }
            if(item && owned)
            {
                label(panel_text, 104, 110, "L + R  AFTER THE BREAK", muted_text, Align::LEFT);
            }
        }
        else
        {
            show_cue_stats(cues[item]);
            if(item && owned)
            {
                label(panel_text, 104, 110, cues[item].power, muted_text, Align::LEFT);
            }
        }

        if(! owned)
        {
            if(clothing && item >= rival_count)
            {
                label(panel_text, 104, 107, item == chalk_headband ? "WIN A RIVAL MATCH" : "BEAT FINN",
                      muted_text, Align::LEFT);
            }
            else
            {
                label(panel_text, 104, 107, bn::format<28>("BEAT {} TO UNLOCK",
                      rivals[clothing ? item : item - 1].short_name), muted_text, Align::LEFT);
            }
        }

        if(owned && (clothing || ! equipped))
        {
            menu_option(46, 130, equipped ? "REMOVE" : "EQUIP", true, Align::LEFT);
        }
        if(! clothing && item)
        {
            label(menu_text, 120, 130, details ? "SEL STATS" : "SEL ABILITY", mint_text);
        }
        label(menu_text, 203, 130, "B GEAR", muted_text, Align::RIGHT);
    }

    void show_badges(const Career& career)
    {
        int mastered = 0;
        for(uint8_t wins : career.wins_by_rival)
        {
            mastered += wins >= 10;
        }
        const char* labels[] = { "FIRST SIGNATURE CUE", "THREE WIN STREAK", "DEFEAT FINN",
                                 "ALL SIGNATURE CUES", "MASTER A RIVAL", "FIVE-SHOT CHALLENGE" };
        bool earned[] = { bool(career.defeated), career.best_streak >= 3, career.owns(boss_rival),
                          career.defeated == (1 << rival_count) - 1, mastered > 0, career.challenges > 0 };
        for(int index = 0; index < 6; ++index)
        {
            int y = 63 + index * 8;
            label(panel_text, 38, y, labels[index], ivory_text, Align::LEFT);
            label(panel_text, 200, y, earned[index] ? "EARNED" : "LOCKED", earned[index] ? gold_text : muted_text,
                  Align::RIGHT);
        }
        label(panel_text, 120, 116, bn::format<32>("BEST STREAK {}  /  {} MASTERED", career.best_streak, mastered),
              mint_text);
    }

    void set_equipment(int style, int outfit)
    {
        active_outfit = outfit;
        if(style == cue_style)
        {
            return;
        }
        cue_style = style;
        stick.set_tiles(bn::sprite_items::cue.tiles_item(), style);
        close_stick_length = 0;
        auto colors = cue_colors(style);
        bn::sprite_palette_item recolored(colors, bn::bpp_mode::BPP_4);
        bn::sprite_palette_ptr palette = stick.palette();
        palette.set_colors(recolored);
        bn::sprite_palette_ptr close_palette = close_stick.palette();
        if(close_palette != palette)
        {
            close_palette.set_colors(recolored);
        }
    }

    void show_taunt(int rival, Banter event)
    {
        taunt_text.clear();
        taunt_chips.clear();
        if(rival < 0)
        {
            return;
        }
        bn::string_view quote = banter_line(rival, event);
        int width = std::clamp((text.width(quote) + 11) & ~3, 52, 120);
        int left = std::min(144, 234 - width);
        int first_tile = (width - 52) / 4 * 2;
        // Solo rivals occupy the right-hand card in either HUD layout.
        int top = full_hud ? 34 : 17;
        for(int side = 0; side < 2; ++side)
        {
            taunt_chips.push_back(make_sprite(bn::sprite_items::taunt_chip, 1, first_tile + side));
            bn::sprite_ptr& chip = taunt_chips.back();
            position(chip, left + 32 + side * 64, top + 16);
            chip.set_bg_priority(0);
            chip.set_visible(true);
        }
        label(taunt_text, left + width / 2, top + 11, quote);
    }

    void show_room(bool start_close)
    {
        clear_panel();
        hide_match_hud();
        create_match_sprites();
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
            "L / R", "L + R", "D-PAD", "PAUSE / D-PAD", "PAUSE / L / R", "SELECT", "HOLD A", "B", "D-PAD / A"
        };
        constexpr const char* actions[] = {
            "AIM", "ACTIVATE EQUIPPED CUE", "SET SPIN", "CHOOSE AN OPTION", "CYCLE POCKET LINES", "SWITCH CAMERAS",
            "RELEASE TO SHOOT", "CANCEL / RECALL 8", "MOVE / PLACE CUE"
        };
        for(int row = 0; row < 9; ++row)
        {
            label(panel_text, 39, 49 + row * 8, buttons[row], mint_text, Align::LEFT);
            label(panel_text, 112, 49 + row * 8, actions[row], ivory_text, Align::LEFT);
        }
        menu_option(120, 130, paused ? "RETURN TO PAUSE" : "RETURN TO TITLE", true);
    }

    void pause(bool linked, bool full_hud_enabled, int lines, int selected, int focus)
    {
        open_panel(true);
        label(panel_text, 120, 47, "MATCH PAUSED", gold_text);
        menu_option(120, 65, "RESUME", focus == 0);
        menu_option(120, 82, full_hud_enabled ? "HUD: FULL" : "HUD: COMPACT", focus == 1);
        if(lines)
        {
            menu_option(120, 98, selected >= 0 ? bn::format<24>("POCKET LINE {} / {}", selected + 1, lines) :
                        bn::string<24>("POCKET LINES"), focus == 2);
        }
        menu_option(120, 114, "CONTROLS", focus == 3);
        menu_option(120, 130, linked ? "LEAVE MATCH" : "CONCEDE", focus == 4);
    }

    void concession(bool linked)
    {
        open_panel();
        label(panel_text, 120, 34, linked ? "LEAVE LINK MATCH" : "CONCEDE MATCH", gold_text);
        label(panel_text, 120, 80, linked ? "THIS ENDS THE MATCH FOR BOTH PLAYERS" : "THIS COUNTS AS A RANKED LOSS");
        confirm_options(linked, 0);
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
            menu_option(120, 130, action, true);
        }
    }

    void link_result(bool won, const char* reason)
    {
        link_status(won ? "YOU WIN" : "YOU LOSE", reason, "LINK CABLE MATCH", "A / B  LOBBY");
    }

    void result(bool won, const char* reason, const Career& career, MatchReward reward, int new_rival, int rival)
    {
        open_panel();
        label(panel_text, 120, 34, won ? "YOU WIN" : "YOU LOSE", gold_text);
        label(panel_text, 120, 55, reason, muted_text);
        label(panel_text, 119, 68, bn::format<16>("ELO {}", career.profile.rating), mint_text, Align::RIGHT);
        rating_change(140, 152, 68, reward.rating);
        if(reward.cue)
        {
            show_gear_icon(reward.cue, false, true, 64, 82);
            label(panel_text, 103, 81, bn::format<32>("EARNED  {}", cues[reward.cue].name), gold_text, Align::LEFT);
        }
        if(reward.accessory)
        {
            panel_detail = make_sprite(bn::sprite_items::cosmetic_avatars, 0, reward.accessory - 1);
            position(*panel_detail, 64, 99);
            panel_detail->set_bg_priority(0);
            panel_detail->set_visible(true);
            label(panel_text, 103, 94, bn::format<32>("UNLOCKED  {}", accessories[reward.accessory - 1]), gold_text,
                  Align::LEFT);
        }
        else if(reward.milestone)
        {
            label(panel_text, 120, 94, "RIVAL MASTERED", gold_text);
        }
        else if(reward.challenge)
        {
            label(panel_text, 120, 94, "FIVE-SHOT CHALLENGE CLEARED", gold_text);
        }
        if(new_rival >= 0)
        {
            label(panel_text, 132, 106, bn::format<40>("NEXT  {}", rivals[new_rival].name), mint_text);
        }
        label(panel_text, 120, 115, banter_line(rival, won ? Banter::player_win : Banter::rival_win), muted_text);
        result_options(0, new_rival >= 0);
    }

    void confirm_options(bool linked, int focus)
    {
        menu_text.clear();
        menu_option(75, 130, linked ? "LEAVE" : "CONCEDE", focus == 0);
        menu_option(165, 130, "CANCEL", focus == 1);
    }

    void result_options(int focus, bool next_match)
    {
        menu_text.clear();
        menu_option(75, 130, next_match ? "NEXT MATCH" : "REMATCH", focus == 0);
        menu_option(165, 130, "PICK RIVAL", focus == 1);
    }

    void clear_panel()
    {
        clear_panel_contents();
        panel.reset();
    }

    void release_match_sprites()
    {
        hide_playfield();
        hide_match_hud();
        // Hidden sprites still hold their palettes until the handles are released.
        balls.clear();
        shadows.clear();
        group_balls.clear();
        avatars.clear();
        cue_tint = CueBallTint::ivory;
    }

    void scoreboard(const EightBall& game, const Profile& profile, int rival_index, Phase phase,
                    int choices, int selected, int link_player, int call, int cursor_pocket, bool full, CueSpin spin, bool relocating, const char* ability_hint)
    {
        if(! match_hud || full != full_hud)
        {
            match_hud.reset();
            const auto& item = full ? bn::regular_bg_items::match_hud : bn::regular_bg_items::match_hud_compact;
            match_hud = item.create_bg(8, 48);
            match_hud->set_priority(0);
            full_hud = full;
        }
        const Rival& rival = rivals[rival_index];
        hud.clear();
        footer.clear();
        bool linked = link_player >= 0;
        int local = linked ? link_player : 0;
        int ball_y = full ? 27 : 9;
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
                if(! linked && player == local && active_outfit)
                {
                    avatars[player].set_item(bn::sprite_items::cosmetic_avatars, active_outfit - 1);
                }
                else if(! linked && player != local)
                {
                    if(rival_index == boss_rival)
                    {
                        avatars[player].set_item(bn::sprite_items::finn_icon);
                    }
                    else
                    {
                        avatars[player].set_item(bn::sprite_items::rival_icons, rival_index);
                    }
                }
                else
                {
                    avatars[player].set_item(bn::sprite_items::avatars, linked ? player : 0);
                }
                position(avatars[player], left + 12, 12);
                const auto& name_color = player == game.player ? gold_text : ivory_text;
                const char* name = player == local ? "YOU" : linked ? "FRIEND" : rival.short_name;
                label(hud, left + 26, 9, name, name_color, Align::LEFT);
                label(hud, left + 26, 16, linked ? bn::format<12>("GBA {}", player + 1) :
                      bn::format<12>("ELO {}", player ? rival.rating : profile.rating), mint_text, Align::LEFT);
            }
            avatars[player].set_visible(full);

            Group group = game.groups[player];
            bool assigned = group != Group::open;
            int count = assigned ? game.remaining(group) : 0;
            bool on_eight = assigned && count == 0;
            int first = group == Group::stripes ? 9 : 1;
            int visible = 0;
            for(int index = 0; index < 7; ++index)
            {
                bn::sprite_ptr& icon = group_balls[player * 7 + index];
                int id = first + index;
                bool show = assigned && (on_eight ? index == 0 : game.table.balls[id].live);
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
            position(bar[i], 104 + i * 8, full ? 21 : ball_y);
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
            label(hud, 120, 10, "POWER", muted_text);
        }

        if(phase == Phase::call)
        {
            status(bn::format<32>("CALL: {}", pocket_names[cursor_pocket]));
        }
        else if(phase == Phase::place)
        {
            status(relocating ? "LONG REACH" : foul_label(game.notice),
                   relocating ? "A PLACE / B CANCEL" : "D-PAD MOVE / A PLACE");
        }
        else if(ability_hint)
        {
            status(ability_hint, call >= 0 ? "B CHANGE POCKET" : "");
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
                status(bn::format<32>("{} {}", rival.short_name, game.breaking ? "BREAKS" : "IS AIMING"),
                       game.ball_in_hand ? "BALL IN HAND" : "");
            }
            else if(phase == Phase::rolling)
            {
                status(game.player == local ? bn::string<32>("YOUR SHOT") :
                       bn::format<32>("{} SHOT", linked ? "FRIEND" : rival.short_name));
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

        if(hold_camera && camera_angle >= 0 && pocket == camera_pocket)
        {
            // Hold the view while the balls roll.
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
            camera_pocket = staged_pocket;
            ++camera_revision;
            staged_angle = -1;
            staged_settle = false;
            background->set_visible(false);
            aim_backdrop->set_visible(true);
            aim_table.commit();
            return true;
        }

        if(angle == camera_angle && cue.x == camera_anchor.x && cue.y == camera_anchor.y &&
           pocket == camera_pocket)
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
        staged_pocket = pocket;
        aim_table.draw(staged_camera, pocket);
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
        constexpr int max_pose_turn = 11;
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
                close_stick.set_tiles(bn::sprite_items::close_cue.tiles_item(), cue_style * 64 + std::min(length, 32) - 1);
                if(length > 32)
                {
                    close_tail.set_tiles(bn::sprite_items::close_cue.tiles_item(), cue_style * 64 + length - 1);
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

    void trajectory(const EightBall& game, int angle, bool bank_aid)
    {
        const Table& table = game.table;
        Vec cue = table.balls[0].pos;
        Vec heading = direction(angle);
        Preview preview;
        if(bank_aid)
        {
            preview = trace_aim(table, heading);
        }
        else
        {
            preview.first = table.trace(cue, heading, 0);
        }
        const Ray& first = preview.first;
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
        if(bank_aid && first.rail)
        {
            Vec travel_path = preview.bank.end - first.end;
            int distance = length(travel_path);
            if(distance > 6 * unit)
            {
                Vec rebound = normalized(travel_path);
                LineInk bank_ink = preview.bank.ball >= 0 && ! game.legal_target(preview.bank.ball) ?
                                   LineInk::warning : LineInk::bank;
                line.draw(project(first.end + travel(rebound, show_ghost ? ghost_ball_radius : 0), ball_height),
                          project(preview.bank.end, ball_height), bank_ink);
            }
        }
        else if(bank_aid && preview.object_moves && game.legal_target(first.ball))
        {
            line.draw(project(table.balls[first.ball].pos, ball_height),
                      project(preview.object.end, ball_height), LineInk::bank);
        }
        line.commit();
    }

private:
    bn::optional<bn::regular_bg_ptr> background;
    bn::optional<bn::regular_bg_ptr> panel;
    bn::optional<bn::sprite_ptr> portrait;
    bn::optional<bn::sprite_ptr> panel_detail;
    bn::vector<bn::sprite_ptr, rating_plot_slices> chart;
    bn::vector<bn::sprite_ptr, 2> loadout_frames;
    bn::optional<bn::sprite_ptr> cue_stats;
    int cue_style = 0;
    int active_outfit = 0;
    bool pause_panel_open = false;
    bn::optional<bn::regular_bg_ptr> match_hud;
    bn::optional<bn::regular_bg_ptr> aim_backdrop;
    bn::sprite_text_generator text{ hud_font };
    bn::vector<bn::sprite_ptr, 16> hud;
    bn::vector<bn::sprite_ptr, 16> footer;
    bn::vector<bn::sprite_ptr, 20> menu_text;
    bn::vector<bn::sprite_ptr, 8> taunt_text;
    bn::vector<bn::sprite_ptr, 2> taunt_chips;
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
    int camera_pocket = -1;
    int staged_pocket = -1;
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
        bool on_panel = &target == &panel_text || &target == &taunt_text ||
                        (&target == &menu_text && bool(panel)) || bool(match_hud);
        text.set_bg_priority(on_panel ? 0 : 2);
        text.generate(x - 120, y - 80, value, target);
    }

    void show_gear_icon(int item, bool clothing, bool owned, int x, int y, bool large = false)
    {
        const auto& artwork = clothing ? bn::sprite_items::wardrobe_portraits :
                              (large ? bn::sprite_items::cue_previews : bn::sprite_items::cue);
        auto palette = artwork.palette_item();
        bn::array<bn::color, 16> colors{};
        if(! owned)
        {
            std::fill_n(colors.data() + 1, 15, bn::color(8, 13, 16));
            palette = bn::sprite_palette_item(colors, bn::bpp_mode::BPP_4);
        }
        else if(! clothing)
        {
            colors = cue_colors(item);
            palette = bn::sprite_palette_item(colors, bn::bpp_mode::BPP_4);
        }
        bn::sprite_item preview(artwork.shape_size(), artwork.tiles_item(), palette);
        portrait = make_sprite(preview, 0, item);
        position(*portrait, x, y);
        portrait->set_bg_priority(0);
        portrait->set_visible(true);
    }

    void show_cue_stats(const CueSpec& cue)
    {
        constexpr int bar_width = 52;
        constexpr int bar_start = 2;
        constexpr int first_row_y = 76;
        constexpr int row_spacing = 11;
        static constexpr bn::array<bn::color, 16> colors = {
            bn::color(), { 10, 17, 20 }, { 11, 20, 23 }, { 13, 24, 26 }, { 17, 28, 28 },
            { 10, 13, 16 }, { 6, 9, 11 }
        };
        constexpr const char* names[] = { "POWER", "SPIN", "CONTROL" };
        const int values[] = { cue.speed, cue.spin, cue.control };
        std::memset(cue_stat_tiles, 0, sizeof(cue_stat_tiles));
        for(int stat = 0; stat < 3; ++stat)
        {
            int y = first_row_y + stat * row_spacing;
            int value = values[stat];
            label(panel_text, 104, y, names[stat], muted_text, Align::LEFT);
            label(panel_text, 212, y, bn::format<8>("{}", value), ivory_text, Align::RIGHT);
            int filled = (value * bar_width + 50) / 100;
            unsigned fill_ink = value >= 90 ? 4 : value >= 80 ? 3 : value >= 70 ? 2 : 1;
            for(int row = 0; row < 4; ++row)
            {
                int py = stat * row_spacing + row + 2;
                for(int x = 0; x < bar_width; ++x)
                {
                    int px = bar_start + x;
                    unsigned color = x < filled ? fill_ink : unsigned(row == 0 ? 5 : 6);
                    cue_stat_tiles[(py >> 3) * 8 + (px >> 3)].data[py & 7] |= color << ((px & 7) * 4);
                }
            }
        }
        bn::sprite_tiles_item tiles(cue_stat_tiles, bn::bpp_mode::BPP_4);
        bn::sprite_palette_item palette(colors, bn::bpp_mode::BPP_4);
        bn::sprite_item item(bn::sprite_shape_size(64, 32), tiles, palette);
        cue_stats = make_sprite(item, 1);
        bn::sprite_tiles_ptr tiles_ptr = cue_stats->tiles();
        tiles_ptr.reload_tiles_ref();
        cue_stats->set_bg_priority(0);
        position(*cue_stats, 175, first_row_y + 12);
        cue_stats->set_visible(true);
    }

    void rating_change(int sign_x, int value_x, int y, int change)
    {
        const auto& ink = change < 0 ? muted_text : gold_text;
        label(panel_text, sign_x, y, change < 0 ? "-" : "+", ink);
        label(panel_text, value_x, y, bn::format<8>("{}", std::abs(change)), ink, Align::RIGHT);
    }

    void show_rating_chart(const Career& career)
    {
        int initial_rating = career.profile.rating;
        if(career.count)
        {
            const CareerMatch& first = career.recent(career.count - 1);
            initial_rating = first.rating - first.change;
        }
        int low = std::min(initial_rating, career.profile.rating);
        int high = std::max(initial_rating, career.profile.rating);
        for(int age = 0; age < career.count; ++age)
        {
            int rating = career.recent(age).rating;
            low = std::min(low, rating);
            high = std::max(high, rating);
        }
        if(high <= 1200)
        {
            low = std::min(low, 800);
            high = 1200;
        }
        else
        {
            low = std::max(100, low - 28);
            high = std::max(high + 24, low + 320);
        }
        constexpr int origin_y = 71;
        auto row = [&](int rating) { return 35 - (rating - low) * 33 / (high - low); };
        std::memset(rating_tiles, 0, sizeof(rating_tiles));
        for(int rating : { high, (low + high) / 2, low })
        {
            int y = row(rating);
            for(int x = 0; x < rating_plot_width; x += 2)
            {
                rating_pixel(x, y, 3);
            }
        }
        Vec last{ 2, row(initial_rating) };
        rating_pixel(last.x, last.y, 8);
        for(int index = 0; index < career.count; ++index)
        {
            const CareerMatch& match = career.recent(career.count - 1 - index);
            Vec point{ 2 + (index + 1) * (rating_plot_width - 4) / career.count, row(match.rating) };
            rating_segment(last, point);
            last = point;
        }
        if(! career.count)
        {
            label(panel_text, 120, 93, "NO MATCHES YET", muted_text);
        }
        label(panel_text, 120, 115, "ELO RANGE", muted_text);
        label(panel_text, rating_plot_left, 115, bn::format<8>("{}", low), muted_text, Align::LEFT);
        label(panel_text, rating_plot_left + rating_plot_width - 1, 115, bn::format<8>("{}", high), muted_text,
              Align::RIGHT);
        for(int index = 0; index < rating_plot_slices; ++index)
        {
            bn::sprite_tiles_item tiles(rating_tiles[index], bn::bpp_mode::BPP_4);
            bn::sprite_item item(bn::sprite_shape_size(64, 64), tiles, bn::sprite_items::marks.palette_item());
            chart.push_back(make_sprite(item, 2));
            chart.back().set_bg_priority(0);
            position(chart.back(), rating_plot_left + 32 + index * 64, origin_y + 32);
            chart.back().set_visible(true);
        }
    }

    void menu_option(int x, int y, const bn::string_view& value, bool focused, Align align = Align::CENTER)
    {
        label(menu_text, x, y, value, focused ? gold_text : mint_text, align);
        if(focused)
        {
            int left = align == Align::CENTER ? x - text.width(value) / 2 : x;
            label(menu_text, left - 9, y, ">", gold_text, Align::LEFT);
        }
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

    void clear_panel_contents()
    {
        portrait.reset();
        panel_detail.reset();
        chart.clear();
        loadout_frames.clear();
        cue_stats.reset();
        panel_text.clear();
        menu_text.clear();
    }

    void open_panel(bool pause_layout = false)
    {
        clear_panel_contents();
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

    void create_match_sprites()
    {
        if(! balls.empty())
        {
            return;
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
    }

    void hide_match_hud()
    {
        match_hud.reset();
        show_taunt(-1, Banter::intro);
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
        view.show_lobby(records.career(), title_focus);
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
                title_input();
                break;
            case Screen::roster:
                choose_rival();
                break;
            case Screen::record:
                record_input();
                break;
            case Screen::gear:
                gear_input();
                break;
            case Screen::collection:
                collection_input();
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
                    menu_focus = 0;
                    show_pause();
                    break;
                }
                play();
                break;
            case Screen::pause:
                pause_input();
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
                if(menu_axis())
                {
                    menu_focus = 1 - menu_focus;
                    view.confirm_options(linked(), menu_focus);
                }
                else if(bn::keypad::b_pressed() || (bn::keypad::a_pressed() && menu_focus == 1))
                {
                    screen = Screen::pause;
                    menu_focus = 4;
                    show_pause();
                }
                else if(bn::keypad::a_pressed())
                {
                    if(linked())
                    {
                        cable.leave();
                        leave_at = frame + 24;
                        screen = Screen::link_leaving;
                        view.release_match_sprites();
                        view.link_status("LINK CABLE", "LEAVING MATCH", "", "");
                    }
                    else
                    {
                        complete(false, "MATCH CONCEDED");
                    }
                }
                break;
            case Screen::result:
                if(linked())
                {
                    if((! cable.sending() || cable.lost()) && (bn::keypad::b_pressed() || bn::keypad::a_pressed()))
                    {
                        lobby();
                    }
                }
                else if(menu_axis())
                {
                    menu_focus = 1 - menu_focus;
                    view.result_options(menu_focus, next_rival_unlocked);
                }
                else if(bn::keypad::b_pressed() || (bn::keypad::a_pressed() && menu_focus == 1))
                {
                    roster_index = result_roster_index;
                    open_roster();
                }
                else if(bn::keypad::a_pressed())
                {
                    if(next_rival_unlocked)
                    {
                        match_rival = result_roster_index;
                    }
                    start_match();
                }
                break;
            case Screen::link_wait:
                if(bn::keypad::b_pressed() || bn::keypad::a_pressed())
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
    DirectionRepeat side_spin_repeat;
    DirectionRepeat vertical_spin_repeat;
    int charge = 0;
    int charge_fraction = 0;
    int hold = 0;
    int timer = 0;
    int match_rival = 0;
    int result_roster_index = 0;
    bool next_rival_unlocked = false;
    int roster_index = 0;
    int gear_cursor = 0;
    int menu_focus = 0;
    int title_focus = 0;
    int local_shots = 0;
    bool local_power_spent = false;
    bool rival_power_spent = false;
    bool rival_reaching = false;
    int bank_guides_left = 0;
    int rival_bank_shots_used = 0;
    CueAbility armed_power = CueAbility::none;
    PotCombo pot_combos[2];
    bool saved_turn = false;
    bool force_placing = false;
    Vec force_origin;
    int taunt_life = 0;
    Banter active_taunt = Banter::intro;
    int taunt_cooldown = 0;
    bool shot_started_on_eight = false;
    bool clothing = false;
    bool cue_details = false;
    int history_offset = 0;
    RecordPage record_page = RecordPage::rating;
    int sound_cooldown = 0;
    uint16_t last_pots = 0;
    CableMatch cable;
    int turn_index = 0;
    unsigned leave_at = 0;
    Vec link_placement;
    bool placed_for_link = false;
    int pending_speed = 0;

    bool linked() const { return cable.paired(); }
    int ranked_cue() const { return linked() ? 0 : records.career().equipped; }
    int shooting_cue() const { return ! linked() && game.player ? match_rival + 1 : ranked_cue(); }
    int local_player() const { return linked() ? cable.player() : 0; }

    const Rival& rival() const { return rivals[match_rival]; }
    CueAbility rival_ability() const { return cues[match_rival + 1].ability; }

    static int menu_horizontal()
    {
        return int(bn::keypad::right_pressed()) - int(bn::keypad::left_pressed());
    }

    static int menu_vertical()
    {
        return int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());
    }

    static int menu_axis()
    {
        int vertical = menu_vertical();
        return vertical ? vertical : menu_horizontal();
    }

    void title_input()
    {
        if(menu_axis())
        {
            title_focus ^= menu_vertical() ? 2 : 1;
            view.title_options(title_focus);
            return;
        }
        if(bn::keypad::start_pressed())
        {
            match_rival = records.career().rival_index();
            start_match();
            return;
        }
        if(bn::keypad::select_pressed())
        {
            screen = Screen::help;
            view.controls(false);
            return;
        }

        int action = title_focus;
        if(bn::keypad::b_pressed())
        {
            action = 3;
        }
        else if(bn::keypad::l_pressed())
        {
            action = 2;
        }
        else if(! bn::keypad::a_pressed())
        {
            return;
        }
        switch(action)
        {
            case 0:
                roster_index = records.career().rival_index();
                open_roster();
                break;
            case 1:
                screen = Screen::record;
                record_page = RecordPage::rating;
                history_offset = 0;
                view.show_menu();
                view.show_record(records.career(), record_page, history_offset);
                break;
            case 2:
                view.show_menu();
                open_gear();
                break;
            case 3:
                cable.open();
                screen = Screen::link_wait;
                view.show_menu();
                view.link_status("LINK CABLE", "WAITING FOR A FRIEND", "OPEN LINK ON BOTH GBAS", "A / B TITLE");
                break;
            default: break;
        }
    }

    void gear_input()
    {
        if(bn::keypad::b_pressed())
        {
            lobby();
        }
        else if(menu_axis())
        {
            menu_focus = 1 - menu_focus;
            view.gear_options(menu_focus);
        }
        else if(bn::keypad::a_pressed() || bn::keypad::l_pressed() || bn::keypad::r_pressed())
        {
            screen = Screen::collection;
            select_collection(bn::keypad::r_pressed() || (! bn::keypad::l_pressed() && menu_focus == 1));
            view.show_collection(records.career(), gear_cursor, clothing, cue_details);
        }
    }

    int pause_lines() const
    {
        return phase == Phase::aim && ! finding_lines ? suggestions.size() : 0;
    }

    void pause_input()
    {
        int vertical = menu_vertical();
        if(vertical)
        {
            do
            {
                menu_focus = (menu_focus + vertical + 5) % 5;
            } while(menu_focus == 2 && ! pause_lines());
            show_pause();
            return;
        }

        bool confirm = bn::keypad::a_pressed();
        int horizontal = menu_horizontal();
        int shoulder = int(bn::keypad::r_pressed()) - int(bn::keypad::l_pressed());
        if(bn::keypad::start_pressed() || bn::keypad::b_pressed() || (confirm && menu_focus == 0))
        {
            screen = Screen::match;
            view.clear_panel();
            score();
            if(taunt_life)
            {
                view.show_taunt(match_rival, active_taunt);
            }
            last_guide = -1;
            draw_table();
        }
        else if(confirm && menu_focus == 4)
        {
            screen = Screen::concede;
            menu_focus = 0;
            view.concession(linked());
        }
        else if(bn::keypad::select_pressed() || (confirm && menu_focus == 3))
        {
            screen = Screen::pause_help;
            view.controls(true);
        }
        else if(menu_focus == 1 && (confirm || horizontal))
        {
            full_match_hud = ! full_match_hud;
            show_pause();
        }
        else if(pause_lines() && (shoulder || (menu_focus == 2 && (confirm || horizontal))))
        {
            int direction = shoulder ? shoulder : horizontal;
            cycle_pocket_line(direction ? direction : 1);
            show_pause();
        }
    }

    void lobby()
    {
        cable.close();
        screen = Screen::lobby;
        menu_focus = 0;
        view.show_lobby(records.career(), title_focus);
    }

    void open_roster()
    {
        screen = Screen::roster;
        menu_focus = 0;
        view.show_menu();
        view.show_roster(records.career(), roster_index);
    }

    void open_gear()
    {
        screen = Screen::gear;
        menu_focus = 0;
        view.show_loadout(records.career(), menu_focus);
    }

    void select_collection(bool outfits)
    {
        clothing = outfits;
        cue_details = false;
        menu_focus = 0;
        int outfit = records.career().outfit;
        gear_cursor = outfits ? (outfit ? outfit - 1 : sunglasses) : records.career().equipped;
    }

    void choose_rival()
    {
        const Career& career = records.career();
        bool unlocked = roster_index <= career.rival_index();
        if(bn::keypad::b_pressed())
        {
            lobby();
        }
        else if(int change = menu_horizontal())
        {
            roster_index = (roster_index + change + rival_count) % rival_count;
            view.show_roster(career, roster_index);
        }
        else if(bn::keypad::a_pressed())
        {
            if(unlocked)
            {
                match_rival = roster_index;
                start_match();
            }
            else
            {
                bn::sound_items::reject.play(bn::fixed(0.4));
            }
        }
    }

    void collection_input()
    {
        if(bn::keypad::b_pressed())
        {
            open_gear();
            return;
        }
        if(bn::keypad::l_pressed() || bn::keypad::r_pressed())
        {
            select_collection(! clothing);
        }
        else if(int change = menu_horizontal())
        {
            int count = clothing ? accessory_count : cue_count;
            gear_cursor = (gear_cursor + change + count) % count;
            if(! gear_cursor)
            {
                cue_details = false;
            }
        }
        else if(bn::keypad::select_pressed() && ! clothing && gear_cursor)
        {
            cue_details = ! cue_details;
        }
        else if(bn::keypad::a_pressed())
        {
            if(! records.collection_action(gear_cursor, clothing))
            {
                return;
            }
            bn::sound_items::confirm.play(bn::fixed(0.35));
        }
        else
        {
            return;
        }
        view.show_collection(records.career(), gear_cursor, clothing, cue_details);
    }

    void record_input()
    {
        if(bn::keypad::b_pressed())
        {
            lobby();
            return;
        }
        int page_change = menu_horizontal();
        if(! page_change)
        {
            page_change = int(bn::keypad::r_pressed()) - int(bn::keypad::l_pressed());
        }
        if(page_change)
        {
            record_page = RecordPage((int(record_page) + page_change + 3) % 3);
        }
        else if(bn::keypad::select_pressed())
        {
            record_page = record_page == RecordPage::badges ? RecordPage::rating : RecordPage::badges;
        }
        else if(record_page == RecordPage::history)
        {
            int direction = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());
            int max_offset = std::max(0, ((records.career().count - 1) / matches_per_page) * matches_per_page);
            int offset = std::clamp(history_offset + direction * matches_per_page, 0, max_offset);
            if(history_offset == offset)
            {
                return;
            }
            history_offset = offset;
        }
        else
        {
            return;
        }
        view.show_record(records.career(), record_page, history_offset);
    }

    void disconnect()
    {
        cable.close();
        screen = Screen::link_lost;
        view.release_match_sprites();
        view.link_status("LINK ENDED", "FRIEND DISCONNECTED", "", "A / B  LOBBY");
    }

    void start_match()
    {
        screen = Screen::match;
        seed ^= frame * 0x9e3779b9u;
        int breaker = linked() ? 0 : (records.profile().wins + records.profile().losses) & 1;
        turn_index = 0;
        game.start(breaker);
        angle = 0;
        view.show_room(prefer_close_view);
        local_shots = 0;
        local_power_spent = rival_power_spent = false;
        bank_guides_left = 0;
        rival_bank_shots_used = 0;
        armed_power = CueAbility::none;
        pot_combos[0] = {};
        pot_combos[1] = {};
        saved_turn = false;
        force_placing = false;
        taunt_life = 0;
        taunt_cooldown = 0;
        prepare_turn();
        taunt(Banter::intro);
        draw_table();
        bn::sound_items::confirm.play(bn::fixed(0.45));
    }

    void complete(bool won, const char* reason)
    {
        screen = Screen::result;
        menu_focus = 0;
        view.release_match_sprites();
        if(linked())
        {
            view.link_result(won, reason);
        }
        else
        {
            int previous_unlock = records.career().rival_index();
            MatchReward reward = records.add_result(won, match_rival, local_shots);
            int unlocked = records.career().rival_index();
            bool new_unlock = unlocked > previous_unlock;
            next_rival_unlocked = new_unlock;
            result_roster_index = new_unlock ? unlocked : match_rival;
            view.result(won, reason, records.career(), reward, new_unlock ? unlocked : -1, match_rival);
        }
        bn::sound_items::confirm.play(bn::fixed(0.55));
    }

    void show_pause()
    {
        view.pause(linked(), full_match_hud, pause_lines(), selected_line, menu_focus);
    }

    void score()
    {
        view.scoreboard(game, records.profile(), match_rival, phase, finding_lines ? 0 : suggestions.size(), selected_line,
                        linked() ? cable.player() : -1, called_pocket, call_cursor, full_match_hud, spin, force_placing,
                        local_power_hint());
    }

    const char* local_power_hint() const
    {
        CueAbility cue = cues[ranked_cue()].ability;
        if(phase != Phase::aim || cue == CueAbility::none)
        {
            return nullptr;
        }
        if(bank_guides_left)
        {
            if(spin.side)
            {
                return "ICE BANK / CENTER SIDESPIN";
            }
            if(prefer_close_view)
            {
                return "ICE BANK / SELECT FOR GUIDE";
            }
            return bank_guides_left > 1 ? "ICE BANK / 2 SHOTS LEFT" : "ICE BANK / 1 SHOT LEFT";
        }
        if(armed_power == CueAbility::tailspin)
        {
            return "TAILSPIN ARMED";
        }
        if(armed_power == CueAbility::surge)
        {
            return "SURGE ARMED";
        }
        if(saved_turn)
        {
            return "RIVER RUN / EXTRA SHOT";
        }
        const PotCombo& combo = pot_combos[0];
        if(combo.active())
        {
            return combo.ready() ? "RIVER RUN / MISS COVERED" : "RIVER RUN / POT TO ARM";
        }
        if(local_power_spent || game.breaking || (cue == CueAbility::pot_combo && game.requires_call()))
        {
            return nullptr;
        }
        switch(cue)
        {
            case CueAbility::bank_read: return "L + R  ICE BANK";
            case CueAbility::pot_combo: return "L + R  RIVER RUN";
            case CueAbility::move_cue: return "L + R  LONG REACH";
            case CueAbility::tailspin: return "L + R  TAILSPIN";
            case CueAbility::surge: return "L + R  SURGE";
            default: return nullptr;
        }
    }

    void activate_power()
    {
        CueAbility cue = cues[ranked_cue()].ability;
        if(cue == CueAbility::none || local_power_spent || game.breaking ||
           (cue == CueAbility::pot_combo && game.requires_call()))
        {
            return;
        }
        if(cue == CueAbility::move_cue)
        {
            Vec current = game.table.balls[0].pos;
            placement = { ((current.x + unit / 2) / unit) * unit, ((current.y + unit / 2) / unit) * unit };
            force_origin = placement;
            force_placing = true;
            set_phase(Phase::place);
        }
        else
        {
            local_power_spent = true;
            if(cue == CueAbility::bank_read)
            {
                bank_guides_left = ice_bank_shots;
            }
            armed_power = cue;
            if(cue == CueAbility::tailspin && ! spin.side && ! spin.vertical)
            {
                spin.vertical = -2;
            }
            if(cue == CueAbility::pot_combo)
            {
                pot_combos[0].arm();
            }
            last_guide = -1;
            score();
        }
        bn::sound_items::confirm.play(bn::fixed(0.38));
    }

    void set_phase(Phase value)
    {
        bool keep_aim_input = (phase == Phase::aim && value == Phase::charge) ||
                              (phase == Phase::charge && value == Phase::aim);
        phase = value;
        timer = 0;
        last_guide = -1;
        if(! keep_aim_input)
        {
            aim_hold = 0;
            previous_aim_input = 0;
            side_spin_repeat = {};
            vertical_spin_repeat = {};
        }
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
        view.set_equipment(shooting_cue(), linked() ? 0 : records.career().outfit);
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
            rival_reaching = false;
            if(! game.breaking && ! rival_power_spent)
            {
                if(rival_ability() == CueAbility::move_cue && ! game.ball_in_hand)
                {
                    rival_reaching = true;
                    game.ball_in_hand = true;
                }
                else if(rival_ability() == CueAbility::pot_combo && ! game.requires_call())
                {
                    rival_power_spent = true;
                    pot_combos[1].arm();
                    rival_power();
                }
            }
            planner.start(game, match_rival, cue_shot_speed(match_rival + 1, max_speed));
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
        int power_scale = cue_shot_speed(ranked_cue(), 100);
        aim_power = std::clamp((choice.speed * 100 + power_scale / 2) / power_scale, min_speed, max_speed);
        spin = {};
    }

    void spin_input()
    {
        int horizontal = side_spin_repeat.step(int(bn::keypad::right_held()) - int(bn::keypad::left_held()));
        int vertical = vertical_spin_repeat.step(int(bn::keypad::up_held()) - int(bn::keypad::down_held()));
        CueSpin next{ std::clamp(spin.side + horizontal, -max_spin, max_spin),
                      std::clamp(spin.vertical + vertical, -max_spin, max_spin) };
        if(next.side != spin.side || next.vertical != spin.vertical)
        {
            spin = next;
            selected_line = -1;
            if(bank_guides_left)
            {
                last_guide = -1;
            }
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
        shot_started_on_eight = game.requires_call();
        shot_origin = game.table.balls[0].pos;
        bool local_ranked = ! linked() && game.player == 0;
        if(local_ranked)
        {
            ++local_shots;
        }
        if(local_ranked && bank_guides_left)
        {
            --bank_guides_left;
        }
        saved_turn = false;
        int cue = shooting_cue();
        if(local_ranked)
        {
            speed = cue_shot_speed(cue, speed);
        }
        int spin_strength = cue_spin_strength(cue);
        CueAbility ability = armed_power;
        armed_power = CueAbility::none;
        if(! linked() && game.player && ! game.breaking && ! rival_power_spent &&
           ((rival_ability() == CueAbility::tailspin && ! game.requires_call()) ||
            (rival_ability() == CueAbility::surge && speed >= 360 && ! game.requires_call() &&
             ! planner.has_verified_pot())))
        {
            ability = rival_ability();
            rival_power_spent = true;
            rival_power();
            if(ability == CueAbility::tailspin)
            {
                shot_spin.vertical = -2;
            }
        }
        if(ability == CueAbility::tailspin)
        {
            spin_strength = (spin_strength * (100 + tailspin_bonus) + 50) / 100;
        }
        if(ability == CueAbility::surge)
        {
            speed = (speed * (100 + surge_bonus) + 50) / 100;
        }
        game.table.strike(direction(angle), speed, pocket, shot_spin, spin_strength);
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
            if(aim_hold < aim_full_speed)
            {
                ++aim_hold;
            }
            if(amount && selected_line >= 0)
            {
                selected_line = -1;
                score();
            }
        }
    }

    void place_ball()
    {
        if(force_placing && bn::keypad::b_pressed())
        {
            force_placing = false;
            set_phase(Phase::aim);
            return;
        }
        int x = int(bn::keypad::right_held()) - int(bn::keypad::left_held());
        int y = int(bn::keypad::down_held()) - int(bn::keypad::up_held());
        hold = (x || y) ? hold + 1 : 0;
        int pace = hold > 16 ? 2 * unit : unit;
        placement.x = std::clamp(placement.x + x * pace, left, right);
        placement.y = std::clamp(placement.y + y * pace, top, bottom);
        if(bn::keypad::a_pressed())
        {
            if(! game.table.can_place(placement) || (force_placing && placement.x == force_origin.x && placement.y == force_origin.y))
            {
                bn::sound_items::reject.play(bn::fixed(0.45));
                return;
            }
            // Link packets use whole-pixel positions. The placement preview moves on that grid.
            if(force_placing)
            {
                local_power_spent = true;
                force_placing = false;
                called_pocket = -1;
            }
            game.table.place_cue(placement);
            link_placement = placement;
            placed_for_link = true;
            game.ball_in_hand = false;
            prepare_aim();
            bn::sound_items::confirm.play(bn::fixed(0.32));
        }
    }

    void rival_power(Banter event = Banter::ability)
    {
        display_taunt(event, 140);
    }

    void taunt(Banter event)
    {
        if(! linked() && ! taunt_cooldown)
        {
            display_taunt(event, event == Banter::intro ? 60 : 140);
        }
    }

    void display_taunt(Banter event, int cooldown)
    {
        active_taunt = event;
        view.show_taunt(match_rival, event);
        taunt_life = 112;
        taunt_cooldown = cooldown;
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
            int shooter = game.player;
            bool human = shooter == local_player();
            bool combo_saved = game.finish_shot(&pot_combos[shooter]);
            saved_turn = combo_saved && human;
            if(combo_saved)
            {
                bn::sound_items::confirm.play(bn::fixed(0.38));
                if(! human)
                {
                    rival_power(Banter::combo_saved);
                }
            }
            ++turn_index;
            if(game.outcome == Outcome::playing)
            {
                if(game.ball_in_hand)
                {
                    taunt(human ? Banter::player_foul : Banter::rival_foul);
                }
                else if(game.player == local_player() && ! (human && shot_started_on_eight) && game.requires_call())
                {
                    taunt(Banter::on_eight);
                }
                else if(game.potted_group(Group::open))
                {
                    if(game.potted_group(game.groups[shooter]))
                    {
                        taunt(human ? Banter::player_pot : Banter::rival_pot);
                    }
                    else
                    {
                        taunt(human ? Banter::player_helped_me : Banter::i_helped_you);
                    }
                }
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
        if(taunt_life && ! --taunt_life)
        {
            view.show_taunt(-1, Banter::intro);
        }
        if(taunt_cooldown)
        {
            --taunt_cooldown;
        }
        if(phase != Phase::place && phase != Phase::call && bn::keypad::select_pressed())
        {
            prefer_close_view = ! prefer_close_view;
            last_guide = -1;
            score();
        }
        switch(phase)
        {
            case Phase::aim:
                update_suggestions();
                if(bn::keypad::l_held() && bn::keypad::r_held() &&
                   (bn::keypad::l_pressed() || bn::keypad::r_pressed()))
                {
                    activate_power();
                    break;
                }
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
                    charge_fraction = 0;
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
                        charge_fraction += (timer < 24 ? 5 : 8) * cue_charge_rate(ranked_cue());
                        charge = std::min(max_speed, charge + charge_fraction / 100);
                        charge_fraction %= 100;
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
                    if(rival_ability() == CueAbility::bank_read && rival_bank_shots_used < ice_bank_shots && ! game.breaking &&
                       game.table.trace(cpu_plan.placement, direction(planner.planned_angle()), 0).rail)
                    {
                        cpu_plan.angle = planner.planned_angle();
                        ++rival_bank_shots_used;
                        rival_power();
                    }
                    if(game.ball_in_hand)
                    {
                        if(rival_reaching && length(cpu_plan.placement - game.table.balls[0].pos) >= unit)
                        {
                            rival_power_spent = true;
                            rival_power();
                        }
                        rival_reaching = false;
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
        bool remote_shot_playing = linked() && game.player != local_player();
        bool hold_camera = (phase == Phase::rolling && ! remote_shot_playing) || phase == Phase::link_send;
        if(phase == Phase::cpu_aim && use_close_view)
        {
            heading = cpu_plan.angle;
        }
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
                        view.trajectory(game, displayed, ! spin.side && bank_guides_left);
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
                if(! use_close_view || ! view.camera_settling())
                {
                    int displayed = use_close_view ? view.displayed_angle() : angle;
                    view.draw_cue(game.table, displayed, std::clamp((timer - 12) / 2, 0, 12));
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
