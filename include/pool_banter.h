#ifndef POOL_BANTER_H
#define POOL_BANTER_H

#include "pool.h"

namespace pool
{
enum class Banter
{
    intro, player_pot, player_foul, rival_pot, rival_foul, on_eight, player_win, rival_win,
    ability, combo_saved, player_helped_me, i_helped_you
};

inline constexpr const char* rival_banter[][12] = {
    { "READY TO MAKE WAVES?", "NICE POT!", "THAT ONE GOT AWAY.", "DOWN THE RIVER!", "I LOST THE CURRENT.",
      "JUST THE EIGHT NOW.", "YOU WENT WITH THE FLOW.", "WHAT A RIVER RUN!", "LET'S GET A RUN GOING.",
      "THE CURRENT CARRIES ME!", "THANKS FOR POTTING MINE!", "THAT POT WAS YOURS." },
    { "COOL HEAD. STEADY CUE.", "CLEAN AS ICE.", "SLIPPED A LITTLE?", "NEAT AND TIDY.", "THAT WAS SLIPPERY.",
      "STAY COOL ON THE EIGHT.", "WELL PLAYED.", "ICE COLD FINISH.", "WATCH THE ICE BANK.",
      "", "THAT WAS ONE OF MINE.", "THAT ONE WAS YOURS." },
    { "BET YOU MISS MY ANGLE.", "YOU FOUND THE GAP.", "A LITTLE TOO CLEVER.", "QUICK AS A FOX.", "I GOT GREEDY.",
      "CAN YOU FINISH IT?", "YOU CAUGHT ME.", "OUTFOXED YOU.", "HERE COMES TAILSPIN.",
      "", "THANKS FOR THE GIFT.", "I POTTED ONE FOR YOU." },
    { "EIGHT ARMS. ONE CUE.", "THAT WAS SHARP.", "NEED A HAND?", "RIGHT WHERE I WANT IT.", "ARMS GOT TANGLED.",
      "MAKE THE LAST ONE COUNT.", "I TIP MY CAP.", "THAT'S A WRAP.", "I CAN REACH THAT.",
      "", "YOU CLEARED ONE FOR ME.", "MY MISTAKE. YOUR BALL." },
    { "WELCOME TO THE REEF.", "GOOD HUNT.", "THAT WAS A BAD ANGLE.", "RIGHT ON TARGET.", "I MISREAD THE TIDE.",
      "ONE LAST SHOT.", "THE REEF IS YOURS.", "THE REEF HOLDS.", "HERE COMES THE SURGE.",
      "", "YOU FED THE SHARK.", "YOU CAN HAVE THAT ONE." }
};
static_assert(sizeof(rival_banter) / sizeof(*rival_banter) == rival_count);

constexpr const char* banter_line(int rival, Banter event)
{
    return rival_banter[rival][int(event)];
}
}

#endif
