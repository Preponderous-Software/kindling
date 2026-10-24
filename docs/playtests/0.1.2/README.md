# Playtest: Kindling 0.1.2 candidate

Five AI agents played the desktop build headless with the
[playtester](https://github.com/Preponderous-Software/playtester) kit (Kindling profile, playtester
`main` at b69e15e), each toward its own goal. [summary.md](summary.md) is the merged summary; each
session's folder has its `report.json`, `session.json`, every harness command (`actions.jsonl`), its
raw notes (`findings.jsonl`), the game's probe events (`events.jsonl`) and the screenshots its
findings cite.

| | |
|---|---|
| Build | s01 to s04: `main` at `08933b7` (0.1.1 plus a test-only change), Release. s05: `da3215f`, an early commit of the pull request that added this folder, run on its own after the fixes for s01 to s04. The probe reports version 0.1.1; this is filed under the next expected version |
| Date | 2026-10-23 |
| Machine | 20 cores, WSL2, Mesa llvmpipe; 640×360 windows; s01 to s04 at once (2 to 5 fps), s05 alone but beside other work on the machine (4 to 10 fps) |

| Session | Goal | Seed | Budget | Start |
|---|---|---|---|---|
| s01 | The first 15 minutes as a brand-new player; note every moment of confusion | 2718 | 30 min | new game |
| s02 | Finish the Survey and the Clear step from what the screen says; note fiddly aim and whether clean and dirty surfaces can be told apart | 77 | 35 min | new game |
| s03 | Finish Repair and Furnish; above all, say whether every floor gap can be aimed at and filled | 1881 | 35 min | a save at the start of Repair |
| s04 | Split, stack, lay and light the fire, welcome Ada, watch the reveal, then judge how it looks and reads | 1881 | 30 min | a save at the start of Kindle |
| s05 | Replace the three rotten floorboards first, then the rest of Repair and as much of Furnish as possible | 1881 | 35 min | a save at the start of Repair |

The saves for s03, s04 and s05 were made from `08933b7` by running `--autopilot --speed 6` on seed
1881 and pressing F5 as each step began.

## What went wrong in the run

- No crashes and no invalid reports. The game ran slowly throughout (see above), so the agents'
  timings are game time, and several findings about lost or lagging input come from the machine,
  not the game.
- s03 ran out of time before the floor gaps. s05 was run to answer that question: every gap could
  be aimed at and filled (3 of 3), the bug the 0.1.1 playtest found.
- s05 could not find the last two open roof sections, so Repair did not finish and Furnish was not
  reached. Nobody reached Furnish in this run.

## What was done about it

The findings were fixed in the pull request that added this folder (the one major and most minors).
See its description for the list, and for what was left.
