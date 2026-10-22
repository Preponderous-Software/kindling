# Kindling

A cozy frontier homestead game: bring an abandoned valley back to life, one cabin, one neighbour
and one winter at a time. Kindling runs on [Strata](https://github.com/Preponderous-Software/strata),
a small C++ engine.

![Concept key art: a log cabin at dusk in a mountain valley, its window lit and chimney smoking](docs/images/concept-key-art.png)

*Concept key art (a mood sketch drawn in code, not in-game art).*

**Status: concept.** There is no playable game yet. This repository holds the design documents and
two small prototypes that show the core idea working on the real engine and file formats.

## The idea

You arrive in Cottonwood Fork with a horse, a wagon and a deed to a claim nobody has lived on for
years. You restore each cabin from ruin to home: survey, clear, repair, furnish, then **kindle**,
lighting the first fire in its hearth. Then a settler moves in. Over five winters the valley comes
back to life.

There is no combat and no way to lose. Winter makes settlers chilly and grumpy, never hungry or
hurt. The game is about making places beautiful and lived-in for people you come to care about.

## In the engine

The same room before and after restoration, rendered headless by Strata 0.3.0. The only light in
the first image is moonlight; the second adds the hearth.

| Before | After |
|---|---|
| ![A ruined cabin room at night: the roof has fallen in, a broken rafter leans to the floor, snow lies under the hole, the window is boarded and the hearth is cold](docs/images/strata-hearth-before.jpg) | ![The same room restored: a fire burns in the stone hearth, a braided rug lies on the floor, a quilt hangs on the rocking chair and curtains frame a snowy window](docs/images/strata-hearth-after.jpg) |

The whole restoration of that room, recorded tick by tick as a
[`usd-replay/1`](https://github.com/Preponderous-Software/usd-recorder/blob/1aae66d/docs/REPLAY.md)
replay and played in [usd-web](https://github.com/Preponderous-Software/usd-web)'s browser player:

| Start (tick 0) | End (tick 3,134) |
|---|---|
| ![Top-down replay of the ruined room: debris scattered across a dusty floor, holes in the planks, a snow drift by the hearth](docs/images/replay-start.jpg) | ![The same replay at the end: clean floorboards, a braided rug, furniture in place, the fire lit and the settler at the rocking chair](docs/images/replay-end.jpg) |

Both prototypes are in [`prototypes/`](prototypes/README.md), with the commands that produce these
images.

## Documents

| Document | What it covers |
|---|---|
| [CONCEPT.md](CONCEPT.md) | The game: player fantasy, pillars, the core loop, seasons, settlers, the valley, art direction |
| [MVP.md](MVP.md) | The first playable slice: what is in, what is out, milestones and how each is checked |
| [ARCHITECTURE.md](ARCHITECTURE.md) | How it is built: the engine boundary, game modules, saves, content, replays, the browser build |

## Built on

- [Strata](https://github.com/Preponderous-Software/strata): rendering, the window and input,
  checksummed saves (`modules/save`), OpenUSD (`modules/usd`), and deterministic simulation pieces
  (`strata::sim`).
- [usd-recorder](https://github.com/Preponderous-Software/usd-recorder): its C++ replay writer
  records each restoration for the reveal timelapse.
- [usd-web](https://github.com/Preponderous-Software/usd-web): plays those replays in a browser.

[steading](https://github.com/Preponderous-Software/steading), a colony sim on the same engine, is
the closest sibling: Kindling borrows its patterns for settlers, seasons and saves.

## License

[MIT](LICENSE).
