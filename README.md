# Break Room

Break Room is an 8-ball game with ranked solo play and two-player Link Cable matches for Game Boy Advance, written in C++ with Butano. Open [break_room.gba](break_room.gba) in mGBA or load it on a GBA flashcart with SRAM support.

## Controls

| GBA button | Action |
| --- | --- |
| Left / Right | Tap to nudge the aim by about a third of a degree. Hold to sweep across the table. |
| L / R | Fine aim with unaccelerated repeat. |
| Up / Down while aiming | Cycle available pocket lines and set their suggested power. |
| Hold A, then release | Charge and shoot. Aim adjustments still work while holding A. The meter fills while you hold A. A tap uses the starting or suggested power. |
| B | Cancel while charging or reset power while aiming. |
| D-pad, then A | Move and place the cue ball when you have ball in hand. |
| Start | Pause during a match. |
| Select on the 8 | Change the called pocket before shooting. |
| Select from the lobby | Show the controls. |

The mint aim line shows the cue path and its first cushion rebound. Amber shows the object ball’s direction, and red warns of an illegal first hit. The guide stays the same at every power level and hides when the cue ball starts very close to contact. Mint brackets mark a pocket you have called for the 8. During ball in hand, red brackets mark an invalid placement.

## Matches and rating

The table stays open on the break. The first object ball potted on a legal shot after the break assigns solids and stripes. Pot your group to keep shooting. A scratch, no contact, hitting the wrong group first, or failing to reach a rail or pocket a ball after contact hands your opponent the cue ball. Clear your group before shooting at the 8. Use the D-pad to select a pocket and press A to call it. Sinking the 8 in a different pocket, early, or on a foul loses the match. An 8 on the break is respotted. Solo breaks alternate.

You start at 850 rating against Lou. Wins and losses change your rating according to the strength of your opponent. Your best rating unlocks five increasingly accurate rivals with better shot selection and cue-ball placement. Rating, best rating, and your win-loss record save automatically to SRAM after each match.

## Link Cable

Run the same ROM on two Game Boy Advance systems, using one copy per system and a GBA Link Cable. Press **B** for **Link Cable** from both title screens. The match starts when both systems connect, and the first GBA on the cable breaks. Each player aims and shoots from their own GBA when their turn comes up. Ball-in-hand placement and 8-ball calls are sent with the shot. Both GBAs need the same ROM version.

Use **Start**, **B**, then **A** to leave a linked match. Link matches play under the same house rules and have no effect on the solo rating. In mGBA, connect two instances through its multiplayer window and choose **Link Cable** in both. [Butano Link Cable API](https://gvaliente.github.io/butano/group__link.html)

## Build

Install devkitARM and the GBA tools through devkitPro. The default Makefile path is `../gba_development/butano/butano`. For a different Butano checkout, point `LIBBUTANO` to the directory containing `butano.mak`. [Butano devkitARM setup](https://gvaliente.github.io/butano/getting_started_dka.html)

```sh
make -j8 LIBBUTANO=/path/to/butano/butano
```

The original indexed art and WAV effects are included in `graphics/` and `audio/`. Regenerate them with `python3 tools/make_assets.py` if you have Pillow installed.

The table, ball motion, and ray queries live in `include/pool.h` and `src/pool.cpp`. The 8-ball rules build on that table via `EightBall`. The Butano input, rendering, and SRAM integration are in `src/main.cpp`.

Run the host-side rules, physics, and rival tests with a C++17 compiler.

```sh
c++ -std=c++17 -O2 -Iinclude src/pool.cpp tests/pool_tests.cpp -o /tmp/break-room-tests
/tmp/break-room-tests
```
