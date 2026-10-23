# Playtest summary: r-20261023-0855

4 sessions, build kindling 0.1.0 (17ed1e8), generated 2026-10-23 09:28.

**Findings:** 27 reports in 26 groups -- 0 blocker, 3 major, 18 minor, 6 polish.

## Outcomes

| Session | Goal | Outcome | Fun | Minutes | Objectives | Deaths | Findings | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s01 | Play the first 15 minutes as a brand-new player who has never seen the game; note every moment of confusion. | achieved | 3 | 14.8 | 1 | 0 | 6 | The tone, the per-tool hints on the crosshair ('Too long to carry. Saw it into pieces (4).') and the status lines are charming and mostly self-explanatory. But a new player's first 15 minutes go almost entirely into hauling debris one piece at a time, with silent failures when E has no valid target (debris buried in snow, or off the crosshair). Explain Survey, give feedback when E has nothing to take, and speed up hauling. Then the opening would feel cozy rather than chore-like. |
| s02 | Finish the Survey and the whole Clear step (haul the debris, shovel the snow, sweep the floor, scrub the walls and the chimney) using only what the screen tells you; record the game time each checklist line completes. Judge especially how forgiving the aim is: picking up debris, shovelling the drift, finding the roof hole in the survey; note every target that still needed a fiddly, exact aim. | partial | 3 | 28.1 | 1 | 0 | 6 | The Survey and the first half of Clear are pleasant. The shovel is forgiving across the whole drift, the feedback lines ('Tossed out the door... 9 pieces left') are good, and sawing the rafter into three pieces is a nice surprise. Aim still bites in three places: the roof hole seen from the door, debris close to your feet (the 12-degree cone includes pitch), and shingles hidden in the snow. The real problem is pacing: sweeping and scrubbing are long hunts for the last few percent, and the walls run so slowly that the Clear step was not finished in ~26 game-minutes. |
| s03 | Your save starts at the Repair step (the cabin is already cleared): finish Repair and then Furnish (planks, roof shingles, window boards, the chair, the table, bringing furnishings in from the wagon and turning them with R); record the game time of each checklist line and note anything about the wagon, carrying stacks of planks or shingles, placing or turning things that was confusing or fiddly. | blocked | 2 | 24.3 | 0 | 0 | 7 | The window and roof jobs feel good: prying boards, the sky opening up and the shingles working in quickly are satisfying. The floor is broken: after the rotten boards are pried up, the open gap can't be targeted, so the planks can't go in and Repair, Furnish and Kindle are blocked. The plank-stack flow around it (no way to set the stack down indoors, planks hard to find on the wagon, rotten boards that look like holes) made the floor the most frustrating part of the session even before it hard-stopped. |
| s04 | Your save starts at the Kindle step: split firewood, stack it, lay and light the fire, welcome the settler and watch the reveal (do not skip it); then spend the rest of your time walking around inside and outside the cabin, judging how it looks and reads (finding the woodpile and kindling, the fire taking hold, light, models, the people and the mare) and noting anything ugly, broken, floating, clipping or hard to read. | achieved | 3 | 23.8 | 2 | 0 | 8 | The Kindle beat pays off. The fire catching in the hearth, the warm doorway at dusk, Ada's shadow on the wall and the reveal's daylight shot of the restored room all look good, and the mare reads well. Most of the 20 minutes went to fighting small hit areas: kindling, split halves, the woodpile and the ash each needed exact aim. Taking the andiron log from the full woodpile un-ticks the stacking item and forces a fifth round. Ada also arrives off-screen, so the welcome falls flat. |

## Findings

### 1. [major] Open floor gap left by a pried-up rotten board cannot be targeted, so new planks can't be laid

- **Category:** bug; **reported by** 1 agent(s): s03
- **Detail:** After prying up all three rotten floorboards and hauling the broken boards out, I fetched the 3-plank stack from the wagon. With the crosshair squarely over the open gap (green ground visible through it), the centre names nothing (aim None) and holding the left button for 4 s does nothing; planksLeft stays 3. I swept pitch in ~5 degree steps from -83 to -34 and yaw across two gaps: the aim was only ever 'Floorboards' or nothing, never 'Gap in the floor' (that string exists in the game's target text). With no other way to lay the planks, Repair can't be finished, and Furnish and Kindle are out of reach. The pick volume for a Removed plank seems to vanish with the board. Expected: 'Gap in the floor - Hold the left button to work the plank in.'
- **Repro:** pt start (s03 Repair save, seed 1881); pry the rotten floorboards near x=1.3,z=-0.3 (5, hold LMB on the dark strip); haul out the broken boards; take 'New plank' from the back of the wagon (E); walk back and aim straight at the gap (pitch about -68); pt mousehold 4 -> no progress, aim None
- **Screenshots:** [plank_gap_no_target.png](s03/shots/plank_gap_no_target.png)

### 2. [major] Plank stack can't be put down indoors although the rotten board tells you to put it down

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** If you fetch planks before prying, the rotten floorboard says 'Put the plank down and pry this up first (5)'. But E anywhere in the cabin, on floor or rotten board, only answers 'Hold the plank over a gap in the floor and work it in (left mouse)'. The only way to free your hands is to walk back to the wagon and press E while aiming at the cart body ('Your wagon'). E while aiming at a furnishing on the wagon (Blue jar, Framed sampler) silently does nothing, and returning the stack gives no confirmation message.
- **Repro:** take planks from wagon; walk in; aim at a rotten floorboard; pt key e -> refusal message; walk to wagon, aim at 'Blue jar', E -> nothing; aim at 'Your wagon', E -> stack returned silently
- **Screenshots:** [plank_cant_put_down.png](s03/shots/plank_cant_put_down.png)

### 3. [major] Taking a log from the full hearth woodpile for the andirons un-ticks 'Split firewood and stack it (4 of 4)'

- **Category:** balance; **reported by** 1 agent(s): s04
- **Detail:** With the woodpile at 4 of 4 (ticked), the woodpile hint says 'Stacked. E takes a log for the fire.' Doing exactly that and laying the log on the andirons drops the checklist back to '(3 of 4)', so the step cannot finish until the player goes back outside, fetches a fifth round, splits it, finds the half on the ground and carries it in. The game invites the action and then penalises it. Expected: the andiron log does not count against the stack, or the checklist asks for 4 + 1 up front.
- **Repro:** pt start (save at Kindle); split and stack until 4 of 4; with bare hands aim at 'Woodpile (4 logs)' beside the hearth, E; checklist shows 3 of 4; lay it on the andirons; item stays unticked
- **Screenshots:** [woodpile_untick.png](s04/shots/woodpile_untick.png)

### 4. [minor] Fallen shingles under the snow drift can't be picked up, and nothing says why

- **Category:** bug, ux; **reported by** 2 agent(s): s01, s02
- **Also reported as:** "Fallen shingles are hidden inside the snow drift and cannot be picked up until it is shovelled"
- **Detail:** Two 'Fallen shingle' pieces lie inside the snow drift's footprint (1.0m, bearing +11/+15 from x=0.41,z=0.15). With the crosshair there it names only 'Snow drift', and E does nothing and shows no message. Several hauling trips failed silently until I shovelled the snow, and then the shingles could be picked up. Expected either a hint like 'buried under snow, shovel first' or for E to take the debris anyway.
- **Repro:** pt start --profile kindling --game-dir <kindling checkout> --seed 3141; finish Survey; haul debris until only the shingles near the snow are left; pt walkto debris; pt key e (nothing happens); pt key 3; pt mousehold 8; pt key grave; pt key e (now works)
- **Screenshots:** [stuck2.png](s01/shots/stuck2.png) [shingle_under_snow.png](s02/shots/shingle_under_snow.png)

### 5. [minor] 'Cold hearth' (shovel the ash) registers only in a narrow pitch band, and the firebox floor reads 'Floorboards'

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** Standing 0.6m from the hearth, 'Cold hearth' appears only at about -46 to -50 degrees of pitch. At -52 the crosshair is visibly on the firebox floor between the andirons but reads 'Floorboards'; above -45 it reads 'Chimney stones'. From 1.3m it says 'Too far to reach. Step closer.' though the interact range is 2.2m. No ash heap is visible to aim at. The kindling, log and match targets later used the same hearth area and were easier.
- **Repro:** Shovel (3); stand in front of the fireplace about 0.6m away; tilt down slowly from -40 to -55 and watch the target name
- **Screenshots:** [ash_shovel_aim.png](s04/shots/ash_shovel_aim.png)

### 6. [minor] Ada's arrival happens off-screen: 'wagon coming up the trail' and 'Ada Lark is here' fire on the same tick

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** When the fire takes hold, both lines appear at tick 45235, and 3 seconds later Ada is already seated in the rocking chair. There is no wagon on the trail and no walk-in, so the welcome beat the text promises is never seen. The crosshair on Ada reads 'Log wall', so she has no name or interaction.
- **Repro:** Complete Kindle while indoors; watch pt events and the screen
- **Screenshots:** [ada_instant.png](s04/shots/ada_instant.png)

### 7. [minor] Debris pickup cone includes pitch: close pieces at your feet need a steep exact look-down

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** E takes debris within ~12 degrees of the crosshair, but that includes the vertical angle. With a board 0.8 m straight ahead (bearing 0) and the view at pitch -43, E did nothing; at 1.4-1.5 m a pitch of -24 also failed. It worked every time once the view was tilted to roughly atan(eye height/distance) (-45 to -62 degrees). Since you naturally walk right up to a piece, the piece ends up under the bottom of the view. About 1 in 4 of my first tries missed. Expected: a larger vertical tolerance, or picking the nearest piece in reach when the crosshair is on the floor near it.
- **Repro:** pt walkto debris; (with pitch about -40 and a board 0.8 m ahead) pt key e -> carrying nothing; pt hold Down 0.4 (pitch about -62); pt key e -> carrying debris
- **Screenshots:** [debris_pitch.png](s02/shots/debris_pitch.png)

### 8. [minor] E next to debris silently does nothing unless you look steeply down

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** After walkto debris (0.89m ahead, bearing 0), E with a level view did nothing and showed no feedback; I had to tilt about 60 degrees down. The last broken board stopped 24 degrees off the crosshair and E failed the same way. No message such as 'nothing in reach' tells the player to aim.
- **Repro:** pt start ... --seed 3141; finish Survey; pt walkto debris; pt hold Up 1 (level view); pt key e -> carrying stays nothing
- **Screenshots:** [carry.png](s01/shots/carry.png)

### 9. [minor] Hauling 15-17 debris one piece per trip takes most of the first session

- **Category:** balance; **reported by** 1 agent(s): s01
- **Detail:** The Clear step's hauling (15 pieces, 17 once the rafter is sawn) ran from tick ~3300 to ~21900, about 10 game minutes of walking back and forth to the door with one piece at a time. A first 15-minute session is mostly this one chore. Carrying a few small pieces at once, or fewer pieces, would keep the pace up.
- **Repro:** pt start ... --seed 3141; finish Survey; repeat: pt walkto debris; pt key e; pt walkto door; pt key e
- **Screenshots:** [last.png](s01/shots/last.png)

### 10. [minor] Hearth woodpile hit area does not match its visible logs; the crosshair on the logs reads 'Floorboards'

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** Carrying a split log, with the crosshair on the top log of the woodpile left of the hearth, the target reads 'Floorboards' and E only shows 'Stack it on the woodpile by the hearth, or lay it in the hearth (E)' without stacking. 'Woodpile (n logs)' only appears in a narrow band of pitch and distance. Each of the 3 stacking trips took several tries.
- **Repro:** Carry a split log in; stand about 1m from the woodpile left of the hearth; aim at the visible logs (pitch about -63); the target reads Floorboards; E does nothing
- **Screenshots:** [woodpile_floor.png](s04/shots/woodpile_floor.png)

### 11. [minor] Kindling bundle and split halves beside the chopping block are not targetable with the crosshair on them

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** After splitting, the kindling bundle lies on the ground by the block. With the crosshair squarely on it (pitch -49, about 1m) no name appears and E does nothing; it only registered at pitch -50.4 after several nudges. A split half on the ground sits behind the held axe model and needed exact aim; E about 8 degrees off picked nothing, despite the promised 'nearest within 12 degrees' forgiveness. The block's own hit area also covers the halves and kindling next to it, so the crosshair keeps reading 'Chopping block - Set a log round on it'.
- **Repro:** At the chopping block put a round on and split it (7, hold mouse); aim at the kindling bundle on the ground; E
- **Screenshots:** [kindling_no_target.png](s04/shots/kindling_no_target.png)

### 12. [minor] Planks on the wagon are hard to find; most of the cart only says 'Your wagon'

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** The hint 'Tools, planks, shingles and furnishings' covers the whole cart, but the plank stack is a small dark box at the back right, easy to miss and nearly invisible at dusk. I grabbed the shingles by accident first and needed several minutes and many aim adjustments to find 'New plank'. Once, pressing E on the plank label did not take it. Pressing E on 'Your wagon' gives no feedback when you carry nothing.
- **Repro:** walkto wagon; sweep yaw at pitch about -17; only a 5-10 degree window at the back right names 'New plank'
- **Screenshots:** [wagon_planks_hard.png](s03/shots/wagon_planks_hard.png)

### 13. [minor] Pressing E at the wagon body does nothing and gives no message

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** With empty hands, E on 'Your wagon' does nothing at all. A short line such as 'Aim at the planks, shingles or a furnishing' would help.
- **Repro:** walkto wagon; aim at the cart bed; pt key e
- **Screenshots:** [wagon_e_silent.png](s03/shots/wagon_e_silent.png)

### 14. [minor] Rotten floorboards look like open holes and only their thin dark strip is targetable

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** Rotten boards render as dark and green squares that look the same as an already-removed gap. Aiming at the middle names nothing; only the narrow dark edge gives 'Rotten floorboard'. I could not tell rotten boards from finished gaps by looking. The checklist says 'Replace the rotten floorboards' but never mentions prying them up first.
- **Repro:** From the s03 save, walk in, look down at the dark/green patch near the hearth and sweep the crosshair across it.
- **Screenshots:** [rotten_looks_like_hole.png](s03/shots/rotten_looks_like_hole.png)

### 15. [minor] Scrubbing the walls is very slow: about 70 s of scrubbing for +9% of the checklist

- **Category:** balance; **reported by** 1 agent(s): s02
- **Detail:** A full raster of one wall section (4 rows x 120 degrees, about 70 s of the mouse held down) took 'Scrub the walls' from 13% to 22%. At that rate the walls alone take about 10 more minutes of holding the button, on top of ~8.5 min for the floor. The chimney (a smaller surface) took about 3 min. Time ran out before the walls were finished. The step feels like a chore compared with the satisfying debris/snow lines.
- **Repro:** pt key 2; stand 1 m from the west wall; raster with pt mousehold while pt face +-20 steps and pt hold Up 0.3 between rows
- **Screenshots:** [walls_slow.png](s02/shots/walls_slow.png)

### 16. [minor] Survey step never says what surveying means; 'Grime' is already ticked at spawn

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** At the start the checklist shows 'STEP: Survey' with Grime/Debris/Rotten floorboards/Hole in the roof/Boarded window, and nothing on screen explains that you have to look at each problem. 'Grime' is ticked by tick 134 only because the spawn view faces a grimy wall, so it looks pre-completed. I only worked it out from items ticking as I looked around.
- **Repro:** pt start --profile kindling --game-dir <kindling checkout> --seed 3141; pt shot start
- **Screenshots:** [survey_unexplained.png](s01/shots/survey_unexplained.png)

### 17. [minor] Survey: roof hole is a tiny sky sliver in a near-black ceiling when seen from the door; needs exact aim

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** From just inside the door the hole in the roof shows as a few blue pixels in an unlit ceiling. With the crosshair ~35 px left of it the label reads 'Ceiling boards'; a 360-degree sweep of the ceiling at pitch ~30 never named it. It was found only by guessing it sits above the snow drift and nudging onto the sliver. Standing directly under it is very forgiving (pitch 62-83 all read 'Hole in the roof'), but nothing on screen tells the player to go under it. Expected: a hole that lets in visible light, or a larger forgiving cone from across the room.
- **Repro:** pt start --profile kindling --seed 202; pt walkto door; pt hold w 1.5; pt hold Up 1.0; turn around the ceiling with pt face 90 x4; then pt face to yaw ~303, pitch ~32: label is 'Ceiling boards'; pt look 60 30 lands on 'Hole in the roof'
- **Screenshots:** [roof_hole_tiny.png](s02/shots/roof_hole_tiny.png)

### 18. [minor] Sweeping: the last 2-3% of the floor is hard to find, with no visible dirt in the dim light

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** Sweep the floor stalled at 97-98% after lawnmower sweeps of the whole room. The remaining patches (by the door and in front of the hearth) look the same as swept floor in the shadowed interior. They were found only by hovering the crosshair and reading the per-spot '[####] 44% clean' meter. That meter is a good tool, but the hunt took several game-minutes. The checklist shows floorClean scaled to the threshold (0.85 = 100%), which is fine. Expected: stronger contrast between dirty and swept boards, or a hint that points to the dirtiest area.
- **Repro:** pt key 1; sweep strips across the room with pt mousehold + pt hold w; the checklist reaches 97%; scan the floor with the crosshair and read the meter
- **Screenshots:** [floor_hunt.png](s02/shots/floor_hunt.png)

### 19. [minor] Window boards: the gaps between boards name nothing, and boards pried off become new debris to haul

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** At eye level the crosshair falls in the gap between boards, so the target name disappears and holding the button does nothing; you have to nudge onto a board. Each board pried off adds to 'Haul out the old wood', which re-opens a ticked line (0 -> 4 pieces). Picking a board up off the floor needs the crosshair within about 12 degrees in 3D: looking straight down at your feet (pitch -83) misses a board 1.3 m ahead, though the help says E takes the nearest piece. The pry and haul loop itself felt fine.
- **Repro:** From the s03 save: pt hold Down 0.15 onto a board, mousehold 6; repeat; look down at pitch -83 and press E near a board -> nothing; pitch -56 -> picks up
- **Screenshots:** [pry4.png](s03/shots/pry4.png)

### 20. [minor] pt walkto door leaves the player against the wall beside the narrow doorway; entering takes repeated lining up

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** Many times 'walkto door' reported within reach, but walking forward slid along the outside log wall or snagged on the frame (for example ending at x 4.9 z 4.1 after holding W from x 1.98 z 5.12). Getting in or out took 2 or 3 tries each trip, which adds up during the 5 or more trips the Kindle step needs between the block and the hearth.
- **Repro:** From the chopping block: pt walkto door; pt hold w 1.5; check the indoors flag
- **Screenshots:** [dd3.png](s04/shots/dd3.png)

### 21. [polish] Firelit chimney stones beside the firebox render as flat saturated yellow slabs

- **Category:** bug; **reported by** 1 agent(s): s04
- **Detail:** After dusk with the fire burning, the stones on the left of the firebox are filled with flat bright yellow and look like a missing texture rather than firelight. The rest of the room past the hearth is nearly black even with the lantern lit. Ada is small, low-poly and flat red; she casts a nice big shadow on the wall.
- **Repro:** After the fire takes hold and dusk falls, stand left of the hearth looking along the mantel
- **Screenshots:** [ada_label.png](s04/shots/ada_label.png)

### 22. [polish] Game slowed to ~6 tps / 2 fps for a while during chimney scrubbing (machine load 23)

- **Category:** perf; **reported by** 1 agent(s): s02
- **Detail:** pt status showed tps 5.8 and fps 2 while the host load average was 23 with other sessions running. It recovered to 30 tps later. This is probably the shared machine and not the game, but it is noted for context.
- **Repro:** pt status during heavy parallel load
- **Screenshots:** [ch3.png](s02/shots/ch3.png)

### 23. [polish] Rotten floorboard survey needs an exact aim, and the patch doesn't read as rot

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** With the crosshair right beside the dark/green checkered patch it still said 'Floorboards'. Only dead on did it name 'Rotten floorboard' and tick the survey item. The patch looks like a shadowed checkerboard rather than rotten wood.
- **Repro:** pt start ... --seed 3141; pt walkto door; pt hold w 1.5; look at the dark patch to the front-left of the door
- **Screenshots:** [rot5.png](s01/shots/rot5.png)

### 24. [polish] Status messages overlap the target prompt, and the checklist is hard to read over a bright window

- **Category:** text; **reported by** 1 agent(s): s03
- **Detail:** 'Light comes through the gap.' is drawn over the 'Board nailed over the window / Pry it off' prompt and its meter. The white checklist text sits over the bright sky in the window and becomes hard to read. The carried shingle and plank stacks fill the lower right quarter of the view and hide the floor gap you are trying to aim at.
- **Repro:** pry window boards one after another
- **Screenshots:** [pry6.png](s03/shots/pry6.png)

### 25. [polish] Swept and unswept floorboards look almost the same in the dim cabin

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** The per-spot meter ('71% clean', 'Clean here. Move on.') and the checklist % work well, but you can hardly see which parts of the floor are still dusty, so sweeping turns into hunting with the meter. After about 4 game minutes of sweeping the floor was 42%.
- **Repro:** pt start ... --seed 3141; reach Clear; pt key 1; sweep the floor with pt mousehold while turning
- **Screenshots:** [final.png](s01/shots/final.png)

### 26. [polish] The 'The fire' target label sits right on top of the flames

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** When the fire catches, the centre target label 'The fire' covers the small flame the player is meant to watch take hold. Expected: the label dims or moves once the fire is lit.
- **Repro:** Strike a match (8, hold) on the laid fire; watch the centre of the screen
- **Screenshots:** [match1.png](s04/shots/match1.png)

## Objective timings

Game seconds from the start of the world to each objective (event tick / the game's ticks per second, from events.jsonl). Game time, not wall time: with many sessions on one machine a world can run slower than real time, and wall-clock timings would mostly measure the machine.

| # | Objective | s01 | s02 | s03 | s04 | min | median | max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Survey | 77 | 66.1 | - | - | 66.1 | 71.5 | 77 |
| 5 | Kindle | - | - | - | 1507.8 | 1507.8 | 1507.8 | 1507.8 |
| 6 | Welcome | - | - | - | 1533.1 | 1533.1 | 1533.1 | 1533.1 |

