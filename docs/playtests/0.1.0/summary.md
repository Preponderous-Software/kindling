# Playtest summary: r-20261022-2117

4 sessions, build kindling 0.1.0 (cfae820), generated 2026-10-22 22:00.

**Findings:** 33 reports in 31 groups -- 0 blocker, 6 major, 20 minor, 7 polish.

## Outcomes

| Session | Goal | Outcome | Fun | Minutes | Objectives | Deaths | Findings | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s01 | Play the first 15 minutes as a brand-new player who has never seen the game; note every moment of confusion. | achieved | 2 | 14.9 | 1 | 0 | 10 | The premise and the feedback text are warm and clear ('Tossed out the door: a broken board. 9 pieces left inside.', 'Clean here. Move on.'), and the lantern and survey reveal are nice touches. But a new player's first 15 minutes are mostly spent fighting the crosshair: picking up thin shingles and chasing a shrinking snow drift needs pixel-exact aim, so I finished only Survey and about half of the debris before night fell. A more forgiving E pickup (nearest item in front, within reach) and stable hit areas would change the feel of the opening more than anything else. |
| s02 | Finish the whole Clear step (haul the debris, shovel the snow, sweep the floor, scrub the walls and the chimney) using only what the screen tells you; record the game time each checklist line completes, and note every moment the on-screen feedback (the progress meter under the crosshair, the 'Too far to reach' note, dust, the done messages) left you unsure what to do or whether it was working. | partial | 3 | 28.5 | 1 | 0 | 8 | The Clear step mostly reads well on screen: the per-patch meter, 'Clean here. Move on.', 'Too long to carry. Saw it (4)' and the 'Tossed out the door... N pieces left' messages are clear and satisfying. Where it falls down is on unclear targets: snow has no overall percentage and its last drift hides behind the HUD, sweeping finishes at an unstated 85%, and the exterior walls say 'Clean here' but don't count, which cost me the walls and chimney lines in this session. I also found dropped key taps at low fps. |
| s03 | Your save starts at the Repair step (the cabin is already cleared): finish Repair and then Furnish (planks, roof shingles, window boards, the chair, the table, bringing furnishings in from the wagon and turning them with R); record the game time of each checklist line and note anything about carrying, placing or turning things that was confusing or fiddly. | partial | 2 | 28.2 | 0 | 0 | 6 | Repair is readable and the per-line status messages are satisfying, but one-bundle-per-roof-section trips to a cluttered wagon, fast nightfall that hides the roof holes, and a held plank that hides its own target make it a slog; I could not get a plank to take at the window gap at all (no aim name), which blocked Repair and so Furnish. No objective_completed events were logged for checklist lines, only status text. |
| s04 | Your save starts at the Kindle step: lay and light the fire, welcome the settler and watch the reveal (do not skip it); then spend the rest of your time walking around inside and outside the cabin as evening falls, judging how it looks (light, models, the fire, the people and the mare) and noting anything ugly, broken, floating, clipping or hard to read. | achieved | 3 | 18.9 | 2 | 0 | 9 | The Kindle chores (split, haul, shovel, lay, strike) feel tactile, and the lit hearth, the timelapse reveal, the snowy dawn yard and seated Ada are charming. But the ending is undercut. The finished checklist never leaves the screen, so you can't see the cabin you made, and the welcome fire dies within a minute of the reveal. Fix those two, and make the woodpile/kindling easier to find and the '4 logs' count honest, and the finale would land. |

## Findings

### 1. [major] Exterior log walls answer 'Clean here. Move on.' to the scrub brush but never count toward 'Scrub the walls'

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** Scrubbing along the walls, I drifted out the door and kept scrubbing the outside of the cabin. Aim showed 'Log wall' / 'Clean here. Move on.', exactly what a finished interior wall shows, while the checklist stayed at 'Scrub the walls (31%)' for about 6 real minutes (ticks ~36100-49600). Expected: the outside wall either has no scrub interaction, or a note like 'Only the inside walls need scrubbing'. Wasting minutes here is what kept me from finishing the Clear step in time.
- **Repro:** pt start --profile kindling --game-dir <kindling checkout> --seed 202; finish Survey; walk outside; pt key 2; aim at any outside log wall; pt mousehold 3
- **Screenshots:** [now.png](s02/shots/now.png)

### 2. [major] Finished checklist + Ada's brief permanently cover two-thirds of the screen after Done; nothing hides them

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** After the reveal (STEP: Done) the 9-line checklist/'ADA'S BRIEF: ALL MET' list stays drawn from the top down to about y=260 of 360 and overprints the aim label in the middle. F1 only toggles the controls help. During the post-game walk around the cabin, Ada and the mare were mostly hidden behind the text. I had to tilt the view up so models showed in the bottom strip. Expected the list to collapse or fade once Done, or F1 to hide it too.
- **Repro:** pt start --profile kindling --game-dir <kindling checkout> --seed 1881; finish Kindle; pt await REVEAL 30; wait for WORLD; pt key F1; pt shot x
- **Screenshots:** [done_checklist_covers_view.png](s04/shots/done_checklist_covers_view.png)

### 3. [major] Picking up debris needs pixel-exact aim; E does nothing on debris that is nearby but not under the crosshair

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Clear step, first haul: it took ~110 game seconds to pick up the first piece. Fallen shingles are thin, flat slivers; E only works when the crosshair names the piece and it is in reach. Pitch stops at about -75 deg, so a piece right at your feet (0.2 m, where walking up to it leaves you) cannot be aimed at at all; you have to step back. At 6-7 fps the arrow keys overshoot (a 0.2 s tap turned ~48 deg), so lining up a 1-pixel-thick shingle is a chore. After 15 minutes, 8 of 15 debris pieces were still inside. Expected: E picks up the nearest debris in front within reach, or a bigger hit volume for small pieces.
- **Repro:** pt start --profile kindling --game-dir <kindling checkout> --seed 1881; finish Survey; pt walkto debris; pt key e (nothing happens); try to put the crosshair on the shingle at your feet
- **Screenshots:** [debris_aim.png](s01/shots/debris_aim.png)

### 4. [major] Rotten floorboard gap by the window accepts no plank and has no aim name

- **Category:** bug; **reported by** 1 agent(s): s03
- **Detail:** Carrying a New plank from the wagon, standing at the window (x -3.16, z -0.82) and aiming at the dark gap in the floor (pitch about -39 to -44): the centre label is empty (no name, no hint, aim null in state) and holding the left button for 5 s twice did nothing; planksLeft stayed 3. A few degrees either side the aim reads plain 'Floorboards' with no hint. Expected the gap to be named (e.g. 'Rotten floorboard') with a meter, as roof holes are. Repair could not be finished in the time box, so Furnish was never reached.
- **Repro:** pt start --profile kindling --seed 1881 (Repair save); take 'New plank' from the wagon (E); enter cabin; pt walkto window; tilt down to the dark gap in the floor; pt mousehold 5
- **Screenshots:** [gap_noaim.png](s03/shots/gap_noaim.png)

### 5. [major] Snow drift shrinks away from the crosshair; last few percent take over a minute of re-aiming

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Shovelling the single snow drift (3): the drift shrinks toward its centre as it is cleared, so the crosshair keeps falling off it and the aim label silently switches to 'Floorboards'. The meter read 90%, 96%, then 99% while there was still snow left (state snow 0.04, then 0.01). The job started at tick ~3280 and finished at tick 6440; most of that was chasing a tiny blob. Expected: a stable hit area, or the drift to finish at about 95%.
- **Repro:** pt start ... --seed 1881; pt key 3; aim at the snow drift by the hearth; pt mousehold 4; pt mousehold 3 (aim now 'Floorboards' with snow left)
- **Screenshots:** [snow_chase.png](s01/shots/snow_chase.png)

### 6. [major] Welcome fire burns out ~36 s after the reveal ends, leaving Ada in a dark cabin

- **Category:** balance; **reported by** 1 agent(s): s04
- **Detail:** Fire lit at tick 28863 (kindling + one split log on the andirons) went out at tick 36063: 'The fire has burned down to coals. Lay more wood and kindling.' The reveal ended at tick 34987, so the cosy ending, with Ada 'settled by the fire', lasts about 36 game seconds before the room goes dark. The HUD then asks for kindling 'from the chopping block' again. Expected the welcome fire to keep burning through the ending (or Ada to tend it).
- **Repro:** finish Kindle with a single log on the andirons; watch the reveal; pt wait 40; pt state (hearthLit=false); pt events 5
- **Screenshots:** [fire_burns_out.png](s04/shots/fire_burns_out.png)

### 7. [minor] Aim hint overlaps the help text at game start, and 'am' is clipped at the right edge

- **Category:** text; **reported by** 2 agent(s): s01, s02
- **Also reported as:** "At the start, the aim hint draws over the help text, and the clock's 'am' is clipped"
- **Detail:** On the first frame the centre aim text ('Log wall / Grimy. Scrub it (2).') is drawn on top of the controls help block, and both are unreadable where they overlap. The 'am' after the clock is tiny and cut off at the right edge in the first screenshot. A new player's first look at the controls is garbled.
- **Repro:** pt start ... --seed 1881; pt shot start
- **Screenshots:** [overlap.png](s01/shots/overlap.png) [help_overlap.png](s02/shots/help_overlap.png)

### 8. [minor] HUD text overlaps at 640x360: help over the aim label, carry line wraps into toolbar

- **Category:** text, ux; **reported by** 2 agent(s): s03, s04
- **Also reported as:** "HUD text overlaps itself at 640x360: checklist, help, aim label, Ada's dialogue and toasts collide in the centre"
- **Detail:** With help shown, the help lines draw on top of the aim name/hint/meter at screen centre making 'Pry it off...' unreadable; the checklist covers the top half of the view. The 'Carrying Bundle of shingles: hold the left button where it goes' line wraps 'goes' onto a second line that collides with the tool bar. While carrying, the aim still offers 'Braided rug - Take it (E)'.
- **Repro:** pt start (Repair save); pt shot; take shingles at the wagon; pt shot
- **Screenshots:** [overlap_hud.png](s03/shots/overlap_hud.png) [welcome_hud_overlap.png](s04/shots/welcome_hud_overlap.png)

### 9. [minor] 'Shovel out the snow' shows no percentage, and the last small drift hides behind the checklist

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** Sweep and Scrub show (NN%) on the checklist, but the snow line doesn't. With 8% of the snow left, the remaining drift was small and sat behind the top-left checklist text from most angles, and nothing on screen said there was still snow left or where it was. The meter under the crosshair turned out to be overall snow progress (74%, then 92%), not progress on that one drift, and nothing tells you that.
- **Repro:** shovel most of the snow near the hearth with pt key 3 + pt mousehold; at snow ~0.08 look around from the hearth
- **Screenshots:** [s12.png](s02/shots/s12.png)

### 10. [minor] Checklist overlay covers the upper-left quarter of the view and hides debris and floor targets

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** The checklist is drawn opaque-ish over a large part of the screen. When looking down at the floor, the pieces of debris that are in reach (1.2-1.5 m) and the rotten patch often sit right under the checklist text. Several of my failed pickups were on pieces I could not see. Expected: a smaller or more transparent checklist, or one that can be collapsed separately from F1 help.
- **Repro:** pt start ... --seed 1881; walk inside; tilt view down toward debris to the left
- **Screenshots:** [shingle_try.png](s01/shots/shingle_try.png)

### 11. [minor] E on an out-of-reach item fails silently; reach only shown by label colour

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Aiming at a Fallen shingle about 1.6 m away shows its name in a slightly dimmer grey. Pressing E does nothing, with no message. Expected: a 'Too far' note, or a clearer in-reach cue.
- **Repro:** pt start ... --seed 1881; stand ~1.6 m from a shingle, aim at it, pt key e
- **Screenshots:** [out_of_reach.png](s01/shots/out_of_reach.png)

### 12. [minor] Fallen shingles lying on the snow drift take the crosshair while you shovel

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** While I held the shovel on the drift, the aim kept switching to 'Fallen shingle' (a debris piece on top of the drift), so shoveling stopped partway through without any message. It works once you haul the shingles out first, but the order isn't explained.
- **Repro:** after Survey, walk to the drift by the hearth; pt key 3; look down; pt mousehold 3
- **Screenshots:** [snow4.png](s02/shots/snow4.png)

### 13. [minor] Frame rate 6-7 fps for most of the session makes fine aiming hard

- **Category:** perf; **reported by** 1 agent(s): s01
- **Detail:** pt state showed 6-7 fps from the start and 18 fps at the end, with tps steady around 29-30. At 6-7 fps, holding a turn key overshoots badly. Mouse look via relative motion sometimes had no effect (pt look -150 -60 at tick ~2180 left yaw unchanged). The machine was shared with other sessions, so this is partly the environment.
- **Repro:** pt start ... --seed 1881; pt status; pt hold Left 0.2 (turned ~48 deg)
- **Screenshots:** [rot5.png](s01/shots/rot5.png)

### 14. [minor] Held plank covers the crosshair and the floor gap you are trying to aim at

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** While carrying a New plank, the plank model is drawn right across the centre of the screen, hiding the crosshair and the target gap; the player cannot see where the plank will go.
- **Repro:** Take 'New plank' at the wagon; walk into the cabin; tilt down toward the floor near the window
- **Screenshots:** [plank_occludes.png](s03/shots/plank_occludes.png)

### 15. [minor] Kindling at the chopping block is hidden behind the block and only visible in lantern light

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** The hearth hint says to bring kindling 'from the chopping block', but the splinter pile sits on the far side of the block (about x=-2.3, z=6.5) and is hidden from where you split. At night it is only visible in lantern light. Split logs on the ground are also a thin pick target: they register only in a 2-3 degree band of pitch.
- **Repro:** split a round; pt walkto chopping_block; look for 'Kindling'
- **Screenshots:** [kindling_hidden.png](s04/shots/kindling_hidden.png)

### 16. [minor] Night falls within the first 13 minutes and the cabin goes pitch black, with no lantern hint

- **Category:** balance; **reported by** 1 agent(s): s01
- **Detail:** The clock runs from 9:04 am to 9:37 pm in about 12.5 game minutes. By then the cabin is nearly black and the debris can't be seen (screenshot). Nothing on screen suggests the lantern (L). It is only listed in the help, which I had hidden with F1 at the start, as the help suggests. The lantern works well once it is on. Expected: a 'It's getting dark - L for your lantern' status line, or a longer first day.
- **Repro:** pt start ... --seed 1881; play inside the cabin until about tick 22000
- **Screenshots:** [dark_night.png](s01/shots/dark_night.png)

### 17. [minor] Roof holes are invisible at night and their hint ignores what you carry

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** After dark (the clock runs about one game hour per real minute, so night falls within ~8 minutes of play), the roof holes are black on a dark ceiling even with the lantern lit; only the centre label reveals them, so finding them means sweeping the view blindly. While carrying a bundle, the hint still says 'Bring shingles from the wagon and work them in.' instead of telling you to hold the button.
- **Repro:** Play past ~9 pm game time; take Bundle of shingles; go to hearth; look up at the ceiling
- **Screenshots:** [hole_night.png](s03/shots/hole_night.png)

### 18. [minor] Sawing the rafter raises the debris count (5 left -> 7 left) with no explanation

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** E on the Fallen rafter gives a good message ('Too long to carry. Saw it into pieces (4)'). After sawing, though, the checklist jumps from 5 left to 7 left without saying why, and the pieces have two names ('Length of rafter' at the crosshair, 'Piece of rafter' elsewhere).
- **Repro:** pt walkto debris until 'Fallen rafter'; pt key 4; pt mousehold 4
- **Screenshots:** [saw3.png](s02/shots/saw3.png)

### 19. [minor] Short key taps (E, tool keys) are dropped at low frame rates

- **Category:** bug; **reported by** 1 agent(s): s02
- **Detail:** At about 7 fps, 'pt key e' on an in-reach Broken board did nothing, three times, with no message. Holding E for 0.4 s picked it up. The same thing happened with the tool keys: pressing 4 and then holding the mouse sometimes left the saw meter at 0%. Input looks like it is sampled once per frame, so a press shorter than a frame is lost. Expected: key-down events are queued so that every tap counts.
- **Repro:** pt start ... --seed 202 (with other sessions loading the machine); pt walkto debris; aim at the board; pt key e; pt state shows carrying: nothing
- **Screenshots:** [e1.png](s02/shots/e1.png)

### 20. [minor] Survey 'Hole in the roof' only registers when the crosshair is exactly on the small hole

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** The hole was plainly visible on screen just below the crosshair, but the aim still read 'Ceiling boards'. Finding the angle that ticked the line took several minutes of nudging. A bigger hit area, or a hint on the line, would help.
- **Repro:** pt walkto hearth; look up toward the blue sky patch; nudge until the aim reads 'Hole in the roof'
- **Screenshots:** [h4.png](s02/shots/h4.png)

### 21. [minor] Survey step never says that it is done by looking at each problem

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** The game opens on 'STEP: Survey' with 'Grime' already ticked and four unticked items. Nothing explains that you tick them by pointing the crosshair at each one. I worked it out by trial. The window wouldn't tick because a fallen rafter was in the way, and the rotten floorboards were hidden behind the checklist overlay. Survey finished at tick 2541 (85 s), which is fine once you understand it. Expected: one line such as 'Look over the cabin: aim at each problem'.
- **Repro:** pt start ... --seed 1881; read the checklist; walk in through the door
- **Screenshots:** [survey_unclear.png](s01/shots/survey_unclear.png)

### 22. [minor] Sweep completes at 85% without ever saying 85% is the target

- **Category:** ux; **reported by** 1 agent(s): s02
- **Detail:** 'Sweep the floor' ticked at 85% ('The floor is swept.'). The checklist shows a percentage that looks like it should reach 100, so a player can't tell how much sweeping is left.
- **Repro:** pt key 1; sweep the interior floor while holding the mouse
- **Screenshots:** [sweepdone.png](s02/shots/sweepdone.png)

### 23. [minor] Taking a log from the woodpile for the andirons un-ticks 'stack it by the hearth (4 of 4)' and reopens the step

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** The Kindle checklist asks for 4 logs stacked, then 'Lay a split log on the andirons'. The obvious source is the woodpile. Taking one drops the line back to '[ ] (3 of 4)', so Kindle cannot complete until you go back out, split another round and stack it. The real requirement is 5 logs, but the list says 4. Either count the andiron log or say 'stack 4, and one more for the fire'.
- **Repro:** stack 4 split logs on the woodpile (line ticks); E on woodpile with empty hands; pt state --full -> checklist[1] done=false '(3 of 4)'
- **Screenshots:** [checklist_unticks.png](s04/shots/checklist_unticks.png)

### 24. [minor] Wagon items overlap: hard to pick the bundle/plank, easy to grab the rug by mistake

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** The shingles, plank, rug, quilt and curtains sit in one pile; each item is only targetable in a 3-4 degree band, and the aim label can lag a frame at low fps, so I picked up the Braided rug instead of the shingles once. Each roof section needs its own bundle, so this is repeated 4x for the roof and 3x for planks.
- **Repro:** pt walkto wagon; tilt slowly from -10 to -30 pitch and watch the label switch between Your wagon / Bundle of shingles / Braided rug
- **Screenshots:** [wagon5.png](s03/shots/wagon5.png)

### 25. [minor] Woodpile is hard to find: dark corner left of the hearth, out of view when facing the hearth

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** The carry hint says 'Stack it on the woodpile by the hearth'. The woodpile is two low rails in the dark back-left corner, out of view when you face the hearth, and the bottom carry message covers it when you look down. It took several turns to find. The thin logs also let the crosshair hit 'Floorboards' through the gaps. With empty hands, the full pile shows no hint that E takes a log.
- **Repro:** carry a split log in; pt walkto hearth; look around for the woodpile
- **Screenshots:** [woodpile_hard_to_find.png](s04/shots/woodpile_hard_to_find.png)

### 26. [polish] 'Strike a match' line ticks ~35 s after 'It catches.' with no progress feedback

- **Category:** ux; **reported by** 1 agent(s): s04
- **Detail:** After 'It catches.' the flames show but the checklist line stays unticked until the fire grows (~tick 29900, ~35 game s later). No 'taking hold' meter or text says why, so it looks like the fire did not count.
- **Repro:** lay kindling + log; pt key 8; pt mousehold 5; pt state --full every few seconds
- **Screenshots:** [fire_lit_unticked.png](s04/shots/fire_lit_unticked.png)

### 27. [polish] Day clock runs ~1 game minute per second: evening passes during the Kindle chores, so Ada arrives at 4 am

- **Category:** balance; **reported by** 1 agent(s): s04
- **Detail:** Load was at 2:19 pm. Evening had passed by the time the fire was laid, and 'Ada's wagon is coming up the trail' at 4:05 am in full dark. The post-game walk then happens at dawn, not 'as evening falls'. An arrival in the small hours reads oddly for the cosy beat.
- **Repro:** pt start ...; play Kindle normally; pt events (tick 34183 clock 4:05 am)
- **Screenshots:** [welcome_hud_overlap.png](s04/shots/welcome_hud_overlap.png)

### 28. [polish] Furnishings at the wagon can be taken during the Clear step without guidance

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** Walking past the wagon during Clear, the crosshair offers 'Braided rug - Take it (E)'. A new player can't tell whether to bring furniture in before the floor is swept.
- **Repro:** during Clear, pt walkto wagon; look at the rug
- **Screenshots:** [sweep.png](s01/shots/sweep.png)

### 29. [polish] Nothing is shown in hand while carrying debris

- **Category:** ux; **reported by** 1 agent(s): s01
- **Detail:** After picking up a broken board, the only sign is the orange 'Carrying: E to put down (outside to haul out)' line. The board doesn't appear in view. Tools like the broom and shovel do show in hand.
- **Repro:** pick up any debris with E; pt shot
- **Screenshots:** [carry2.png](s01/shots/carry2.png)

### 30. [polish] Prying window boards silently re-opens the completed 'Haul out the old wood' line

- **Category:** ux; **reported by** 1 agent(s): s03
- **Detail:** Each pried board drops as debris and the haul line grew from 1 to 4 pieces; reasonable, but no message says the boards became debris to haul.
- **Repro:** Pry the three window boards with the pry bar (5); watch the haul line count
- **Screenshots:** [pry1.png](s03/shots/pry1.png)

### 31. [polish] Reveal title 'Ada's cabin, from ruin to ho...' cut off at the right edge

- **Category:** text; **reported by** 1 agent(s): s04
- **Detail:** The timelapse title runs off the window at 640x360. Its 5:20 pm frames are also nearly black apart from the window.
- **Repro:** finish Kindle; pt await REVEAL 30; pt shot r
- **Screenshots:** [reveal_title_cut.png](s04/shots/reveal_title_cut.png)

## Objective timings

Game seconds from the start of the world to each objective (event tick / the game's ticks per second, from events.jsonl). Game time, not wall time: with many sessions on one machine a world can run slower than real time, and wall-clock timings would mostly measure the machine.

| # | Objective | s01 | s02 | s03 | s04 | min | median | max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Survey | 84.7 | 127.5 | - | - | 84.7 | 106.1 | 127.5 |
| 5 | Kindle | - | - | - | 1139.4 | 1139.4 | 1139.4 | 1139.4 |
| 6 | Welcome | - | - | - | 1164.7 | 1164.7 | 1164.7 | 1164.7 |

