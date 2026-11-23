# Break Room

Break Room is an 8-ball game with ranked solo play and two-player Link Cable matches for Game Boy Advance, written in C++ with Butano. Press Select during a match to switch between the camera behind the cue ball and the overhead view. The selected view stays active through shots and pocket calls and resumes after ball in hand. The close camera frames the far rail and holds position as a shot plays out. Ball textures rotate with their movement. Open [break_room.gba](break_room.gba) in mGBA or load it on a GBA flashcart with SRAM support.

## Controls

| GBA button | Action |
| --- | --- |
| Left / Right | Tap to nudge the aim by about a third of a degree. Hold to sweep across the table. |
| L / R | Fine aim with unaccelerated repeat. |
| L + R together | Switch between the full and compact HUDs. The compact HUD keeps the ball rows and power meter visible. |
| Up / Down while aiming | Cycle available pocket lines and set their suggested power. |
| Hold A, then release | Charge and shoot. Aim adjustments still work while holding A. With the HUD visible, the meter fills while you hold A. A tap uses the starting or suggested power. |
| B | Cancel while charging. While aiming, change the called 8-ball pocket or reset power on other shots. |
| D-pad, then A | Move and place the cue ball when you have ball in hand. |
| Start | Pause during a match. Select on the pause screen opens the controls. |
| Select during a match | Toggle the close and overhead views while aiming. |
| Select from the lobby | Show the controls. |

In the overhead view, a thin line shows the cue ball’s direction to its first contact. A hollow cue-ball preview marks the contact position when it fits. The line turns red for an illegal first hit and stays the same at every power level. The selected 8-ball pocket lights up in gold. During ball in hand, the cue ball flashes red over a blocked spot and pressing A there plays a rejection sound. Ball-in-hand uses the normal overhead view and returns to your aiming camera after you place the cue ball.

## Matches and rating

The table stays open on the break. The first object ball potted on a legal shot after the break assigns solids and stripes. Pot your group to keep shooting. A scratch, no contact, hitting the wrong group first, or failing to reach a rail or pocket a ball after contact hands your opponent the cue ball. Clear your group before shooting at the 8. Pocket selection opens in the overhead view. Use D-pad Right and Left to cycle around the six pockets or Up and Down to jump across the table. Press A to call the selected pocket and return to your chosen aiming view. Sinking the 8 in a different pocket, early, or on a foul loses the match. An 8 on the break is respotted. Solo breaks alternate.

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

Table physics and ray queries live in `include/pool.h` and `src/pool.cpp`. Camera projection and ball rotation live in `include/pool_view.h` and `src/pool_view.cpp`. The 8-ball rules build on that table via `EightBall`. The Butano input, rendering, and SRAM integration are in `src/main.cpp`.

Run the host-side rules, physics, and rival tests with a C++17 compiler.

```sh
c++ -std=c++17 -O2 -Iinclude src/pool.cpp src/pool_view.cpp tests/pool_tests.cpp -o /tmp/break-room-tests
/tmp/break-room-tests
```
