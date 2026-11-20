# Strata noise (vendored)

`include/strata/sim/noise.hpp` and `src/noise.cpp` are copied unchanged from
[Strata](https://github.com/Preponderous-Software/strata) `main` at commit
`649f0a52a70937c5629251457ab7a17c04c05351` (Strata PR #103, after v0.7.0),
`modules/sim/include/strata/sim/noise.hpp` and `modules/sim/src/noise.cpp`: seeded 2D
value noise and fractal Brownian motion, deterministic on every compiler when built with
`-ffp-contract=off`. MIT licensed, see [LICENSE](LICENSE). Strata v0.8.0 (commit
`5ea7e14f0e59618179c11fe9784e9b4102acc0f1`, the one `KINDLING_STRATA_REF` in
`CMakeLists.txt` pins) carries both files byte for byte as copied here.

They are vendored rather than linked from Strata's `strata::sim` module so the rules library
keeps building without Strata (and the browser build, which turns Strata's sim module off,
keeps working), and, when they were copied, so the game did not have to pin an unreleased
Strata commit. The determinism test checks the copy against the values Strata's own unit test
pins. Update it by copying the files from a newer Strata commit and bumping this note. Now
that a Strata release carries `noise.hpp`, linking `strata::sim` instead is an option.
