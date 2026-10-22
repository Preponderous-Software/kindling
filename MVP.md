# MVP

The MVP is the smallest slice that feels like Kindling: **one cabin restored from ruin to home,
ending with the first fire and a settler moving in.** If that one room feels good, the rest of the
game is more of it.

## In scope

| Area | The MVP has |
|---|---|
| Place | One single-room cabin (the hearth room in [`prototypes/`](prototypes/README.md)), its yard, and a short path to a woodpile and a stall |
| Time | One season: late autumn into first frost, with a day/night cycle |
| Player | First-person movement and interaction (look, walk, pick up, use a tool) |
| Restoration | All six steps of the core loop: survey, clear, repair, furnish, kindle, welcome |
| Settler | One settler (Ada Lark) with one brief, one mood readout, and a few lines at arrival |
| Horse | One horse in a stall to brush and feed (no riding) |
| Firewood | Split logs and stack them; carry kindling to the hearth |
| Reveal | The before/after timelapse of the player's own restoration |
| Saves | Save and continue at any point; a damaged save is refused and the previous one is kept |
| Platform | Windows desktop (primary) and Linux desktop |

## Out of scope

Each of these is deferred, not rejected:

- More cabins, the valley map, other settlers and friendship levels.
- Riding, foraging, fishing, farming and the other seasons.
- Tailoring and outfits.
- Proving up (the five-winter arc) and the territorial map.
- A browser build and cloud saves.
- Audio beyond a few placeholder sounds.
- Personal content packs.

## What "done" means for each step

Each step has a check a playtester can do in the build, without reading code.

| Step | Done when |
|---|---|
| Survey | Walking into the cabin highlights each kind of damage (grime, debris, rotten plank, roof hole, boarded window) at least once, and a short checklist appears. |
| Clear | Debris can be picked up and carried out of the door. Sweeping and scrubbing visibly clean the floor and walls, a patch at a time. The snow under the roof hole can be shovelled out. |
| Repair | Rotten planks can be pried up and replaced; new planks look pale and darken over the next day. The roof hole can be patched from inside. Window boards can be pried off. The chair can be mended. |
| Furnish | At least eight items (rug, quilt, curtains, sampler, mantel things, table, basket, woodpile) can be placed, moved and removed. The settler's brief is shown, and the room shows when the brief is met. |
| Kindle | Laying kindling, striking a match and shielding the flame is an interactive sequence. The room's light changes from moonlight to firelight as the fire catches. The fire flickers. |
| Welcome | The settler walks in, reacts to the room (different lines for brief met and not met), and sits by the fire. The timelapse then plays. |

## Milestones

Each milestone ends in something that can be run or looked at.

| Milestone | Result | Checked by |
|---|---|---|
| **M0 Prototypes** (done) | The hearth room rendered in Strata before and after; a restoration recorded as a `usd-replay/1` replay and played in usd-web | The images in the README; `usd-recorder replay validate` passes |
| **M1 Walk the room** (done) | First-person camera in the ruined cabin on Strata, with collision and object picking | A build where you can walk the room and the object under the crosshair is named |
| **M2 Clean** (done) | Surfaces that hold grime and change as they are cleaned (needs a Strata addition, see [ARCHITECTURE.md](ARCHITECTURE.md#engine-additions)) | Sweeping a patch of floor visibly cleans exactly that patch, and it stays clean after save and load |
| **M3 Repair and furnish** (done) | Plank, roof, window and chair repairs; placing and moving furniture | The repair and furnish checks in the table above |
| **M4 Kindle** (done) | The fire ritual, firelight, a flickering flame and the candle | The kindle check above; a headless golden image of the lit room |
| **M5 Welcome and reveal** (done) | The settler's arrival and the timelapse of the player's own restoration | The welcome check above; the recorded replay validates |
| **M6 Saves** (done) | Save and continue at any step, through Strata's save core | Save and quit at each step, reload, and the room is identical (same state fingerprint) |
| **M7 Horse** (done) | The stall, brushing and feeding | The horse reacts to brushing and to being fed |

M2 is the riskiest: it is the one that needs new engine work, and the whole restoration feeling
depends on it.

## How the MVP is tested

- **Determinism.** The game state has a fingerprint (FNV-1a over every saved field, like
  steading's). The same seed and the same inputs always give the same fingerprint, on every
  compiler. A test pins the fingerprint after a scripted restoration.
- **Golden images.** Key moments (ruin, cleared, furnished, lit) render headless and are compared
  to committed images, the way Strata checks its own renderer.
- **Replays.** Every scripted test restoration writes a replay, and CI runs
  `usd-recorder replay validate` on it.
- **Saves.** Save, corrupt a byte, reload: the damaged save must be refused and the previous one
  loaded.
- **Playtests.** Each milestone gets a short playtest against the "done" table above, with notes
  committed under `docs/playtests/`.

## Success

The MVP succeeds if a first-time player, given no instructions beyond the survey checklist, takes
the room from ruin to a lit fire and a settled settler in under an hour, and wants to do the next
cabin.
