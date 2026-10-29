# Playtest: Kindling 0.2.0 candidate

Four AI agents played the desktop build headless with the
[playtester](https://github.com/Preponderous-Software/playtester) kit (Kindling profile, playtester
`main` at 0561475), each toward its own goal. [summary.md](summary.md) is the merged summary; each
session's folder has its `report.json`, `session.json`, every harness command (`actions.jsonl`), its
raw notes (`findings.jsonl`), the game's probe events (`events.jsonl`), where the player went
(`track.jsonl`) and the screenshots its findings cite.

| | |
|---|---|
| Build | `main` at `7ecf1fe` (0.1.2 plus the engine frame clock, the frozen save fixtures, the Strata v0.6.0 pin and the browser loading shell), Release. The probe reports version 0.1.2 because `CMakeLists.txt` has not been bumped yet; this is filed under the next expected version, 0.2.0 |
| Date | 2026-10-29 |
| Machine | 20 cores, WSL2, Mesa llvmpipe; 640×360 windows; all four at once, beside playtests of other games (load average about 55 to 60, 2 to 5 fps, 25 to 30 ticks a second) |

| Session | Goal | Seed | Budget | Start |
|---|---|---|---|---|
| s01 | The first 15 minutes as a brand-new player; note every moment of confusion | 2718 | 30 min | new game |
| s02 | Finish the Survey and the Clear step from what the screen says; check that a saw cut is kept on release, whether the debris counter goes up, and whether dirty and clean surfaces can be told apart | 77 | 35 min | new game |
| s03 | Load a save written by 0.1.2 at the start of Repair, finish Repair and as much of Furnish as possible; say whether every floor gap and roof hole could be found and fixed | 1 | 35 min | `tests/fixtures/v1-repair.save` (0.1.2) |
| s04 | Split, stack, lay and light the fire, welcome Ada, watch the reveal, then judge how it looks and reads | 1881 | 30 min | a save early in Kindle |

s03 started from the frozen 0.1.2 fixture `tests/fixtures/v1-repair.save` (SHA-256 `7a0f8ea2...`),
copied in as the session's save, so the session also checks that a save players already have loads
in this build. s04's save was made from `7ecf1fe` by running `--autopilot --speed 6` on seed 1881
and pressing F5 just after the Kindle step began (tick 8244).

## What went wrong in the run

- No crashes and no invalid reports. The machine was shared with other playtests and ran the game
  at 2 to 5 fps, so timings are game time, and some lost key presses come from the machine.
- The first harness smoke run failed: the game reached the world but its probe stayed at tick 0
  and the window stayed black for 90 seconds under the full load. The smoke run straight after
  passed, as did every agent's `pt start`. It was not seen again and is not filed.
- Nobody finished the Clear step: s01 and s02 both ran out of time on the walls (13% and 24%).
  s03 finished Repair and got Furnish to 2 of 5; s04 reached the end.

## Results

- **The 0.1.2 save loads:** s03's game said "Loaded your save." at the Repair step and played on
  through Repair.
- **Fixed since 0.1.2, confirmed in play:** a saw cut keeps its progress when the button is let go
  (s02: 6% to 12% across a release); every floor gap (3 of 3) and every roof section (4 of 4) could
  be found and fixed (s03); the dusty floor and the scrubbed chimney show their progress (s02).
- **0 blocker, 2 major, 18 minor, 12 polish** in 30 groups. The two majors were filed as issues:
  the framed sampler hung on a side wall shows only as a thin strip (s03), and the log walls scrub
  so slowly, and look so alike clean or grimy, that the Clear step cannot reasonably be finished
  (s02, with s01 stopping at 13% too). The minors and polish are in [summary.md](summary.md).
- "Build reports version 0.1.2" (s01, s03, s04) is the expected state of an unreleased build, not
  a bug: the version is bumped when the release is cut (docs/RELEASING.md).
