/* PuyoAI3 browser bridge
 * - Keeps the existing simulator/online UI untouched.
 * - Sends the current board and the next three pairs to the WASM AI.
 * - The WASM AI uses GTR for the opening plan and then Beam Search +
 *   ama-style linear evaluation.
 */
(function (global) {
    'use strict';

    const CONFIG = {
        WORKER_PATH: './puyo-ai-worker-wasm.js',
        TICK_MS: 120,
        WIDTH: 6,
        HEIGHT: 14
    };

    const STATE = {
        worker: null,
        workerReady: false,
        autoEnabled: false,
        busy: false,
        turn: 0,
        timer: null
    };

    function status(text) {
        const el = document.getElementById('ai-status');
        if (el) el.textContent = text;
    }

    function updateButton() {
        const btn = document.getElementById('ai-auto-button');
        if (!btn) return;

        btn.textContent = STATE.autoEnabled ? 'AI自動: ON' : 'AI自動: OFF';
        btn.style.backgroundColor = STATE.autoEnabled ? '#4CAF50' : '#8e44ad';
    }

    function makePieces() {
        const current = global.currentPuyo;
        if (!current) return [];

        const pieces = [{
            mainColor: current.mainColor | 0,
            subColor: current.subColor | 0
        }];

        const queue = Array.isArray(global.nextQueue)
            ? global.nextQueue
            : [];
        const index = Number.isFinite(global.queueIndex)
            ? global.queueIndex
            : 0;

        for (let i = 0; i < 2; ++i) {
            const pair = queue[index + i];
            if (!pair || pair.length < 2) break;
            pieces.push({
                mainColor: pair[1] | 0,
                subColor: pair[0] | 0
            });
        }

        return pieces;
    }

    function makeBoardBuffer() {
        const result = new Uint8Array(
            CONFIG.WIDTH * CONFIG.HEIGHT
        );

        const board = global.board;
        if (!Array.isArray(board)) return result.buffer;

        for (let y = 0; y < CONFIG.HEIGHT; ++y) {
            for (let x = 0; x < CONFIG.WIDTH; ++x) {
                const value =
                    board[y] && Number.isFinite(board[y][x])
                        ? board[y][x]
                        : 0;
                result[y * CONFIG.WIDTH + x] = value & 0xff;
            }
        }

        return result.buffer;
    }

    function makePieceBuffer(pieces) {
        const result = new Uint8Array(6);

        for (let i = 0; i < 3; ++i) {
            if (!pieces[i]) continue;
            result[i * 2] = pieces[i].mainColor & 0xff;
            result[i * 2 + 1] = pieces[i].subColor & 0xff;
        }

        return result.buffer;
    }

    function initWorker() {
        if (STATE.worker) return;

        STATE.worker = new Worker(
            CONFIG.WORKER_PATH,
            { type: 'module' }
        );

        STATE.worker.onmessage = (event) => {
            const msg = event.data || {};

            if (msg.type === 'ready') {
                STATE.workerReady = true;
                status('WASM AI 準備完了');
                return;
            }

            if (msg.type === 'log') {
                console.log('[AI]', msg.message);
                return;
            }

            if (msg.type === 'error') {
                console.error('[AI]', msg.message);
                STATE.busy = false;
                status('AIエラー');
                return;
            }

            if (msg.type !== 'move') return;

            STATE.busy = false;

            if (!Number.isFinite(msg.x) || !Number.isFinite(msg.rotation)) {
                status('AI: 手を生成できません');
                return;
            }

            const pattern = msg.patternName;
            if (pattern) {
                status(`AI: GTR ${pattern}`);
            } else {
                status('AI: ama型評価 + Beam Search');
            }

            executeMove(msg.x, msg.rotation);
            ++STATE.turn;
        };
    }

    function executeMove(x, rotation) {
        if (!global.currentPuyo) return;

        global.currentPuyo.mainX = x;
        global.currentPuyo.rotation = rotation;

        // puyoSim.js keeps these variables in its own global scope.
        // Assigning the object is sufficient for rendering; the explicit
        // render call is only a visual update before hardDrop.
        if (typeof global.renderBoard === 'function') {
            global.renderBoard();
        }

        setTimeout(() => {
            if (!global.currentPuyo) return;
            if (global.gameState !== 'playing') return;

            if (typeof global.hardDrop === 'function') {
                global.hardDrop();
            }
        }, 20);
    }

    function think() {
        if (!STATE.autoEnabled ||
            !STATE.workerReady ||
            STATE.busy) {
            return;
        }

        if (global.gameState !== 'playing') return;
        if (!global.currentPuyo) return;

        const pieces = makePieces();
        if (pieces.length === 0) return;

        // The GTR planner requires three pairs. After that, two additional
        // pieces are sufficient for the search; if the queue is temporarily
        // short, simply wait for the next tick.
        if (STATE.turn < 3 && pieces.length < 3) return;

        STATE.busy = true;
        status(
            STATE.turn < 3
                ? 'AI: GTR構築中...'
                : 'AI: 盤面評価中...'
        );

        STATE.worker.postMessage({
            type: 'think',
            turn: STATE.turn,
            boardBuffer: makeBoardBuffer(),
            pieceBuffer: makePieceBuffer(pieces)
        });
    }

    function resetAI() {
        STATE.turn = 0;
        STATE.busy = false;

        if (STATE.worker && STATE.workerReady) {
            STATE.worker.postMessage({ type: 'reset' });
        }

        status('WASM AI 待機中');
    }

    global.toggleAI = function () {
        STATE.autoEnabled = !STATE.autoEnabled;

        if (STATE.autoEnabled) {
            initWorker();

            if (STATE.timer) clearInterval(STATE.timer);
            STATE.timer = setInterval(think, CONFIG.TICK_MS);

            status(
                STATE.workerReady
                    ? 'AI自動: ON'
                    : 'WASM AI 初期化中...'
            );
        } else {
            if (STATE.timer) {
                clearInterval(STATE.timer);
                STATE.timer = null;
            }

            STATE.busy = false;
            status('AI自動: OFF');
        }

        updateButton();
    };

    global.resetAIState = resetAI;

    // resetGame() is defined by puyoSim.js. Wrap it once so the AI's
    // GTR turn counter always starts from zero after a game reset.
    const originalResetGame = global.resetGame;
    if (typeof originalResetGame === 'function') {
        global.resetGame = function () {
            originalResetGame.apply(this, arguments);
            resetAI();
        };
    }

    updateButton();
    status('WASM AI 待機中');

})(window);
