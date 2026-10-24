# Frozen save fixtures

Saves written by a released build of Kindling, kept so that every later build is tested against
saves players already have. `tests/test_saves.cpp` loads each one by name and plays on from it.
**Never edit or regenerate a fixture once it is committed.** A new save format (`kSaveVersion`
in `src/save/save.h`) gets its own `v<N>-*.save` fixtures, written by the release that introduces
it, and a migration on load for the older ones.

| File | Format | Written by | State |
|---|---|---|---|
| `v1-repair.save` | 1 | 0.1.2 (`90102aa699c4a25ffeedad1d769de94a0f5c901b`) | autopilot, seed 1, tick 4649: the first tick of the Repair step (surveyed and cleared, nothing mended); fingerprint `2aa5346ae9869620` |
| `v1-done.save` | 1 | 0.1.2 (`90102aa699c4a25ffeedad1d769de94a0f5c901b`) | autopilot, seed 1, tick 11495: the restoration finished (Done); fingerprint `eb2bd619a5db582a` |

SHA-256:

```
7a0f8ea2ac9fc92a3e497f5f6e9f9e9c0db3f1e1167c70c0e8a98d8f3cce3dbf  v1-repair.save
a8967348f58041cc83c8d61b5af8a3e1d5cbafb523a82c4b54765e9a6e316f01  v1-done.save
```

## How they were made (2026-10-24)

[`tools/save-fixtures/write_fixtures.cpp`](../../tools/save-fixtures/write_fixtures.cpp) plays a new
game with the autopilot and writes each state with the game's own `writeSave`. It was compiled
against a checkout of the `0.1.2` tag (GCC 9.4 on Ubuntu 20.04, the same `-ffp-contract=off` as
`kindling_sim`) and run once:

```bash
git worktree add --detach ../kindling-0.1.2 0.1.2
cd ../kindling-0.1.2
g++ -std=c++17 -O2 -ffp-contract=off -Isrc /path/to/tools/save-fixtures/write_fixtures.cpp \
    src/sim/*.cpp src/save/*.cpp -o write_fixtures
./write_fixtures out/
```

The same program built from `main` at `4e30aec` wrote byte-identical files: neither `src/sim/`
nor `src/save/` changed between 0.1.2 and then.
