#ifndef POOL_CAREER_H
#define POOL_CAREER_H

#include "pool.h"

namespace pool
{
constexpr int career_history_capacity = 20;
constexpr int boss_rival = rival_count - 1;

enum class CueAbility { none, bank_read, pot_combo, move_cue, tailspin, surge };

struct CueSpec
{
    const char* name;
    const char* power;
    const char* tip;
    const char* detail;
    CueAbility ability;
    int speed;
    int spin;
    int control;
};

inline constexpr int tailspin_bonus = 50;
inline constexpr int surge_bonus = 20;
inline constexpr int ice_bank_shots = 2;

inline constexpr CueSpec cues[] = {
    { "HOUSE CUE", "STANDARD EQUIPMENT", "", "", CueAbility::none, 70, 70, 70 },
    { "RIVER REED", "RIVER RUN", "POT ON YOUR NEXT SHOT", "SAVE ONE CLEAN MISS", CueAbility::pot_combo, 72, 68, 92 },
    { "FROST TIP", "ICE BANK", "GUIDES YOUR NEXT 2 SHOTS", "OVERHEAD / NO SIDESPIN", CueAbility::bank_read, 64, 78, 96 },
    { "FOXTAIL", "TAILSPIN", "DEFAULTS TO DRAW", "", CueAbility::tailspin, 68, 96, 73 },
    { "INKWOOD", "LONG REACH", "MOVE THE CUE BALL", "ANY LEGAL SPOT ON TABLE", CueAbility::move_cue, 79, 82, 62 },
    { "REEF FANG", "SURGE", "NEXT SHOT ONLY", "", CueAbility::surge, 96, 64, 54 },
};
constexpr int cue_count = sizeof(cues) / sizeof(*cues);
static_assert(cue_count == rival_count + 1);

constexpr int cue_shot_speed(int cue, int charge)
{
    return (charge * (100 + (cues[cue].speed - 70) / 4) + 50) / 100;
}

constexpr int cue_spin_strength(int cue)
{
    return 100 + (cues[cue].spin - 70) / 2;
}

constexpr int cue_charge_rate(int cue)
{
    return 100 - (cues[cue].control - 70) / 2;
}

inline constexpr const char* accessories[] = {
    "RIVER VEST", "ICE BOW TIE", "FOX EARS", "INK CAP", "REEF FIN",
    "SUNGLASSES", "CHALK HEADBAND", "GOLD VISOR"
};
constexpr int accessory_count = sizeof(accessories) / sizeof(*accessories);
static_assert(accessory_count == rival_count + 3);
constexpr int sunglasses = rival_count;
constexpr int chalk_headband = sunglasses + 1;
constexpr int gold_visor = sunglasses + 2;

struct CareerMatch
{
    int16_t rating = 0;
    int8_t change = 0;
    uint8_t rival = 0;
    uint8_t won = 0;
};

struct MatchReward
{
    int rating = 0;
    int cue = 0;
    int accessory = 0;
    int milestone = 0;
    bool challenge = false;
};

struct Career
{
    Profile profile;
    CareerMatch history[career_history_capacity];
    uint8_t defeated = 0;
    uint8_t equipped = 0;
    uint8_t count = 0;
    uint8_t next = 0;
    uint8_t wins_by_rival[rival_count] = {};
    uint8_t reserved = 0; // Reserved byte in the SRAM record.
    uint8_t outfit = 0;
    uint16_t streak = 0;
    uint16_t best_streak = 0;
    uint16_t challenges = 0;

    bool owns(int rival) const { return defeated & (1 << rival); }
    int rival_index() const
    {
        int index = 0;
        while(index < boss_rival && owns(index))
        {
            ++index;
        }
        return index;
    }

    bool owns_cue(int cue) const { return cue == 0 || owns(cue - 1); }
    bool owns_accessory(int index) const
    {
        return index < rival_count ? owns(index) : index == sunglasses ||
               (index == chalk_headband && defeated) || (index == gold_visor && owns(boss_rival));
    }

    const CareerMatch& recent(int age) const
    {
        return history[(next + career_history_capacity - 1 - age) % career_history_capacity];
    }

    bool equip_cue(int cue)
    {
        if(equipped == cue || ! owns_cue(cue))
        {
            return false;
        }
        equipped = uint8_t(cue);
        return true;
    }

    bool equip_accessory(int index)
    {
        if(! owns_accessory(index))
        {
            return false;
        }
        outfit = outfit == index + 1 ? 0 : uint8_t(index + 1);
        return true;
    }

    MatchReward record(bool won, int rival, int shots = 0)
    {
        MatchReward reward;
        reward.rating = profile.record(won, rivals[rival].rating);
        history[next] = { int16_t(profile.rating), int8_t(reward.rating), uint8_t(rival), uint8_t(won) };
        next = uint8_t((next + 1) % career_history_capacity);
        if(count < career_history_capacity)
        {
            ++count;
        }
        if(! won)
        {
            streak = 0;
            return reward;
        }
        if(! owns(rival))
        {
            defeated |= uint8_t(1 << rival);
            reward.cue = rival + 1;
            reward.accessory = rival + 1;
        }
        uint8_t& wins = wins_by_rival[rival];
        if(wins < 255)
        {
            ++wins;
        }
        if(streak < 9999)
        {
            ++streak;
        }
        if(streak > best_streak)
        {
            best_streak = streak;
        }
        if(wins == 10)
        {
            reward.milestone = wins;
        }
        reward.challenge = shots > 0 && shots <= 5;
        if(reward.challenge && challenges < 9999)
        {
            ++challenges;
        }
        return reward;
    }
};
}

#endif
