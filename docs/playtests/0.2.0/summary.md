# Playtest summary: 0.2.0

4 sessions, build kindling 0.1.2 (2af0a33), generated 2026-10-29 18:18.

**Findings:** 31 reports in 28 groups -- 0 blocker, 2 major, 19 minor, 10 polish.

## Outcomes

| Session | Goal | Outcome | Fun | Minutes | Objectives | Deaths | Findings | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s01 | Play the first 15 minutes as a brand-new player who has never seen the game; note every moment of confusion. | partial | 3 | 21 | 1 | 0 | 8 | Survey-by-looking is a lovely, gentle onboarding: the checklist ticks off as you notice things, the help hides itself, and shovelling the snow drift feels quick and good. Clear then slows to a grind. Cleaning goes spot by spot, debris goes out one piece at a time, and twice the tool or E silently did nothing because the target under the crosshair was not what I meant. Add feedback for a failed E or tool use, and make cleaning broader, before anything else. |
| s02 | Finish the Survey and the whole Clear step (haul the debris, saw the fallen rafter, shovel the snow, sweep the floor, scrub the walls and the chimney) using only what the screen tells you; record the game time each checklist line completes. Say whether sawing keeps its progress when you let go of the button, whether the debris counter ever goes up, and whether you can see which parts of the floor and walls are still dirty. In particular: can you tell grimy log walls from scrubbed ones by eye (up close, from a step back, and with the lantern lit), and how long does 'Scrub the walls' take to reach its goal? | partial | 3 | 25 | 1 | 0 | 7 | The Clear step's feedback is good: the crosshair label/meter, the brown-vs-grey wall grime, and the status lines (rafter becomes three pieces, snow out) make it clear what to do, and sawing keeps its progress when you let go. But 'Scrub the walls' is a long grind (~2% per patch, 10+ minutes projected) and close-range targets (debris at your feet, the snow drift) need steep exact aim; with 16 pieces to haul one at a time, Clear is far longer than the rest of the opening. |
| s03 | Your save (written by the previous release, 0.1.2) starts at the Repair step with the cabin already cleared. First say whether the game loaded it without complaint. Then finish Repair and as much of Furnish as you can: pry up the rotten floorboards and lay new planks in every gap, shingle every open roof section, take down the window boards, mend the chair and table, then bring the furnishings in from the wagon, place them and turn them with R. Record the game time of each checklist line; say plainly whether every floor gap and every roof hole could be found, aimed at and fixed. When you place the framed sampler, hang it on a side log wall (not the chimney) and say whether it shows as a whole flat picture from in front and from the side, or only as a thin strip; also try the chimney breast. | partial | 3 | 27.9 | 0 | 0 | 8 | The 0.1.2 Repair save loaded without any complaint, and the floor, window and chair work feels good: clear prompts, satisfying status lines, and prying a rotten board while carrying planks lays a new one in one go. But I could not see any of the 4 open roof sections from inside the cabin (the ceiling shows only solid 'Ceiling boards'), so Repair stayed unfinished and I never reached Furnish or the sampler test. Aiming at small targets (rotten boards next to gaps, items on a crowded wagon) also needs more care than it should. |
| s04 | Your save starts early in the Kindle step: split firewood, stack it, lay and light the fire, welcome the settler and watch the reveal (do not skip it); then spend the rest of your time walking around inside and outside the cabin, judging how it looks and reads (the fire and the firelit chimney, the settler in her chair, the mare, labels and on-screen text overlapping, light and models, the log walls inside and out) and noting anything ugly, broken, floating, clipping or hard to read. | achieved | 4 | 19.3 | 2 | 0 | 8 | The Kindle finale works and feels good: kindling, the match you hold until it catches, and a fire that grows over ~15 s are a cozy payoff, and the room with rug, quilt, candles and curtains reads well in the replay. The save already had the firewood split and stacked and the log laid, so only kindling and the match were left to do. The weak spots are presentation: the replay's last frame hides the fire behind the player and misses Ada seated, the chimney gives no sign of the fire from outside, and one toast tells you to lay a log you already laid. |

## Findings

### 1. [major] 'Scrub the walls' needs ~10+ game minutes of strafe-and-scrub; each patch adds ~2 checklist %

- **Category:** balance; **reported by** 1 agent(s): s02
- **Detail:** With the scrub brush, each reach-sized patch goes to 'Clean here. Move on.' in ~1.5-2 game s but only raises wallsClean by ~0.01 (checklist ~+2%). 13% -> 26% took ticks ~5600-9980 (~146 game s, of which ~60 s actual scrubbing; the rest repositioning). Reach is short: strafing along a wall at ~1 m drops to 'Too far to reach. Step closer.' as soon as the wall angles away. Extrapolating, reaching 100% is 10+ minutes of repetitive scrubbing, the slowest line in Clear by far. Not completed in this session. Expected a goal reachable in a few minutes, or a wider brush stroke.
- **Repro:** pt start --seed 77; survey; walk inside; pt key 2; face a side wall within ~1 m; loop: pt mousehold 2.5; pt hold d 0.3 -- watch checklist 'Scrub the walls (%)'
- **Screenshots:** [wall_lantern.png](s02/shots/wall_lantern.png)

### 2. [major] Roof holes not visible from inside: ceiling shows only 'Ceiling boards' while checklist says 4 sections open

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** Carrying the 4 shingle bundles, I swept the whole ceiling at pitch 35, 63 and 77 degrees from two spots in the cabin (x1.96 z2.08 near the door and x2.54 z-1.61 near the back). Every view showed solid dark ceiling boards and the crosshair only ever named 'Ceiling boards'; no open sky, no light shaft, no 'open roof' label. A sunlit patch on the floor near the old rotten boards hints at a hole but none could be seen above it. I could not find, aim at or fix any of the 4 roof sections in the time available, so Repair could not be finished. Expected: open roof sections clearly visible (sky showing through) and labelled when aimed at.
- **Repro:** pt start (v1-repair fixture); take shingles from wagon; walk in through door; pt hold Up 1.5; pt face 90 x4; pt hold Up 1.2; pt face 120 x3
- **Screenshots:** [roof_nohole.png](s03/shots/roof_nohole.png)

### 3. [minor] Game ran at 1-2 fps / 2-15 tps on the loaded machine

- **Category:** perf; **reported by** 4 agent(s): s01, s02, s03, s04
- **Also reported as:** "Game ran at 1-2 fps / 6-15 tps; short arrow-key taps were often dropped"; "Game ran at 1-2 fps and 7-14 ticks a second on the loaded machine"; "Game ran at 1-3 fps / 8-17 tps on the loaded machine"
- **Detail:** Software renderer on a shared machine. Turning by arrow keys overshot badly (pt face missed by 20-40 deg), and walking took several real seconds per metre. This is mostly the environment, recorded once.
- **Repro:** pt status during play
- **Screenshots:** [haul5.png](s01/shots/haul5.png) [wallE.png](s02/shots/wallE.png) [c1.png](s03/shots/c1.png) [start.png](s04/shots/start.png)

### 4. [minor] Checklist text is hard to read against the pale lit chimney stones

- **Category:** text; **reported by** 1 agent(s): s02
- **Detail:** Scrubbed chimney stones under the lantern are pale cream; the white checklist text in the top-left (thin dark outline) nearly disappears against them. The held tool/lantern sprite also covers the '8 Matches' toolbar label.
- **Repro:** scrub the chimney with lantern lit (L), look up at the stones
- **Screenshots:** [chimney.png](s02/shots/chimney.png)

### 5. [minor] Chimney shows no smoke or glow from outside once the fire is lit

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** With hearthLit true, the chimney seen from outside is a plain dark grey box: no smoke, sparks or warm glow, so nothing outdoors says the hearth is lit. A thin dashed sliver also sticks out past the right roof eave in the same view, and the 'Log wall, outside' aim label appears from ~10 m away over the scene.
- **Repro:** After the fire takes hold, walk out the door and round the cabin to the hearth end; look up at the chimney
- **Screenshots:** [chimney_nosmoke.png](s04/shots/chimney_nosmoke.png)

### 6. [minor] Debris aim-assist steals the floor target from the broom; sweeping silently does nothing

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Holding the broom with the crosshair visibly on bare floorboards, the centre label read 'Fallen shingle' (a shingle about 1m off-centre). 10s of left-mouse sweeping did nothing: floor stayed at 8% and no message said why. Expected the broom to sweep the floor under the crosshair, or a hint that a piece of debris is in the way. Tilting further down to get 'Floorboards' fixed it.
- **Repro:** pt start --profile kindling --seed 2718; finish Survey; shovel the snow drift near the rocking chair; pt key 1; aim at floor near a fallen shingle; pt mousehold 10
- **Screenshots:** [broom_blocked.png](s01/shots/broom_blocked.png)

### 7. [minor] Debris counter goes UP from 15 to 17 after sawing the fallen rafter; rafter has no checklist line

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** Sawing the fallen rafter (tool 4, hold LMB) turned it into three 'Piece of rafter' debris; 'Haul out the debris' went from 15 left to 17 left. A status line explains it ('three pieces to haul out instead of one'), so it is understandable, but a counter rising reads as regress, and the rafter itself never appears on the Clear checklist so a player does not know it is a task until the crosshair names it.
- **Repro:** pt start --seed 77; complete Survey; pt key 4; aim the crosshair at the diagonal fallen rafter left of the hearth; pt mousehold 8 (x2)
- **Screenshots:** [debris_up.png](s02/shots/debris_up.png)

### 8. [minor] E at your feet picks the aimed far object instead of debris 0.4 m away unless you look nearly straight down

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** Standing next to a fallen shingle (0.4 m, bearing +2) with the view level-ish (pitch -13), pressing E targeted the cold hearth behind it and said 'Too far to reach. Step closer.' Only after tilting to pitch -42 did E pick the shingle up. The 12-degree assist seems to count pitch, so debris right at your feet needs an exact downward aim.
- **Repro:** stand on a fallen shingle (pt walkto debris), face it with pitch about -13, pt key e
- **Screenshots:** [pick1.png](s02/shots/pick1.png)

### 9. [minor] E does nothing, with no feedback, when debris lies at your feet below the crosshair

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** A fallen shingle 0.9m away at bearing -2 deg, view pitched -42 deg: the label read 'Floorboards' (not in reach) and E neither picked it up nor said anything. Tilting to -71 deg so the label read 'Fallen shingle' made E work. The 12-degree leniency seems to be horizontal only. A failed E needs a message (e.g. 'Nothing in reach'), and debris this close should be picked up. The same silent E also happened near the door (shots/stuck_board.png).
- **Repro:** stand beside the hearth next to fallen shingles; look level-ish (pitch about -40); pt key e
- **Screenshots:** [e_silent.png](s01/shots/e_silent.png)

### 10. [minor] E on a broken board reports 'Nothing to pick up there' when the board is in reach but below the view

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** After walking to a broken board (probe distance 1.5 m, bearing -2), pressing E said 'Nothing to pick up there.' The board was just above the bottom of the screen; I had to tilt the view to put it near the crosshair. Expected the forgiving 12-degree pickup to take the only debris in front of me.
- **Repro:** pt walkto debris; pt key e (with the view pitched steeply down)
- **Screenshots:** [haul.png](s03/shots/haul.png)

### 11. [minor] Floor reach is short: rotten board says 'Too far to reach' at about 1.5 m

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** Standing about 1.5 m from a rotten board looking down at ~37 degrees, the label read 'Too far to reach. Step closer.' Only after stepping almost on top of it and tilting to -50 did it come in reach. The interact range for E is 2.2 m but tool work on the floor feels much shorter.
- **Repro:** Walk toward the rotten board near the rocking chair; stop ~1.5 m away with pitch -37
- **Screenshots:** [f3.png](s03/shots/f3.png)

### 12. [minor] Restoration playback never shows the payoff: player blocks the lit fire and Ada is not yet seated

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** The reveal (auto, and again with T) ends on a 12:00 'Welcome' frame where the player's own avatar stands right in front of the burning hearth, hiding the fire, and the rocking chair is empty (Ada sat down at ~12:03). In the auto reveal one frame (10:20 Furnish) had the camera inside the player's top hat, which fills the lower third. Expected the last frame to be the warm room with the fire visible and Ada in her chair, with the player avatar out of shot.
- **Repro:** Finish Kindle; wait for REVEAL (or press T afterwards); screenshot each frame
- **Screenshots:** [replay_grid.png](s04/shots/replay_grid.png)

### 13. [minor] Rotten floorboard looks almost identical to an open gap, and one sits right next to another

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** Rotten boards render as a dark/green-tinted patch, very like the pried-open gap (green ground showing through). The third rotten board is directly adjacent to the first, so after laying a plank the area still looked like an unfilled gap and I only discovered it by aiming at it. Nice touch though: holding the button on it while carrying planks pried it up and laid a new plank in one go.
- **Repro:** From x1.49 z-0.08 facing yaw ~230 pitch -50, pry the board, lay a plank from the wagon stack; the adjacent dark patch is still a rotten board
- **Screenshots:** [plank1.png](s03/shots/plank1.png)

### 14. [minor] Rotten floorboard shows a 'Pry it up (5)' hint during Survey/Clear, but prying is a later step

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Right after Survey, the crosshair on a rotten floorboard says 'Pry it up with the pry bar (5).' while the checklist says Clear. As a new player I took the pry bar and held the mouse; nothing happened ('Too far to reach'). It was not clear whether prying is allowed yet. The hint should say it is for the Repair step, as the furnishings on the wagon do.
- **Repro:** complete Survey; aim at the dark/green floor patch; pt key 5; pt mousehold 6
- **Screenshots:** [clear0.png](s01/shots/clear0.png)

### 15. [minor] Snow drift reads as 'Floorboards'/'Chimney stones'/'Too far' until you stand almost on it at pitch -66

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** With the crosshair visibly on the edge of the snow drift from ~1.5 m, the aim label said 'Chimney stones' and then 'Floorboards'; once it said 'Snow drift' it was 'Too far to reach' even after stepping closer. Shovelling only worked standing on its edge looking steeply down (pitch -66). Once in reach it was quick (~8 game s).
- **Repro:** after Survey: pt key 3; walk to ~1.5 m from the snow pile in front of the hearth; aim at its edge; read the aim label
- **Screenshots:** [snow2.png](s02/shots/snow2.png)

### 16. [minor] Sweeping and scrubbing work one small patch at a time; the Clear step is a long grind

- **Category:** balance; **reported by** 1 agent(s): s01
- **Detail:** Each 2-3 game-seconds of broom or brush cleans one small spot, then the meter says 'Clean here. Move on.' Six floor patches took the floor from 9% to 22%, and three chimney patches took it to 22%. Holding the mouse while strafing did not sweep a trail. With 15 debris pieces each carried singly to the door as well, Clear looks like 100+ aim-hold-move cycles. Cleaned stone does visibly lighten, which is good feedback. A wider sweep, or sweeping while walking, would help.
- **Repro:** pt key 1; aim at floor; pt mousehold 5; move 1m; repeat; read the Sweep the floor % in the checklist
- **Screenshots:** [sweep_rate.png](s01/shots/sweep_rate.png)

### 17. [minor] Toast after laying kindling tells you to lay a log that is already on the andirons

- **Category:** text; **reported by** 1 agent(s): s04
- **Detail:** With the split log already laid (checklist ticked), laying the kindling shows 'Kindling laid. Now a split log or two on the andirons.' The next step is actually to strike a match. Expected the toast to follow the checklist and point to the matches (8, hold).
- **Repro:** pt start (s04 save); pick up Kindling beside the chopping block (E); carry it in through the door; stand ~1m from hearth, aim at 'Clean hearth', E
- **Screenshots:** [laid_msg.png](s04/shots/laid_msg.png)

### 18. [minor] Wagon bed is crowded: picking planks or shingles needs a precise aim among furnishings

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** On the wagon the jar, clock, sampler, candle and crocks sit in front of the plank and shingle stacks; at a few frames a second it took several nudges to get 'Bundle of shingles' or 'New plank' under the crosshair instead of 'Mantel clock' or 'Your wagon'.
- **Repro:** pt walkto wagon; tilt to the bed; try to aim at the shingles
- **Screenshots:** [wg4.png](s03/shots/wg4.png)

### 19. [polish] Cabin interior is very dark at 9 am

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Walking into the cabin at the start, the back wall and floor were near-black (in2.png). It is hard to see the grime and the debris you are asked to survey. The lantern is only suggested at dusk.
- **Repro:** pt start; pt hold w 8; pt shot
- **Screenshots:** [in2.png](s01/shots/in2.png)

### 20. [polish] Camera clips into the mare when standing in reach of her

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** Within reach (~1.5 m) the camera is inside the mare's neck/head, so a dark grey slab fills the left of the screen (shots/mare1.png). From ~3 m she looks fine (shots/mare2.png).
- **Repro:** pt walkto mare; pt shot
- **Screenshots:** [mare1.png](s04/shots/mare1.png)

### 21. [polish] Camera clips into the wagon when you walk up to it

- **Category:** bug; **reported by** 1 agent(s): s01
- **Detail:** At 1.1m from the wagon, the screen fills with flat brown/green shapes and a post. You cannot see the wagon's tools, planks or furnishings, though the label 'Your wagon' shows.
- **Repro:** pt walkto wagon; pt shot
- **Screenshots:** [wagon_clip.png](s01/shots/wagon_clip.png)

### 22. [polish] Carried kindling is not shown in hand; probe calls it 'firewood'

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** While carrying the kindling, only the bottom-line text says so; no bundle shows in the first-person view. state.json player.carrying reports 'firewood' while the screen says 'Carrying kindling'. The kindling bundle also needed a fairly exact aim: E with it ~15 degrees below the crosshair said 'Nothing to pick up there'.
- **Repro:** Walk to chopping block; aim at the Kindling bundle on the ground; E
- **Screenshots:** [carry_kindling.png](s04/shots/carry_kindling.png)

### 23. [polish] Checklist line 'Hole in the roof (stand under it, look up)' uses a smaller font than the other lines

- **Category:** text; **reported by** 1 agent(s): s01
- **Detail:** In the Survey checklist, the roof-hole line is drawn smaller and dimmer than its siblings, presumably shrunk to fit. It reads as a different kind of item.
- **Repro:** pt start; pt shot
- **Screenshots:** [start.png](s01/shots/start.png)

### 24. [polish] Checklist lines alternate between two font sizes; 'Ada arrives' ticked before she arrives

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** Checklist and Ada's brief lines render in two different sizes/weights in no clear pattern (e.g. '[x] Shovel out the old ash' and '[x] Lay a split log' large, other lines small). Right after the fire takes hold 'Ada arrives' is already ticked while the toast says her wagon is still coming up the trail.
- **Repro:** pt start; look at the checklist in the top left; light the fire and screenshot at once
- **Screenshots:** [checklist_fonts.png](s04/shots/checklist_fonts.png)

### 25. [polish] Checklist overlay covers the upper window boards

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** Standing at the window, the top boards sit behind the checklist text in the top-left corner, making it hard to see which board is left. F1 hides help but the checklist stays.
- **Repro:** pt walkto window; tilt up to the boards
- **Screenshots:** [win3.png](s03/shots/win3.png)

### 26. [polish] First checklist line is drawn smaller and indented differently from the rest

- **Category:** text; **reported by** 1 agent(s): s03
- **Detail:** '[ ] Replace the rotten floorboards (3 left)' renders in a smaller, dimmer font than the following lines, squeezed under the STEP header.
- **Repro:** pt start; pt shot
- **Screenshots:** [start.png](s03/shots/start.png)

### 27. [polish] Good: scrubbed vs grimy walls and swept vs dusty floor are readable by eye

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** Scrubbed log walls turn a warm dark brown while grimy logs stay a pale grey; the difference shows up close, from a step back, and with the lantern lit (somewhat lower contrast under the warm light). Dusty floor has brown leaf flecks; swept patches are plainer, but the floor difference is subtler than the walls. The aimed-spot meter ('[##--] 24% clean' / 'Clean here. Move on.') confirms it.
- **Repro:** pt key 2; scrub a patch; step back; press L
- **Screenshots:** [wall_back.png](s02/shots/wall_back.png)

### 28. [polish] Seated Ada is labelled 'Rocking chair'; quilt stands edge-on through the chair back

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** Aiming at Ada in her chair labels her 'Rocking chair' (and 'Floorboards' when slightly off), with the label drawn across her body. Her quilt shows as a thin vertical striped slab standing up behind her shoulder, clipping the chair back rather than draped over it. Her face has no detail at all.
- **Repro:** After Welcome, stand 2-3 m from the rocking chair by the hearth and aim at Ada
- **Screenshots:** [ada_chair.png](s04/shots/ada_chair.png)

## Objective timings

Game seconds from the start of the world to each objective (event tick / the game's ticks per second, from events.jsonl). Game time, not wall time: with many sessions on one machine a world can run slower than real time, and wall-clock timings would mostly measure the machine.

| # | Objective | s01 | s02 | s03 | s04 | min | median | max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Survey | 63.7 | 71.6 | - | - | 63.7 | 67.7 | 71.6 |
| 5 | Kindle | - | - | - | 533.1 | 533.1 | 533.1 | 533.1 |
| 6 | Welcome | - | - | - | 558.5 | 558.5 | 558.5 | 558.5 |

