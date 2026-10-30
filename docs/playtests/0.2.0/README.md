# Playtest: Kindling 0.2.0 candidate

Four AI agents played the desktop build headless with the
[playtester](https://github.com/Preponderous-Software/playtester) kit (Kindling profile, playtester
`main` at 0561475), each toward its own goal. [summary.md](summary.md) is the merged summary; each
session's folder has its `report.json`, `session.json`, every harness command (`actions.jsonl`), its
raw notes (`findings.jsonl`), the game's probe events (`events.jsonl`), where the player went
(`track.jsonl`) and the screenshots its findings cite.

This run replaces the first 0.2.0 run (on `7ecf1fe`). It re-tests the build after the fixes for the
two majors that run found: #40 (the framed sampler hung on a side wall showed only as a thin strip,
fixed by #43) and #41 (grimy and scrubbed log walls looked alike, fixed by #44). The goals, seeds and
budgets are the first run's, with the s02, s03 and s04 goals extended to look at the walls and the
sampler.

| | |
|---|---|
| Build | `main` at `2af0a33` (0.1.2 plus the engine frame clock, the frozen save fixtures, the Strata v0.6.0 pin, the browser loading shell, the tests for Ada's arrival, #43 and #44), Release. The probe reports version 0.1.2 because `CMakeLists.txt` has not been bumped yet; this is filed under the next expected version, 0.2.0 |
| Date | 2026-10-29 |
| Machine | 20 cores, WSL2, Mesa llvmpipe; 640×360 windows; beside builds and playtests of other games (load average about 90 to 115, 1 to 3 fps, 2 to 20 ticks a second) |
| Tests | all 13 ctest suites passed on the build |

| Session | Goal | Seed | Budget | Start |
|---|---|---|---|---|
| s01 | The first 15 minutes as a brand-new player; note every moment of confusion | 2718 | 30 min | new game |
| s02 | Finish the Survey and the Clear step from what the screen says; check that a saw cut is kept on release, whether the debris counter goes up, whether dirty and clean surfaces can be told apart, and in particular whether grimy and scrubbed log walls can be told apart by eye and how long the walls take | 77 | 35 min | new game |
| s03 | Load a save written by 0.1.2 at the start of Repair, finish Repair and as much of Furnish as possible; say whether every floor gap and roof hole could be found and fixed; hang the framed sampler on a side log wall and on the chimney breast | 1 | 35 min | `tests/fixtures/v1-repair.save` (0.1.2) |
| s04 | Split, stack, lay and light the fire, welcome Ada, watch the reveal, then judge how it looks and reads, the log walls inside and out included | 1881 | 30 min | a save early in Kindle |

s03 started from the frozen 0.1.2 fixture `tests/fixtures/v1-repair.save` (SHA-256 `7a0f8ea2...`),
copied in as the session's save. s04's save was made from `2af0a33` by running `--autopilot --speed 6`
on seed 1881 and pressing F5 just after the Kindle step began (tick 7974); by then the autopilot had
already split and stacked the firewood and laid the log, so s04 laid the kindling and lit the fire.

## What went wrong in the run

- No crashes and no invalid reports; `pt aggregate` reported 0 problems.
- The machine was far busier than in the first run, so the game ran at 1 to 3 fps. Timings are game
  time, short key taps were often lost, and every agent logged the low frame rate once (finding 3
  in the summary; it is the machine, not the game). Each session got through less than in the
  first run: s01 and s02 stayed in Clear, and s03 did not finish Repair.
- The harness smoke run (`pt smoke --profile kindling`) was tried four times and did not pass under
  this load. The game started, the probe ticked, and `walkto door` and `walkto hearth` worked
  (in two of the four), but the scripted look-around did not tick all five Survey lines within its
  24 attempts at 1 to 2 fps (three of five were ticked), and twice `walkto door` stopped 1.64 m
  from the door. All four agents then started and played their sessions normally.
- s03 did not reach Furnish, so the sampler was checked by the coordinator instead (below).

## Results

- **#40 is fixed.** s03 did not get to the sampler, so the coordinator loaded the Furnish save used
  to check #43, carried the framed sampler to the right-hand log wall, pressed R once and hung it
  with E. It hung flat against the logs, a whole picture from in front and from the side, and
  "Something of home on the walls" ticked. These screenshots were not kept in this folder.
- **#41's look is fixed; its pace is not clearly better.** s02 could tell grimy from scrubbed logs
  by eye up close, from a step back and by lantern ([wall_back.png](s02/shots/wall_back.png),
  [wall_lantern.png](s02/shots/wall_lantern.png)): scrubbed logs are bare brown, grimy logs carry
  a pale grey film. s02 still rated "Scrub the walls" a major balance finding: 13% to 26% took about
  146 game seconds of scrubbing and stepping along the wall, close to the first run's rate (13% to
  24% in about 117 s). Much of that time went on moving and re-aiming at 1 to 2 fps, so the pace is
  not judged here; it is left to the owner whether it needs more than #44.
- **s03's major was not reproduced.** s03 reported that none of the four open roof sections could be
  seen from inside (finding 2). The coordinator loaded the same 0.1.2 fixture, walked to about
  (1.3, -1.6) and looked up: the hole showed as open sky, named "Hole in the roof" with "Bring
  shingles from the wagon and work them in". s03 had looked from (2.0, 2.1) and (2.5, -1.6), away
  from the hole (x -0.6 to 0.8, z -1.5 to -0.3), with turns lost to the frame rate. It is treated
  as a minor (the hole is hard to find unless you stand near it, as in the first run) and not filed.
- **The 0.1.2 save still loads:** s03's game loaded the fixture without complaint, and s03 fixed all 3
  rotten floorboards, took down the window boards and mended the chair.
- **Confirmed again:** a saw cut keeps its progress on release (s02); the fire catches and takes hold
  in about 15 game seconds, then Welcome and the reveal play (s04).
- **0 blocker, 2 major (neither filed, see above), 19 minor, 10 polish** in 28 groups. No new blocker
  or major was found, and nothing looked broken by #43 or #44. The minors and polish are in
  [summary.md](summary.md). Several repeat the first run (the debris counter going up after sawing,
  no chimney smoke, the replay's last frame, checklist font sizes, aiming at small targets). New ones
  include the broom doing nothing while the debris aim assist holds the target (s01), a "Pry it up"
  hint during Clear (s01) and a kindling toast that asks for a log already laid (s04).
- "Build reports version 0.1.2" is the expected state of an unreleased build, not a bug: the version
  is bumped when the release is cut (docs/RELEASING.md).
