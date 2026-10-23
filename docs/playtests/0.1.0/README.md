# Playtest: Kindling 0.1.0 candidate

Four AI agents played the desktop build headless with the
[playtester](https://github.com/dmccoystephenson/playtester) kit (Kindling profile, playtester 0.2.0),
each toward its own goal. [summary.md](summary.md) is the merged summary; each session's folder has
its `report.json`, `session.json`, every harness command (`actions.jsonl`), its raw notes
(`findings.jsonl`), the game's probe events (`events.jsonl`) and the screenshots its findings cite.

| | |
|---|---|
| Build | `main` at `cfae820` (after the polish pass, #21–#24), Release, version 0.1.0; no release existed yet, so this is filed under the next expected version |
| Date | 2026-10-22 |
| Machine | 20 cores, WSL2, Mesa llvmpipe; 640×360 windows; four sessions at once (6–9 fps) |
| Harness smoke | `pt smoke --profile kindling` PASS (Survey completed after 15 attempts) |

| Session | Goal | Seed | Budget | Start |
|---|---|---|---|---|
| s01 | The first 15 minutes as a brand-new player; note every moment of confusion | 1881 | 30 min | new game |
| s02 | Finish the whole Clear step from what the screen says; time each checklist line and judge the feedback | 202 | 35 min | new game |
| s03 | Finish Repair and Furnish; note anything fiddly about carrying, placing and turning things | 1881 | 35 min | a save at the start of Repair |
| s04 | Lay and light the fire, welcome Ada, watch the reveal, then judge how everything looks | 1881 | 30 min | a save at the start of Kindle |

The saves for s03 and s04 were made by running `--autopilot --speed 6` on seed 1881 and pressing F5
as each step began.

## What went wrong in the run

- No crashes and no invalid reports. Every session's world ran a little below full speed with four
  sessions sharing the machine, so the agents' timings are game time.
- s02 stopped a runaway helper script of its own with `pkill`, which the prompt forbids; it matched
  only that script, and the game kept running.
- s03 never reached Furnish: a plank in hand blocked the aim at the floor gap (finding 4), so R
  (turning a carried furnishing) went untested.

## What was done about it

The findings were fixed in the pull request that added this folder (blockers: none; all six majors
and most minors). See its description for the list.
