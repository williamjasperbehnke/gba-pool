# Break Room

Break Room is an 8-ball game with ranked solo play and two-player Link Cable matches for Game Boy Advance, written in C++ with Butano. Press Select during a match to switch between the camera behind the cue ball and the overhead view. The close camera follows each player as they aim, holds through the shot, and eases into the next turn. Ball textures rotate with their movement. Open [break_room.gba](break_room.gba) in mGBA or load it on a GBA flashcart with SRAM support.

## Controls

| GBA button | Action |
| --- | --- |
| L / R | Aim left or right. Tap to nudge by about a third of a degree. Hold to accelerate into a faster sweep. |
| L + R while aiming | Activate your equipped signature cue's ability, once per solo rack. |
| D-pad while aiming | Move the red contact dot on the cue-ball preview in the bottom-right corner. Left and Right add sidespin, Up adds follow, and Down adds draw. Tap for one step or hold to repeat. Use the opposite direction to return toward center. |
| D-pad and A in menus | Move the gold cursor and select the highlighted option. B goes back or resumes play from pause. |
| HUD option while paused | Switch between the full and compact HUDs. The compact HUD keeps the ball rows and power meter visible. |
| L / R while paused | Cycle available pocket lines and set their suggested power. |
| Hold A, then release | Charge and shoot. Aim adjustments still work while holding A. With the HUD visible, the meter fills while you hold A. A tap uses the starting or suggested power. |
| B | Cancel while charging. While aiming, change the called 8-ball pocket or reset power on other shots. |
| D-pad, then A | Move and place the cue ball when you have ball in hand. |
| Start | Pause during a match. Select on the pause screen opens the controls. |
| Select during a match | Toggle the close and overhead views while aiming. |
| Select from the lobby | Show the controls. |

In the overhead view, a thin line shows the cue ball’s direction to its first contact. A hollow cue-ball preview marks the contact position when it fits. The line turns red for an illegal first hit and stays the same at every power level. The selected 8-ball pocket lights up in gold. The HUD shows the remaining balls once solids and stripes are assigned. During ball in hand, the cue ball flashes red over a blocked spot and pressing A there plays a rejection sound. Ball-in-hand uses the normal overhead view and returns to your aiming camera after you place the cue ball.

## Matches and rating

The table stays open on the break. The first object ball potted on a legal shot after the break assigns solids and stripes. Pot your group to keep shooting. A scratch, no contact, hitting the wrong group first, or failing to reach a rail or pocket a ball after contact hands your opponent the cue ball. Clear your group before shooting at the 8. Pocket selection opens in the overhead view. Use Right and Left to move within the selected row, or Up and Down to switch rows. Press A to call the selected pocket and return to your chosen aiming view. Sinking the 8 in a different pocket, early, or on a foul loses the match. An 8 on the break is respotted. Solo breaks alternate.

Select **Rivals** on the title screen to browse rivals with **Left / Right**. Beat each rival once to unlock the next. Select **Next Match** on the result screen to continue, or revisit defeated rivals from the ladder. Elo measures your match results independently of ladder access. Open **Record** from the title screen for the rating graph, the 20-match history, and badges. **Start** launches a quick match against your current rival and **Select** opens Controls.

The first win against each rival unlocks their cue and cosmetic. Open **Gear** to preview and equip them. Press **L** or **R** to switch between Cues and Wardrobe. In Cues, Power changes shot speed, Spin changes the strength of follow, draw, and sidespin, and higher Control charges more slowly for finer power adjustments. **Select** shows the signature ability. Sunglasses are available from the start, the Chalk Headband unlocks with your first rival win, and beating Finn earns the Gold Visor. Tap **L + R** while aiming after the break to activate a signature ability once per solo rack.

| Rival | First-win rewards | Ability |
| --- | --- | --- |
| Pip the Otter | River Reed and River Vest | **River Run** saves one clean miss after you pot a ball on the first shot after activation. |
| Nori the Penguin | Frost Tip and Ice Bow Tie | **Ice Bank** shows cue-ball rebounds and object-ball paths for your next two shots in the overhead view with sidespin centered. |
| Ember the Fox | Foxtail and Fox Ears | **Tailspin** boosts spin by 50% for your next shot and sets draw if the spin dot is centered. |
| Ink the Octopus | Inkwood and Ink Cap | **Long Reach** lets you reposition the cue ball to any legal spot on the table. |
| Finn the Shark | Reef Fang and Reef Fin | **Surge** adds 20% speed to your next shot. |

Rivals use their own abilities in solo play. Ink and Finn check difficult shots with the live table physics and look for bank pots when a direct shot is blocked. Link Cable matches use the house cue. Mastery badges are optional rematch rewards.

Career progress saves to SRAM. Saves from the previous roster retain ratings, match history, and streak records. The animal ladder and its rewards start with Pip.

The [fully unlocked save](break_room_unlocked.sav) includes all five rivals, cues, cosmetics, and badges. To use it alongside your own career, copy the ROM as `break_room_unlocked.gba` and open that copy next to the downloaded save.

The game loads the current animal-rival save format. Older save formats and records containing retired rivals start a fresh career.

## Link Cable

Run the same ROM on two Game Boy Advance systems, using one copy per system and a GBA Link Cable. Select **Link Cable** on both title screens. The match starts when both systems connect, and the first GBA on the cable breaks. Each player aims and shoots from their own GBA when their turn comes up. Ball-in-hand placement and 8-ball calls are sent with the shot. Both GBAs need the same ROM version.

Press **Start**, select **Leave Match**, and confirm to leave a linked match. Link matches play under the same house rules and have no effect on the solo rating. In mGBA, connect two instances through its multiplayer window and choose **Link Cable** in both. [Butano Link Cable API](https://gvaliente.github.io/butano/group__link.html)

## Build

Install devkitARM and the GBA tools through devkitPro. The default Makefile path is `../gba_development/butano/butano`. For a different Butano checkout, point `LIBBUTANO` to the directory containing `butano.mak`. [Butano devkitARM setup](https://gvaliente.github.io/butano/getting_started_dka.html)

```sh
make -j8 LIBBUTANO=/path/to/butano/butano
```

The original indexed art and WAV effects are included in `graphics/` and `audio/`. Regenerate them with `python3 tools/make_assets.py` if you have Pillow installed.

Table physics and ray queries live in `include/pool.h` and `src/pool.cpp`. The `EightBall` rules, shot planner, and rating calculation build on that table. Camera projection and ball rotation live in `src/pool_view.cpp`, and `src/aim_table.cpp` renders the 3D table. The portable `Career` record in `include/pool_career.h` handles rewards and history. `src/main.cpp` connects the rules, menus, and versioned SRAM saves; `src/pool_cable.cpp` handles Link Cable transport.

Run the host-side rules, physics, and rival tests with a C++17 compiler.

```sh
c++ -std=c++17 -O2 -Iinclude src/pool.cpp src/pool_view.cpp tests/pool_tests.cpp -o /tmp/break-room-tests
/tmp/break-room-tests
```
