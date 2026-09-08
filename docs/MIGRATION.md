# Migration from puyoAI2

## Kept

- `puyoSim.js`: game simulation, UI state, stage-code support, undo/redo, ojama handling.
- `online.js` / `online.css`: PeerJS online match.
- `index.html`, `style.css`, PWA icons, `manifest.json`, `sw.js`.

## Replaced

- `puyoAI.cpp` -> `ai/gtr/gtr_ai.cpp` plus the new AI/search/simulation modules.
- `puyoAI_updated.js` -> `puyoAI.js`.
- `puyo-ai-worker-wasm.js` -> new WASM worker.
- the old JavaScript search (`puyoAI.js`/`puyo-ai-worker.js`) is removed because it duplicated the AI implementation.

## Not committed

Generated `puyoAI_wasm.mjs` and `puyoAI_wasm.wasm` are build artifacts. GitHub Actions creates them on deployment.
