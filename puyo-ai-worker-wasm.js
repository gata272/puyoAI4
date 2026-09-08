/* WASM worker for PuyoAI3 */
import createPuyoAI from './puyoAI_wasm.mjs';

let moduleInstance = null;
let chooseMove = null;
let resetAI = null;
let setBoardCell = null;
let getPatternName = null;

function postLog(message) {
    self.postMessage({ type: 'log', message });
}

async function init() {
    try {
        postLog('WASM module initialization started');

        moduleInstance = await createPuyoAI();

        chooseMove = moduleInstance.cwrap(
            'ai_choose_move',
            'number',
            [
                'number',
                'number', 'number',
                'number', 'number',
                'number', 'number'
            ]
        );

        resetAI = moduleInstance.cwrap(
            'reset_ai',
            null,
            []
        );

        setBoardCell = moduleInstance.cwrap(
            'set_board_cell',
            null,
            ['number', 'number']
        );

        getPatternName = moduleInstance.cwrap(
            'get_ai_pattern_name',
            'string',
            []
        );

        self.postMessage({ type: 'ready' });
    } catch (error) {
        self.postMessage({
            type: 'error',
            message: `WASM初期化失敗: ${error && error.message ? error.message : error}`
        });
    }
}

const ready = init();

self.onmessage = async (event) => {
    await ready;

    const msg = event.data || {};

    if (msg.type === 'reset') {
        if (resetAI) resetAI();
        return;
    }

    if (msg.type !== 'think') return;

    try {
        const board = new Uint8Array(msg.boardBuffer || []);
        const pieces = new Uint8Array(msg.pieceBuffer || []);

        for (let i = 0; i < board.length; ++i) {
            setBoardCell(i, board[i]);
        }

        const result = chooseMove(
            msg.turn | 0,
            pieces[1] | 0, pieces[0] | 0,
            pieces[3] | 0, pieces[2] | 0,
            pieces[5] | 0, pieces[4] | 0
        );

        if (result < 0) {
            self.postMessage({
                type: 'error',
                message: '合法な手を生成できませんでした'
            });
            return;
        }

        const x = Math.floor(result / 10);
        const rotation = result % 10;
        const patternName = getPatternName
            ? getPatternName()
            : '';

        self.postMessage({
            type: 'move',
            x,
            rotation,
            patternName: patternName === 'NONE' ? '' : patternName
        });
    } catch (error) {
        self.postMessage({
            type: 'error',
            message: `AI実行失敗: ${error && error.message ? error.message : error}`
        });
    }
};
