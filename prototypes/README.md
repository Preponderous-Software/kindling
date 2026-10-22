# Prototypes

Two small prototypes (MVP milestone M0) that check Kindling's core ideas against the real engine
and file formats. They are throwaway code: the game will not be built on top of them.

## hearth-scene: the room in Strata

[`hearth-scene/gen_hearth.py`](hearth-scene/gen_hearth.py) writes a scene file for Strata's
headless renderer, `strata-golden`. The room is built from Strata primitives and meshes generated
by the script (logs, stones, flames, the chair, the pines). Nothing is imported.

```sh
python3 gen_hearth.py > hearth.scene                  # restored, the fire lit
python3 gen_hearth.py before > hearth_before.scene    # the ruin
strata-golden render hearth.scene hearth.png
strata-golden render hearth_before.scene hearth_before.png
```

`strata-golden` is built with Strata (tested with v0.3.0, commit `38517b6`). Without a GPU, set
`LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe` to render with Mesa's software rasteriser.
Output: [`strata-hearth-before.jpg`](../docs/images/strata-hearth-before.jpg) and
[`strata-hearth-after.jpg`](../docs/images/strata-hearth-after.jpg).

## restore-replay: the restoration as a replay

[`restore-replay/restore.cpp`](restore-replay/restore.cpp) simulates one person restoring the
hearth room (survey, clear, repair, furnish, kindle, welcome) and records it as a `usd-replay/1`
replay with usd-recorder's C++ writer. It is deterministic: two runs produce identical files.

```sh
git clone https://github.com/Preponderous-Software/usd-recorder   # tested at 1aae66d
g++ -std=c++17 -O2 -I usd-recorder/cpp/include restore.cpp -o restore
./restore kindling-restore.usda                   # 785 samples

pip install numpy usd-core ./usd-recorder "usd-web @ git+https://github.com/Preponderous-Software/usd-web"
usd-recorder replay validate kindling-restore.usda
usd-recorder replay convert kindling-restore.usda kindling-restore.usdc
usd-web replay kindling-restore.usdc -o site/replay
```

Copy usd-web's `viewer/replay.html` and `viewer/usd-web-replay.js` next to `site/replay/` and
serve the folder to play it. Output:
[`replay-start.jpg`](../docs/images/replay-start.jpg) and
[`replay-end.jpg`](../docs/images/replay-end.jpg).

The room's coordinates match the Strata scene (y up). The replay is Z-up, so a room point
`(x, y, z)` is written as `(x, -z, y)`.
