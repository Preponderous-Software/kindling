# Playtest: Kindling 0.1.1 candidate

Four AI agents played the desktop build headless with the
[playtester](https://github.com/dmccoystephenson/playtester) kit (Kindling profile, playtester `main` at
b25ee56), each toward its own goal. [summary.md](summary.md) is the merged summary; each session's
folder has its `report.json`, `session.json`, every harness command (`actions.jsonl`), its raw notes
(`findings.jsonl`), the game's probe events (`events.jsonl`) and the screenshots its findings cite.

| | |
|---|---|
| Build | `main` at `17ed1e8` (after #28: stacks, the wagon layout, carried things in view), Release. The probe still reports version 0.1.0; this is filed under the next expected version |
| Date | 2026-10-23 |
| Machine | 20 cores, WSL2, Mesa llvmpipe; 640×360 windows; four sessions at once (about 6 fps) |

| Session | Goal | Seed | Budget | Start |
|---|---|---|---|---|
| s01 | The first 15 minutes as a brand-new player; note every moment of confusion | 3141 | 30 min | new game |
| s02 | Finish the Survey and the Clear step from what the screen says; judge how forgiving the aim is | 202 | 35 min | new game |
| s03 | Finish Repair and Furnish; note anything about the wagon, carrying stacks, placing or turning things | 1881 | 35 min | a save at the start of Repair |
| s04 | Split, stack, lay and light the fire, welcome Ada, watch the reveal, then judge how it looks and reads | 1881 | 30 min | a save at the start of Kindle |

The saves for s03 and s04 were made from the same build by running `--autopilot --speed 6` on seed
1881 and pressing F5 as each step began.

## What went wrong in the run

- No crashes and no invalid reports. With four sessions sharing the machine the game ran at about 6
  frames a second, so the agents' timings are game time.
- s03 was blocked: an open gap in the floor could not be aimed at (finding 1), so no plank went in,
  Repair could not finish and Furnish was never reached. The same bug was behind the 0.1.0
  playtest's "gap accepts no plank"; the fix then (looking past the plank in hand) was not the cause.
- s02 ran out of time on the walls (22% at the end; finding 15).

## What was done about it

The findings were fixed in the pull request that added this folder (all three majors and most
minors). See its description for the list.
