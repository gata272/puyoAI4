/* Deterministic maximum-chain benchmark worker. */
import createPuyoAI from './puyoAI_wasm.mjs';

let moduleInstance = null;
let runBenchmark = null;

async function init() {
    moduleInstance = await createPuyoAI();
    runBenchmark = moduleInstance.cwrap(
        'run_chain_benchmark',
        'string',
        ['number', 'number', 'number', 'number', 'number']
    );
    self.postMessage({ type: 'ready' });
}

const ready = init().catch((error) => {
    self.postMessage({
        type: 'error',
        message: `ベンチマークWASM初期化失敗: ${error?.message || error}`
    });
    throw error;
});

self.onmessage = async (event) => {
    await ready;
    const msg = event.data || {};
    if (msg.type !== 'run') return;

    try {
        self.postMessage({ type: 'started' });
        const resultJson = runBenchmark(
            msg.games | 0,
            msg.turns | 0,
            msg.seed | 0,
            msg.depth | 0,
            msg.beamWidth | 0
        );
        self.postMessage({ type: 'result', resultJson });
    } catch (error) {
        self.postMessage({
            type: 'error',
            message: `ベンチマーク実行失敗: ${error?.message || error}`
        });
    }
};
