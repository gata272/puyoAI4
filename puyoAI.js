/* PuyoAI10 trigger-transfer browser bridge
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
        HEIGHT: 14,
        DEFAULT_DEPTH: 3,
        DEFAULT_BEAM_WIDTH: 24,
        STORAGE_KEY: 'puyoAI.searchSettings'
    };

    const STATE = {
        worker: null,
        workerReady: false,
        autoEnabled: false,
        busy: false,
        turn: 0,
        timer: null,
        decisionLogs: [],
        pendingDecision: null,
        logSessionStartedAt: new Date().toISOString(),
        gameId: 1
    };


    function getSearchSettings() {
        const fallback = { depth: CONFIG.DEFAULT_DEPTH, beamWidth: CONFIG.DEFAULT_BEAM_WIDTH };
        try {
            const saved = JSON.parse(localStorage.getItem(CONFIG.STORAGE_KEY) || 'null');
            return {
                depth: Number.isFinite(saved?.depth) ? Math.max(1, Math.min(50, Math.trunc(saved.depth))) : fallback.depth,
                beamWidth: Number.isFinite(saved?.beamWidth) ? Math.max(1, Math.min(500, Math.trunc(saved.beamWidth))) : fallback.beamWidth
            };
        } catch (_) {
            return fallback;
        }
    }

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

    function readStoredAIWeights() {
        try {
            const saved = JSON.parse(localStorage.getItem('puyoAI.developerWeights') || 'null');
            return Array.isArray(saved)
                ? saved.map(v => Number.isFinite(Number(v)) ? Number(v) : null)
                : null;
        } catch (_) {
            return null;
        }
    }

    function applyStoredAIWeights() {
        const values = readStoredAIWeights();
        if (values && STATE.worker && STATE.workerReady) {
            STATE.worker.postMessage({ type: 'weights', values });
        }
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
                applyStoredAIWeights();
                if (typeof global.requestAIWeights === 'function') global.requestAIWeights();
                return;
            }

            if (msg.type === 'log') {
                console.log('[AI]', msg.message);
                return;
            }
            if (msg.type === 'weights') {
                if (typeof global.renderDeveloperWeights === 'function') {
                    global.renderDeveloperWeights(msg.weights || []);
                }
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
            if (STATE.pendingDecision) {
                STATE.pendingDecision.selected = {
                    x: msg.x | 0,
                    rotation: msg.rotation | 0
                };
                STATE.pendingDecision.patternName = pattern === 'NONE' ? '' : (pattern || '');
                STATE.pendingDecision.internalDebugLog = String(msg.debugLog || '');
            }
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

    function snapshotBoard() {
        const board = Array.isArray(global.board) ? global.board : [];
        return Array.from({ length: CONFIG.HEIGHT }, (_, y) =>
            Array.from({ length: CONFIG.WIDTH }, (_, x) => {
                const value = board[y] && Number.isFinite(board[y][x])
                    ? board[y][x]
                    : 0;
                return value | 0;
            })
        );
    }

    function boardHeights(board) {
        return Array.from({ length: CONFIG.WIDTH }, (_, x) => {
            for (let y = CONFIG.HEIGHT - 1; y >= 0; --y) {
                if (board[y][x] !== 0) return y + 1;
            }
            return 0;
        });
    }

    function pieceSnapshot(pieces) {
        return pieces.map(pair => ({
            main: pair.mainColor | 0,
            sub: pair.subColor | 0
        }));
    }

    function nowMs() {
        return typeof performance !== 'undefined' && performance.now
            ? performance.now()
            : Date.now();
    }

    function beginDecisionLog(turn, pieces, depth, beamWidth, board) {
        STATE.pendingDecision = {
            gameId: STATE.gameId,
            turn,
            startedAt: new Date().toISOString(),
            thinkStartMs: nowMs(),
            depth,
            beamWidth,
            pieces: pieceSnapshot(pieces),
            preBoard: board,
            preHeights: boardHeights(board),
            scoreBefore: Number.isFinite(global.score) ? global.score : null,
            selected: null,
            patternName: '',
            internalDebugLog: '',
            resolved: false
        };
    }

    function updateDecisionLogStatus() {
        const el = document.getElementById('ai-decision-log-status');
        if (!el) return;
        el.textContent = `記録: ${STATE.decisionLogs.length}手` +
            (STATE.pendingDecision ? '（思考中）' : '');
    }

    global.__aiNotifyPlacement = function (result) {
        const pending = STATE.pendingDecision;
        if (!pending) return;
        const placementBoard = Array.isArray(result?.board)
            ? result.board
            : snapshotBoard();
        pending.placementBoard = placementBoard;
        pending.placementHeights = boardHeights(placementBoard);
        pending.placementGroups = Array.isArray(result?.groups)
            ? result.groups
            : [];
        pending.placementScore = Number.isFinite(result?.score)
            ? result.score
            : null;
    };

    function appendResolvedDecision(result) {
        const pending = STATE.pendingDecision;
        if (!pending) return;

        const postBoard = Array.isArray(result?.board)
            ? result.board
            : snapshotBoard();
        const completed = {
            ...pending,
            thinkMs: Math.max(0, nowMs() - pending.thinkStartMs),
            postBoard,
            postHeights: boardHeights(postBoard),
            chain: Number.isFinite(result?.chain) ? result.chain : 0,
            scoreBefore: Number.isFinite(pending.scoreBefore) ? pending.scoreBefore : (Number.isFinite(result?.scoreBefore) ? result.scoreBefore : null),
            scoreAfter: Number.isFinite(result?.scoreAfter) ? result.scoreAfter : null,
            scoreDelta: Number.isFinite(pending.scoreBefore) && Number.isFinite(result?.scoreAfter)
                ? result.scoreAfter - pending.scoreBefore
                : null,
            gameState: result?.gameState || global.gameState || 'unknown',
            gameOver: !!result?.gameOver,
            pendingOjama: Number.isFinite(result?.pendingOjama) ? result.pendingOjama : null,
            resolvedAt: new Date().toISOString(),
            resolved: true
        };
        delete completed.thinkStartMs;
        STATE.decisionLogs.push(completed);
        STATE.pendingDecision = null;
        updateDecisionLogStatus();
    }

    function startNewGameLog() {
        STATE.pendingDecision = null;
        STATE.gameId += 1;
        updateDecisionLogStatus();
    }

    function clearDecisionLogSession() {
        STATE.decisionLogs = [];
        STATE.pendingDecision = null;
        STATE.logSessionStartedAt = new Date().toISOString();
        STATE.gameId = 1;
        updateDecisionLogStatus();
    }

    function getDecisionLogPayload() {
        const logs = STATE.decisionLogs.slice();
        if (STATE.pendingDecision) logs.push({
            ...STATE.pendingDecision,
            thinkMs: Math.max(0, nowMs() - STATE.pendingDecision.thinkStartMs),
            status: 'pending'
        });
        return {
            format: 'puyoAI-decision-log-v1',
            generatedAt: new Date().toISOString(),
            sessionStartedAt: STATE.logSessionStartedAt,
            gameCount: new Set(logs.map(entry => entry.gameId)).size,
            settings: getSearchSettings(),
            entries: logs
        };
    }

    function downloadDecisionLog() {
        const payload = getDecisionLogPayload();
        if (!payload.entries.length) {
            status('AI手ログがまだありません');
            return;
        }
        const text = JSON.stringify(payload, null, 2) + '\n';
        const blob = new Blob([text], { type: 'application/json;charset=utf-8' });
        const url = URL.createObjectURL(blob);
        const link = document.createElement('a');
        const stamp = new Date().toISOString().replace(/[:.]/g, '-');
        link.href = url;
        link.download = `puyoAI-decision-log-${stamp}.json`;
        document.body.appendChild(link);
        link.click();
        link.remove();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
        status(`AI手ログを保存しました（${payload.entries.length}手）`);
    }

    async function copyDecisionLog() {
        const payload = getDecisionLogPayload();
        if (!payload.entries.length) {
            status('AI手ログがまだありません');
            return;
        }
        const text = JSON.stringify(payload, null, 2) + '\n';
        try {
            await navigator.clipboard.writeText(text);
            status('AI手ログをクリップボードにコピーしました');
        } catch (_) {
            const area = document.createElement('textarea');
            area.value = text;
            area.style.position = 'fixed';
            area.style.opacity = '0';
            document.body.appendChild(area);
            area.focus();
            area.select();
            let ok = false;
            try { ok = document.execCommand('copy'); } catch (_) {}
            area.remove();
            status(ok ? 'AI手ログをクリップボードにコピーしました' : 'コピーに失敗しました');
        }
    }

    global.downloadAIDecisionLog = downloadDecisionLog;
    global.copyAIDecisionLog = copyDecisionLog;
    global.clearAIDecisionLog = function () {
        clearDecisionLogSession();
        status('AI手ログをクリアしました');
    };
    global.getAIDecisionLog = getDecisionLogPayload;

    global.__aiNotifyTurnResolved = function (result) {
        appendResolvedDecision(result || {});
    };

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

        // The GTR planner requires three pairs. The post-GTR search can use
        // up to three total pairs (current + two lookahead).
        if (STATE.turn < 3 && pieces.length < 3) return;

        STATE.busy = true;
        status(
            STATE.turn < 3
                ? 'AI: GTR構築中...'
                : 'AI: 盤面評価中...'
        );

        const searchSettings = getSearchSettings();
        const inputBoard = snapshotBoard();
        beginDecisionLog(STATE.turn, pieces, searchSettings.depth, searchSettings.beamWidth, inputBoard);
        updateDecisionLogStatus();
        let debug = false;
        try {
            debug = localStorage.getItem('puyoAI.debugMode') === 'true';
        } catch (_) {}

        STATE.worker.postMessage({
            type: 'think',
            turn: STATE.turn,
            depth: searchSettings.depth,
            beamWidth: searchSettings.beamWidth,
            debug,
            boardBuffer: makeBoardBuffer(),
            pieceBuffer: makePieceBuffer(pieces)
        });
    }

    function resetAI() {
        STATE.turn = 0;
        STATE.busy = false;
        startNewGameLog();

        if (STATE.worker && STATE.workerReady) {
            STATE.worker.postMessage({ type: 'reset' });
        }

        status('WASM AI 待機中');
    }

    global.loadAISearchSettings = function () {
        const settings = getSearchSettings();
        const depth = document.getElementById('ai-depth');
        const beam = document.getElementById('ai-beam');
        if (depth) depth.value = settings.depth;
        if (beam) beam.value = settings.beamWidth;
    };

    global.saveAISearchSettings = function () {
        const read = (id, fallback, min, max) => {
            const value = Number.parseInt(document.getElementById(id)?.value, 10);
            return Number.isFinite(value) ? Math.max(min, Math.min(max, value)) : fallback;
        };
        const settings = {
            depth: read('ai-depth', CONFIG.DEFAULT_DEPTH, 1, 50),
            beamWidth: read('ai-beam', CONFIG.DEFAULT_BEAM_WIDTH, 1, 500)
        };
        try {
            localStorage.setItem(CONFIG.STORAGE_KEY, JSON.stringify(settings));
        } catch (_) {}
        const depth = document.getElementById('ai-depth');
        const beam = document.getElementById('ai-beam');
        if (depth) depth.value = settings.depth;
        if (beam) beam.value = settings.beamWidth;
        status(`AI設定: depth ${settings.depth} / beam ${settings.beamWidth}`);
    };

    global.requestAIWeights = function () {
        if (!STATE.worker) initWorker();
        if (!STATE.worker || !STATE.workerReady) return;
        STATE.worker.postMessage({ type: 'weights', values: readStoredAIWeights() || [] });
    };

    global.applyAIWeights = function (values) {
        if (!STATE.worker) initWorker();
        if (!STATE.worker || !STATE.workerReady) return;
        STATE.worker.postMessage({
            type: 'weights',
            values: Array.isArray(values) ? values : []
        });
    };

    global.resetAIWeights = function () {
        try { localStorage.removeItem('puyoAI.developerWeights'); } catch (_) {}
        if (!STATE.worker) initWorker();
        if (!STATE.worker || !STATE.workerReady) return;
        STATE.worker.postMessage({ type: 'weights', reset: true, values: [] });
    };

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
