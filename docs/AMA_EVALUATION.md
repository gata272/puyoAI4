# Ama-style evaluation (updated)

This repository now implements a more faithful, portable version of the public
`citrus610/ama` beam evaluator.

## Implemented

- Public `build` weights from ama are retained.
- Human-form matching now ports the public GTR / SGTR / FRON pattern tables.
- `link_2` / `link_3` follows the public SIMD mask definition using a scalar
  equivalent, so it is no longer the earlier endpoint-count approximation.
- `get_chi`, `shape`, `well`, `bump`, `waste_14`, side bias and nuisance count
  follow the public formulas.
- Quiescence now follows ama's `quiet::generate/search` concept: for each
  reachable column and color, add up to three single puyos and detect the first
  immediate trigger. This is a *drop depth of 3*, not a recursive game depth.
- The action waste feature is now the number of popped puyos, matching ama's
  public `pop.get_size()` use. Tear remains a portable approximation because
  the browser simulator does not expose ama's exact frame-count path metric.

## Deliberate differences

The implementation is not a bit-for-bit port. Ama uses a SIMD bitfield board,
its own exact movement/frame accounting, transposition tables, and a larger
search framework. This project keeps the simulator simple and side-effect-free
so it can be compiled to WebAssembly and share the browser game's board model.

The human-form patterns are sourced from ama's public `beam/form.h`, and the
quiescence/evaluation definitions are based on its public `beam/eval.cpp` and
`beam/quiet.cpp`.

## Search settings

`config/search.json` is the production baseline:

- search depth: 3
- beam width: 8
- quiescence single-puyo drop: 3
- continuation discount: 0.85

The wider-beam benchmark is available with `make benchmark`. It should be run
on the target machine before increasing beam width because the scalar evaluator
is deliberately portable and is slower than ama's SSE/BMI2 implementation.

## Automatic tuning

`make tuner` builds an SPSA tuner. It evaluates a fixed random corpus with
common random numbers, perturbs all 15 weights simultaneously, and writes a
candidate profile to `config/weights_tuned_experimental.json`.

The tuner is intended for parameter research, not as an assertion that one
short random corpus produces globally optimal Puyo Puyo play. For meaningful
results, increase games/turns and compare several independent seeds.

The checked-in experimental file was produced with only a tiny local sample (2 games × 20 turns) and a shallow tuning search, so it is intentionally **not** used by the browser AI. It is included as a reproducible example of the tuner output.
