# Kindling in a browser

Kindling has a browser build at **https://kindling.play.danielstephenson.dev**, one of the games on
[danielstephenson.dev/play](https://danielstephenson.dev/play). It is the same game as the desktop
build: the same rules, scene, HUD and session code, drawn by Strata's WebGL 2 backend (Strata
`docs/WEB.md`) and compiled with Emscripten.

## What differs from the desktop game

| | Desktop (`KINDLING_WITH_STRATA`) | Browser (`KINDLING_WEB`) |
|---|---|---|
| Rules, scene, HUD, session (`src/sim`, `src/bridge`, `src/play`) | the same code | the same code. CI runs the whole test suite as wasm under Node, the pinned determinism and playthrough fingerprints included |
| Entry point | `app/main.cpp` (its own frame loop) | `app/web_main.cpp` (the browser runs the frame loop) |
| Saves | `kindling.save` in the slot directory (`%APPDATA%\Kindling\default`, `~/.local/share/kindling/default`, or `--slot`) | the same format and code (`src/save`), in `/kindling-save/default`, which the page keeps in IndexedDB. Saves stay in that browser on that device |
| When it saves | every two minutes, F5, and on quit | every two minutes, F5 or the Save button, and when the page is hidden or closed |
| Mouse | captured by the window; Esc frees it, Esc again quits | the page takes the pointer lock on a click; Esc gives it back. There is no quit |
| Touch | none | a stick (drag on the left half), look (drag on the right half), Work / Take / Turn / Lamp buttons and a tool bar |
| HUD | drawn on the canvas | drawn on the canvas on a wide page; below 760 CSS pixels wide the page shows the checklist, the crosshair and the words itself |
| Shadows | 4 cascades of 2048² | 3 cascades of 1024², for phones |
| Resolution | the window's | the page's size times the device pixel ratio, capped at 2 |
| F12 screenshot, the restoration replay file | yes | no |

### Saves

The browser build writes the desktop save format with the desktop code: the checksum is checked
before a byte of a save is read, a new save is written to a temporary file and moved into place,
and the save it replaces is kept as the previous save, loaded instead if the current one is
damaged. The page mounts `/kindling-save` on IndexedDB (Emscripten's IDBFS), reads it before the
game starts, and flushes it to IndexedDB after every save. If the browser refuses to store it, the
page says "Could not store the save in this browser".

Saves belong to the page's origin, so they stay with `kindling.play.danielstephenson.dev` in that
browser. Clearing the site's data deletes them. A browser save and a desktop save are the same
format, but nothing moves one to the other.

**New** on the page starts again from the ruined cabin after asking. The current progress is
replaced the next time the game saves (it is then kept as the previous save until the save after
that).

## Controls

On a keyboard and mouse, the desktop controls (see the README), except that Esc only frees the
mouse and F12 does nothing. On a touch screen:

- drag on the left half to walk (all the way out to hurry), and on the right half to look;
- tap a tool on the bar along the bottom, then hold **Work** to use it on what the crosshair is on;
- **Take** picks up or puts down, **Turn** turns what you carry, **Lamp** lights the lantern, and
  **Keys** shows the keyboard controls on the canvas;
- **Save**, **Replay** (once Ada is home), **New** and **?** (how to play) are along the top.

## Building

Needs emsdk (CI pins 6.0.11, as Strata does) and CMake 3.24 or newer:

```sh
emcmake cmake -S . -B build-web -DKINDLING_WITH_STRATA=OFF -DKINDLING_WEB=ON
cmake --build build-web -j8
ctest --test-dir build-web --output-on-failure   # the test suite, as wasm under Node
cd build-web && python3 -m http.server 8000      # open http://localhost:8000/kindling.html
```

`-DFETCHCONTENT_SOURCE_DIR_STRATA=<checkout>` builds against a local Strata checkout instead of the
pinned commit. The page is `kindling.html` + `kindling.js` + `kindling.wasm` (about 1.5 MB); the
page shell, with the touch controls and the IndexedDB mount, is `web/shell.html`.

## The loading shell

The page loads behind the Strata web kit's loading shell
(`web/strata-kit/strata-loader.js`, vendored from Strata at the commit in
`web/strata-kit/PIN`; `web/strata-kit/update.sh <sha>` re-vendors it). It shows
the trailer's first frame (the one on <https://danielstephenson.dev/play>) as a blurred backdrop and real progress with the size ("0.5 of 1.3 MB"). The loader streams
the `.wasm` and hands its bytes to the engine, so nothing downloads twice. If
Kindling can't run (no WebGL 2, the renderer fails, the engine stops, a lost
WebGL context does not come back), the shell says why, and plays the trailer instead (the still under `prefers-reduced-motion`).
A device that draws under 12 frames a second is offered the clip without
losing the game (not in automated browsers or `?bench` runs).

`web/strata-boot.js` bridges this classic (non-`MODULARIZE`) build to the
loader: emcc's script tag sits inert in `<template id="strata-glue">`, the
bridge starts it, and the engine waits in `Module.instantiateWasm` for the
loader's bytes. After each link, `web/strata_page.cmake` puts the loader and
the bridge inline in the page (so the site is still the same three files) and stamps the download's size on the
template, because the arcade compresses on the fly and sends no
`Content-Length`.

## CI and deployment

`.github/workflows/browser.yml` builds the page, runs the tests under Node, and smoke-tests the page
in headless Chromium (SwiftShader) with `web/smoke/smoke.cjs`, on a desktop viewport and on the
iPhone 13 profile. The smoke test checks that the page starts without errors and shows How to play
on a first visit, that the canvas draws the cabin, that walking moves the player (W, or the touch
stick) and looking turns them, that the pointer lock is taken and given back (desktop), that a
tool can be taken (a number key, or the tool bar), and that a save survives a reload. It uploads
its screenshots.

Headless Chromium fakes mouse movement under the pointer lock (it jumps to the cursor and back),
so the smoke test looks through the same call the page's mouse handler makes; how the mouse feels
is checked by hand in a real browser.

When the repository variable `ARCADE_ENABLED` is `true`, a push to `main` deploys the page to
arcade with `Stephenson-Software/arcade-deploy` and the `ARCADE_TOKEN` secret (a manual run of the
workflow deploys too).
