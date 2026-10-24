# Playtest summary: r-20261023-2221

5 sessions, build kindling 0.1.1 (08933b7), kindling 0.1.1 (da3215f), generated 2026-10-23 23:33.

**Findings:** 35 reports in 34 groups -- 0 blocker, 1 major, 22 minor, 12 polish.

## Outcomes

| Session | Goal | Outcome | Fun | Minutes | Objectives | Deaths | Findings | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s01 | Play the first 15 minutes as a brand-new player who has never seen the game; note every moment of confusion. | partial | 3 | 21.4 | 1 | 0 | 7 | The opening reads well: the Survey checklist teaches what is wrong with the cabin in about a minute, aim hints name the right tool by number, and the rafter-needs-sawing twist is a nice surprise. The Clear step then turns into a long haul of 15+ debris one piece at a time with a fussy pick-up cone (frequent 'Nothing to pick up there' / 'Too far to reach' on visible pieces) and a broom pass with no visible dirt to follow, so after ~11 game minutes Clear was still unfinished. Loosen the vertical pick-up aim and make dirt visible, and the first 15 minutes would feel cozy rather than chore-like. |
| s02 | Finish the Survey and the whole Clear step (haul the debris, shovel the snow, sweep the floor, scrub the walls and the chimney) using only what the screen tells you; record the game time each checklist line completes. Note every target that needed a fiddly, exact aim, and whether you could tell which parts of the floor and walls were already clean. | partial | 3 | 26.6 | 1 | 0 | 6 | The Survey is a nice, forgiving opener (74 s; the roof hole counted just from looking up while turning), and the hint line under the crosshair ('Too long to carry. Saw it...', 'Clean here. Move on.') carries most of the teaching well. The haul loop (15 pieces, about 10 game minutes) dragged because of the rafter: no feedback on E, saw progress lost on release, and a counter that went up. Wall and floor cleanliness are barely visible, so sweeping and scrubbing become a hunt guided only by a percentage. Out of time at floor 17%, walls 16%, chimney 0%; the Clear step was not finished. |
| s03 | Your save starts at the Repair step (the cabin is already cleared): finish Repair and then Furnish (pry up the rotten floorboards and lay new planks in the gaps, shingle the roof, board the window, bring the furnishings in from the wagon, place them and turn them with R); record the game time of each checklist line. Above all, say whether every floor gap could be aimed at and filled, and note anything about the wagon, carrying stacks, placing or turning things that was confusing or fiddly. | partial | 3 | 25.2 | 0 | 0 | 6 | Repair is readable and pleasant when it works. The labels and hints at the centre of the screen ('Mend it with the hammer (6)', 'One more section patched.') make each task clear, and the wagon's stack pickup ('Carrying 4 bundles of shingles') is good. But at 2-5 fps, precise aiming at small targets (wagon items, the table, debris seen from steeply above) took most of the session, and I ran out of time before the floor gaps and Furnish, so I cannot say whether every floor gap can be aimed at and filled. The carried stack hiding the roof hole and the silent out-of-reach table are the two design fixes worth making; note too that the brief says 'board the window' but the game asks you to take the boards off. |
| s04 | Your save starts at the Kindle step: split firewood, stack it, lay and light the fire, welcome the settler and watch the reveal (do not skip it); then spend the rest of your time walking around inside and outside the cabin, judging how it looks and reads (the fire and the firelit chimney, labels and on-screen text overlapping, light, models, the people and the mare) and noting anything ugly, broken, floating, clipping or hard to read. | achieved | 3 | 25 | 2 | 0 | 8 | The Kindle loop is cozy and the payoff is good: the firelight on the stones, the warm glow through the doorway, Ada's line about the quilt and the replay all land. Aiming and reach are the friction. Small ground pieces (split logs, kindling) and the corner woodpile need precise aim or exact standing spots, and 'out of reach' is silent at the woodpile. The biggest visual problem is that Ada, the settler the whole game builds toward, is hidden inside the quilt on her rocking chair. |
| s05 | Your save starts at the Repair step (the cabin is already cleared). First of all, replace the three rotten floorboards: pry each one up with the pry bar (5), haul the broken pieces out, bring the plank stack from the wagon and work a new plank into each gap. Say plainly whether every gap could be aimed at and filled. Then finish the rest of Repair (roof, window boards, chair, table) and as much of Furnish as you can (bring furnishings in from the wagon, place them, turn them with R). Record the game time of each checklist line and note anything confusing or fiddly. | partial | 3 | 26.6 | 0 | 0 | 7 | The repair loop feels good: pry, haul, fetch the stack, work it in, and the flavour lines ('A new board, pale against the old...') land well. Chair and table were quick and satisfying. Every floor gap could be aimed at and filled, but hit areas offset from the drawn rotten patch and gaps, plus strict pickup aim for broken boards, made the floor and hauling fiddly. The two remaining roof holes gave no visual cue at all, which stalled Repair before Furnish could start. |

## Findings

### 1. [major] Saw progress on the Fallen rafter resets to 0% every time the mouse button is released

- **Category:** bug; **reported by** 1 agent(s): s02
- **Detail:** Holding LMB with the saw (4) on the Fallen rafter reached 47% (r2.png) and 52% on two separate 3-5 s holds, but after each release the meter read [----------] 0% again (r4.png), so the work was lost. Only one unbroken ~10 s hold cut it. At low frame rates a player naturally releases and re-presses; other partial jobs (sweeping/scrubbing) keep their progress, so this feels like a bug. Expected: partial cuts persist.
- **Repro:** pt start --profile kindling --game-dir <kindling checkout> --seed 77; finish Survey; walkto debris until 'Fallen rafter' (debris-15, near x=-0.7 z=0.9); pt key 4; pt mousehold 3; pt state (meter 0%); pt mousehold 3; pt shot
- **Screenshots:** [r4.png](s02/shots/r4.png)

### 2. [minor] At 2-5 fps, arrow-key look and aim are unreliable

- **Category:** perf; **reported by** 1 agent(s): s03
- **Detail:** fps 2-5, tps 9-32. Holding Down for 2.0 s moved pitch 83 -> 16 one time and 16 -> -12 the next. A 0.25 s hold sometimes did nothing and sometimes moved 23 degrees. The view kept settling after it looked aimed, so one E press at the wagon landed on 'Your wagon' ('Aim at what you want from the wagon, then E.') instead of the shingles. Some of this comes from the shared machine, but small wagon items and the table need precise aim, which makes it worse.
- **Repro:** pt hold Down 2 / pt hold Down 0.25 repeatedly while pt state shows fps <= 4
- **Screenshots:** [pitch_lag.png](s03/shots/pitch_lag.png)

### 3. [minor] Carried shingle stack hides the roof hole you are trying to patch

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** With the 4-bundle shingle stack in hand, the held model takes up the lower-right third of the screen. Looking up at the roof from inside (pitch ~60-83), the last open roof section sat behind the stack. I had to turn in 15-degree steps with nothing to see until the centre label read 'Hole in the roof'. Expected the carried stack to drop lower or go see-through when the view tilts up, or to sit further to the side.
- **Repro:** pt start ...seed 1881; take shingles from the wagon (aim at 'Bundle of shingles', E); walk into the cabin to the hearth; hold Up until pitch ~80; patch 3 sections; the 4th open section is hidden behind the stack
- **Screenshots:** [roof_occluded.png](s03/shots/roof_occluded.png)

### 4. [minor] Debris counter goes UP from 7 to 9 after sawing the rafter, and the rafter gives no hint until aimed at

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** The checklist read 'Haul out the debris (7 left)'; pressing E on the Fallen rafter gives no on-screen reason (only the aim hint 'Too long to carry. Saw it into pieces (4).' when the crosshair is exactly on it). After sawing, the line jumped to '(9 left)' because the rafter became 3 pieces. A counter going backwards reads as a regression. Expected: an E-press message such as 'Too long to carry' and a counter that does not rise (e.g. count the rafter as 3 from the start).
- **Repro:** pt start ... --seed 77; haul debris until 7 left; walkto Fallen rafter; pt key e (nothing); pt key 4; pt mousehold 10; read checklist
- **Screenshots:** [saw_reset.png](s02/shots/saw_reset.png)

### 5. [minor] Debris pick-up cone is tight vertically: shingles a step away and visible on screen give 'Nothing to pick up there' or 'Too far to reach'

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Indoors, standing 1.6m from a cluster of fallen shingles (inside the 2.2m interact range, horizontal bearing +4deg) with the view tilted to the floor at pitch -57, the shingles sit plainly visible ~40px above the crosshair, yet E says 'Nothing to pick up there.' Tilting up to pitch -44 so the crosshair is near them gives 'Too far to reach. Step closer.' even though the probe reports them within reach. When a piece is at the player's feet (0.3m) it is below the view entirely and E keeps failing until the player looks almost straight down. Expected: E picks up the obviously nearest debris in front of the player, as the forgiving-aim note promises. This was the most frequent friction during the Clear step (about 1 E press in 3 failed).
- **Repro:** pt start --profile kindling --game-dir <kindling checkout> --seed 2718; walk in through the door; pt walkto debris; pt face <bearing>; pt key e (with pitch around -57 and debris at 1.5m, or debris at 0.3m)
- **Screenshots:** [fail1b.png](s01/shots/fail1b.png)

### 6. [minor] E aimed at out-of-reach debris does nothing even when another piece is within reach just below the crosshair

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** Looking at the Fallen shingle on the snow drift ('Too far to reach. Step closer.') with a Broken board 0.96 m away just below the view, E did nothing; it needed a tilt down first. The brief says E takes the debris in reach nearest the crosshair within about 12 degrees; here the out-of-reach target captured the press.
- **Repro:** pt start ... --seed 77; finish Survey from x=2.5 z=0.74; face the snow drift at pitch about -28; pt key e
- **Screenshots:** [d1.png](s02/shots/d1.png)

### 7. [minor] E silently does nothing on a Broken board lying against the broken rocking chair, even with it named and in reach

- **Category:** bug; **reported by** 1 agent(s): s02
- **Detail:** Broken board debris-7 lay against the rocker of the Broken rocking chair. With the crosshair on it reading 'Broken board / Haul it out of the cabin.' and inReach true, E did nothing twice, with no message (q6.png). Earlier, with the crosshair a little high, the chair captured the aim and E also did nothing. It finally picked up after switching tools to bare hands and back (unclear which change fixed it). Expected: E picks up the named debris, or the screen says why not.
- **Repro:** pt start ... --seed 77; after Survey haul debris until 3 left; pt walkto debris (Broken board 1.2 m by the rocking chair); aim at the board (label reads Broken board); pt key e twice
- **Screenshots:** [q6.png](s02/shots/q6.png)

### 8. [minor] Floor sweeping: dirty and swept boards look the same, so the player cannot see where to sweep next

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** With the broom, a 6s hold on one spot cleaned it (floor 9% -> 11%) and the label switched to 'Clean here. Move on.' In the dim midday interior the swept spot and the unswept floor look almost identical, so there is no visual guide to the remaining dirt; at ~2% per spot the 'Sweep the floor' item looks like a long, blind grind. Expected: dust/leaves visibly on the floor that disappear when swept, or a clearer contrast.
- **Repro:** pt start ... --seed 2718; enter the cabin; pt key 1; pt hold Down 0.6; pt mousehold 6
- **Screenshots:** [sweep2.png](s01/shots/sweep2.png)

### 9. [minor] Game ran at 1-4 fps and tick rate dropped well below 30 during the session

- **Category:** perf; **reported by** 1 agent(s): s01
- **Detail:** pt status reported fps 1-4 and tps between 7.6 and ~28 at different times; around 22:39-22:42 wall-clock only ~440 ticks passed in 3 minutes. Key presses and mouse holds were sometimes lost (E presses with no status event at all). Likely made worse by parallel sessions sharing the box, but a 640x360 window at 1-2 fps makes aiming at small debris very hard.
- **Repro:** pt start ... --seed 2718; play the Clear step; pt status periodically
- **Screenshots:** [late.png](s01/shots/late.png)

### 10. [minor] Low frame rate makes fine aiming slow

- **Category:** perf; **reported by** 1 agent(s): s05
- **Detail:** fps was 4-10 with tps 29-33 (it dipped to 4.8 on the status check), on a machine shared with other sessions. The probe's aim and pitch lagged behind key presses, which made small adjustments overshoot.
- **Repro:** pt status during play
- **Screenshots:** [r4.png](s05/shots/r4.png)

### 11. [minor] Overturned table out of reach: E does nothing and no 'Too far to reach' note

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** Aimed at the 'Overturned table' (label grey, hint 'Set it back on its legs (E).') from about 2.5 m, near the window. state.json said inReach false, but the screen showed no 'Too far to reach. Step closer.' note (the broken rocking chair does show one). Pressing E twice did nothing and posted no status, so it looks broken. Afterwards I could not find a spot in reach. Expected the same 'too far' note the chair gives, or a status line on E.
- **Repro:** pt walkto window; face toward the overturned table in the corner left of the window (yaw ~190, pitch ~-31, position x -3.1 z -0.29); pt key e
- **Screenshots:** [table_silent.png](s03/shots/table_silent.png)

### 12. [minor] Picking up broken boards needs a near-exact aim: 'Nothing to pick up there' with the board in plain view

- **Category:** ux; **reported by** 1 agent(s): s05
- **Detail:** Hauling 7 broken pieces (3 floor, 4 window) produced 7 'Nothing to pick up there.' messages. The board sat about 1 m away, 10-15 deg above or below the crosshair, and walkto had reported it 'within reach'. The described 12-degree forgiveness does not seem to cover pitch at close range. Hauling the window boards took about 250 game seconds (ticks 24104-31548), the slowest part of Repair.
- **Repro:** after prying the window boards: pt walkto debris; press e without tilting -> 'Nothing to pick up there.' (event ticks 26483-29960)
- **Screenshots:** [nothing_pickup2.png](s05/shots/nothing_pickup2.png)

### 13. [minor] Prying the window boards unchecks the finished 'Haul out the old wood' line

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** At the start of the save, 'Haul out the old wood (0 pieces)' was ticked. Each window board I pried off fell as a 'Broken board' and the line went back to unticked (4 pieces). It is logical, but a completed checklist line silently undoing itself can feel like lost progress. The broken boards were also hard to pick up while looking steeply down: E said 'Nothing to pick up there.' 4 times at pitch -81 with boards 0.8 m away at bearings -9 to -43. It only worked once the crosshair was right on a board.
- **Repro:** pt start; pt mousehold 6 on 'Board nailed over the window' with the pry bar; watch the checklist; then pt hold Down until pitch ~-80 near the boards and press e
- **Screenshots:** [e1.png](s03/shots/e1.png)

### 14. [minor] Remaining roof holes are invisible from inside: '2 sections open' but no sky or light shows

- **Category:** ux; **reported by** 1 agent(s): s05
- **Detail:** Two roof sections were patched by looking up from about (1.62,0.82) around yaw 340, pitch 45; the first showed only as a small blue corner at the screen edge. After that the checklist still said 2 sections open, but a full 360-degree look at pitch 30-60 showed only dark ceiling boards and rafters, with no sky, light shaft or marker. I could not find the remaining two holes in the time left, so Repair (and Furnish) were not finished.
- **Repro:** carry shingles in, stand at about (1.6,0.8), patch the hole at yaw ~340 pitch 45 twice, then look around at pitch 30-60
- **Screenshots:** [roof_hidden.png](s05/shots/roof_hidden.png)

### 15. [minor] Rotten-floorboard and floor-gap hit areas are offset from the drawn patch

- **Category:** ux; **reported by** 1 agent(s): s05
- **Detail:** Every gap could be aimed at and filled (3/3), but two of the three needed hunting for an invisible hotspot. With the crosshair squarely on the visible dark/green rotten patch (from about 1 m, pitch -64) the aim read 'Floorboards' and holding LMB with the pry bar did nothing. Backing up to about 1.5 m and aiming at the patch's far tip finally read 'Rotten floorboard'. When filling, the crosshair on the visible green gap read 'New floorboard' (shots/g9.png), while plain boards to its lower right read 'Gap in the floor' (shots/gap_offset.png). Expected the drawn patch to be the target.
- **Repro:** pt start --profile kindling --seed 1881; stand at about (-0.5,0.55), face yaw ~293, pitch -54: crosshair beside the gap reads Floorboards; turn right ~15 deg: reads 'Gap in the floor'. For the third gap, from the same spot at yaw 104 the crosshair on the green strip reads 'New floorboard', and at yaw 122 (off the strip) it reads 'Gap in the floor'.
- **Screenshots:** [gap_offset.png](s05/shots/gap_offset.png)

### 16. [minor] Scrubbed wall patches cannot be told apart from grimy logs once you step back

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** Scrubbing a log wall shows 'Clean here. Move on.' and raises the checklist (13% to 16%), but from 1-2 m away the scrubbed patch is not visibly different in the dim interior (wall_vis.png). Only the checklist % shows progress, so a player cannot see which wall areas remain. Swept floor patches do show as soft, lighter circles (sw1.png), but from a few metres they are low contrast too (clean_vis.png).
- **Repro:** pt start ... --seed 77; inside, pt key 2; walk up to a wall; pt mousehold 5; pt hold s 1.2; pt shot
- **Screenshots:** [wall_vis.png](s02/shots/wall_vis.png)

### 17. [minor] Seated settler Ada is hidden inside the quilt on the rocking chair

- **Category:** bug; **reported by** 1 agent(s): s04
- **Detail:** Once Ada 'settles by the fire', the patchwork quilt draped on the rocking chair is drawn straight through her. Only a purple skirt shows below it, with no head or torso, and the crosshair names the quilt, not Ada. This is the emotional payoff moment, and the settler can barely be seen.
- **Repro:** complete Kindle, wait for Welcome and the reveal; look at the rocking chair right of the hearth
- **Screenshots:** [ada_quilt_clip.png](s04/shots/ada_quilt_clip.png)

### 18. [minor] Split log on the ground by the chopping block can't be picked up where it is drawn

- **Category:** bug, ux; **reported by** 1 agent(s): s04
- **Also reported as:** "E on a carried log round near the chopping block put it back on the pile"
- **Detail:** After splitting, one split log lay in the grass beside the block. With the crosshair right beside the drawn log, E said 'Nothing to pick up there' twice (shots/b8.png). The crosshair only named 'Split logs' when pointed at empty grass well below the drawn log (shots/splitlog_hitbox.png). The pick-up target should match where the log is drawn, or the 12-degree forgiveness should cover it.
- **Repro:** pt start --profile kindling --seed 1881 (Kindle save); fetch a log round, set it on the block, axe (7) + mousehold until split; pick up one half (E); come back and aim at the remaining half on the grass, press E
- **Screenshots:** [splitlog_hitbox.png](s04/shots/splitlog_hitbox.png) [onblock.png](s04/shots/onblock.png)

### 19. [minor] Under load, held inputs are lost: arrow-key tilt and match holds

- **Category:** perf; **reported by** 1 agent(s): s04
- **Detail:** With the machine busy (fps 2-3, tps dipping to 0.4-0.7 at times), 'hold Down 0.4' tilted the view only about 2 degrees instead of about 23. Two 6-second match holds ended in 'The match goes out' after under 50 game ticks, because the game had stalled. Lighting took 4 tries. Input that is timed in game ticks rather than wall time punishes slow machines.
- **Repro:** on a loaded machine: pt key 8; aim at the laid fire; pt mousehold 6; pt status during the hold
- **Screenshots:** [match1.png](s04/shots/match1.png)

### 20. [minor] Wagon 'within reach' per walkto, but E says 'Too far to reach' and needs a precise aim at the stack

- **Category:** ux; **reported by** 1 agent(s): s05
- **Detail:** walkto wagon stopped 1.5 m away ('within reach'), but aiming at the planks gave 'Too far to reach. Step closer.' Another step was needed. Then the crosshair had to sit exactly on the shingle bundle: one pixel above it the game said 'Aim at what you want from the wagon, then E.'
- **Repro:** pt walkto wagon; aim at plank stack; e
- **Screenshots:** [wagon5.png](s05/shots/wagon5.png)

### 21. [minor] Woodpile beside the hearth is out of reach at what looks like arm's length, and nothing says to step closer

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** Standing right in front of the woodpile rack left of the hearth, the label read 'Woodpile (2 logs) / Stack split logs here (E)', but E only repeated the carry hint 'Stack it on the woodpile by the hearth, or lay it in the hearth (E)'. There was no 'Too far to reach' note, although the hearth shows one in the same situation. I had to walk into the corner before it took the log. Out of reach should say so.
- **Repro:** carry a split log in; walk to the woodpile left of the hearth until it fills the lower middle of the view (about 1 m); press E
- **Screenshots:** [pilereach.png](s04/shots/pilereach.png)

### 22. [minor] walkto/door path from the wagon got stuck on the outside wall corner while carrying

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** Carrying the shingle stack back from the wagon, I ended up against the 'Log wall, outside' next to the door. The door was 2.1 m away at bearing -6, but I could not get closer until I backed off and came at it from another angle. The doorway is narrow compared to how hard it is to judge your position from the first-person view.
- **Repro:** take shingles at the wagon; pt walkto door; pt walkto hearth (reports stuck at 7 m)
- **Screenshots:** [door2.png](s03/shots/door2.png)

### 23. [polish] Aim label and screen lag the view at 2 fps, so the edge of the snow drift was hard to hit

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** At fps 2 / tps 14 the screenshot showed the crosshair at the edge of the snow drift while the label still read 'Floorboards' (sn4/sn5.png). It took three turns before the label read 'Snow drift'. Once on it, shovelling was quick (about 6 s game time). The drift's footprint was forgiving; the low frame rate was the problem.
- **Repro:** pt start ... --seed 77; pt walkto hearth; pt key 3; face right toward the drift and tilt down
- **Screenshots:** [sn5.png](s02/shots/sn5.png)

### 24. [polish] Boarded window is small and dark and the aim label sits on top of it

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Looking at the boarded window from across the room, it is a small dark patch directly under the crosshair, so the centred 'Log wall' label covers it; the checklist ticked it but the player can barely see what they surveyed.
- **Repro:** pt start ... --seed 2718; enter; pt face -7 (toward window); pt hold Up 0.75
- **Screenshots:** [win.png](s01/shots/win.png)

### 25. [polish] Carried plank stack hides much of the floor view when looking for gaps

- **Category:** ux; **reported by** 1 agent(s): s05
- **Detail:** With 3 planks in hand, the plank model covers the lower-right third of the screen. That is exactly where floor gaps appear when you look down to aim.
- **Repro:** take the plank stack from the wagon and look down indoors
- **Screenshots:** [h2.png](s05/shots/h2.png)

### 26. [polish] Current checklist line is drawn in a smaller, dimmer font squeezed under the STEP header

- **Category:** text; **reported by** 1 agent(s): s05
- **Detail:** The first line ('Replace the rotten floorboards (3 left)') and later the active 'Patch the roof' line render smaller or grey compared with the other lines. That reads as disabled or done rather than 'next'.
- **Repro:** pt start ...; pt shot start
- **Screenshots:** [checklist_line1.png](s05/shots/checklist_line1.png)

### 27. [polish] HUD checklist and clock are unreadable over the light-grey stone chimney

- **Category:** text; **reported by** 1 agent(s): s04
- **Detail:** The checklist and the date/time are light text with no backing panel or outline. Standing at the hearth, they disappear into the grey chimney stones, and 'Lay kindling in the hearth' and '11 November' are hard to make out. Text over the dark log walls reads fine. A drop shadow or dim panel would fix it.
- **Repro:** pt walkto hearth; look at the chimney; read the top-left checklist
- **Screenshots:** [hud_on_stone.png](s04/shots/hud_on_stone.png)

### 28. [polish] Hearth hint asks to shovel out the ash but the Clear checklist does not list it

- **Category:** text; **reported by** 1 agent(s): s01
- **Detail:** Aiming at the cold hearth during Clear shows 'Full of old ash. Shovel it out (3).', but the Clear checklist only lists debris, snow, floor, walls and chimney. A new player does not know whether the ash belongs to this step or a later one (it appears to be Kindle work).
- **Repro:** pt start ... --seed 2718; survey; pt key 3; aim at the hearth
- **Screenshots:** [snow1.png](s01/shots/snow1.png)

### 29. [polish] Help panel covers the middle of the screen on first entry

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** The controls help shown at start sits across the lower-centre of the view, overlapping the aim label and the hearth when you first step inside. F1 hides it (and this is said), so it is workable, but a new player's first view of the cabin is half text.
- **Repro:** pt start ... --seed 2718; pt hold w 2; pt shot
- **Screenshots:** [in1.png](s01/shots/in1.png)

### 30. [polish] Mare model reads poorly up close: dark, head/neck looks like a detached wedge

- **Category:** bug; **reported by** 1 agent(s): s04
- **Detail:** At about 2.5 m in her paddock, the mare is an almost black silhouette, and her head and neck are a thin bent wedge that looks separate from the body. She also still asks for hay after the ending, which is fine but goes unprompted.
- **Repro:** after the reveal, from the yard turn toward the mare (Mare ~2.5 m)
- **Screenshots:** [mare_model.png](s04/shots/mare_model.png)

### 31. [polish] Same status message stacks up repeatedly under an aim hint that already says it

- **Category:** text; **reported by** 1 agent(s): s01
- **Detail:** Pressing E twice at the fallen rafter shows 'The rafter is too long to carry. Saw it into pieces (4).' twice as status lines, directly below the aim hint that reads 'Too long to carry. Saw it into pieces (4).' Three copies of one message. Expected: identical status lines de-duplicated (or refreshed).
- **Repro:** pt start ... --seed 2718; enter cabin; aim at the fallen rafter near the hearth; pt key e; pt key e
- **Screenshots:** [rafter2.png](s01/shots/rafter2.png)

### 32. [polish] Stale hints after the ending: outside wall says 'the inside needs scrubbing'; no chimney or smoke seen from the yard

- **Category:** text; **reported by** 1 agent(s): s04
- **Detail:** After 'STEP: Done', hovering the outside log wall still says 'Weathered, not dirty: the inside needs scrubbing.' From the front yard I saw no chimney stack or smoke above the roof, so outside, the lit hearth only shows through the open door. The glow through the doorway is lovely, though.
- **Repro:** after the reveal, walk out the door ~3 m, turn back to face the cabin
- **Screenshots:** [outside_done.png](s04/shots/outside_done.png)

### 33. [polish] Taking a log from the full woodpile briefly un-ticks the 'stack 4 of 4' checklist item

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** The woodpile says 'Stacked. E takes a log for the fire.' Taking that log drops the item from [x] 4 of 4 to [ ] 3 of 4 until the log is laid on the andirons, then it re-ticks. It reads as lost progress for a step the game told you to do.
- **Repro:** with 4 logs stacked and kindling laid, E at the woodpile; read the checklist
- **Screenshots:** [uncheck_stack.png](s04/shots/uncheck_stack.png)

### 34. [polish] Top checklist line is drawn over by 'STEP: Repair' and is hard to read

- **Category:** text; **reported by** 1 agent(s): s03
- **Detail:** The first item, 'Replace the rotten floorboards (3 left)', is drawn in a smaller, dimmer font right under the STEP header and overlaps it, so it reads worse than the other lines. It is the item I most needed to see.
- **Repro:** pt start; pt shot start
- **Screenshots:** [start.png](s03/shots/start.png)

## Objective timings

Game seconds from the start of the world to each objective (event tick / the game's ticks per second, from events.jsonl). Game time, not wall time: with many sessions on one machine a world can run slower than real time, and wall-clock timings would mostly measure the machine.

| # | Objective | s01 | s02 | s03 | s04 | s05 | min | median | max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Survey | 69.5 | 74.4 | - | - | - | 69.5 | 72 | 74.4 |
| 5 | Kindle | - | - | - | 946 | - | 946 | 946 | 946 |
| 6 | Welcome | - | - | - | 971.1 | - | 971.1 | 971.1 | 971.1 |

