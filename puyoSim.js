// ぷよぷよシミュレーション（おじゃま対応 完全版）
// - NextQueue方式
// - 連鎖安全化
// - Undo / Redo
// - Edit / Play
// - おじゃまぷよ送受信・相殺・落下
// - オンライン同期用フック
// - 連戦用フック追加

// 盤面サイズ
const WIDTH = 6;
const HEIGHT = 14; // 全行数（インデックス 0..HEIGHT-1）
const HIDDEN_ROWS = 2; // 上部の隠し行数（可視領域 = HEIGHT - HIDDEN_ROWS）
const MAX_NEXT_PUYOS = 50;
const NUM_VISIBLE_NEXT_PUYOS = 2; // 表示する NEXT の数 (NEXT 1とNEXT 2)

// ぷよの色定義
const COLORS = {
    EMPTY: 0,
    RED: 1,
    BLUE: 2,
    GREEN: 3,
    YELLOW: 4,
    GARBAGE: 5 // おじゃまぷよ
};

// スコア計算の値（ボーナステーブル）
const BONUS_TABLE = {
    CHAIN: [0, 8, 16, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480, 512],
    GROUP: [0, 0, 0, 0, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
    COLOR: [0, 0, 3, 6, 12]
};

// 履歴管理パラメータ
const MAX_HISTORY_SIZE = 300;

// おじゃまボーナス
const ALL_CLEAR_SCORE_BONUS = 2100;

// PT2寄せの換算・制限
const NUISANCE_TARGET_POINTS = 70;
const MAX_OJAMA_DROP_PER_TURN = 30;

// NEXTは128組で色を均等にする
const NEXT_QUEUE_BALANCE_BATCH_SIZE = 128;
const NEXT_QUEUE_COLORS = [COLORS.RED, COLORS.BLUE, COLORS.GREEN, COLORS.YELLOW];

// ゲームオーバー表示用
const BOARD_GAMEOVER_CLASS = 'board-gameover';

// ===== Debug stats (?debug=1 のときだけ表示) =====
const DEBUG_MODE = new URLSearchParams(window.location.search).get('debug') === '1';

const DEBUG_STATS = {
    startedAt: performance.now(),
    turns: 0,              // 経過ターン数
    maxChain: 0,           // 最大連鎖数
    chainSum: 0,           // 連鎖長の合計
    chainEpisodes: 0,      // 連鎖が発生した回数
    hist: Array(20).fill(0) // 10～19 を使う
};

function debugEnsurePanel() {
    if (!DEBUG_MODE) return;
    if (document.getElementById('debug-stats-panel')) return;

    const panel = document.createElement('div');
    panel.id = 'debug-stats-panel';
    panel.style.cssText = [
        'position:fixed',
        'top:8px',
        'right:8px',
        'z-index:99999',
        'background:rgba(0,0,0,0.82)',
        'color:#fff',
        'border:1px solid #555',
        'border-radius:8px',
        'padding:10px',
        'font-size:12px',
        'line-height:1.5',
        'min-width:180px',
        'pointer-events:none',
        'white-space:nowrap'
    ].join(';');

    const histRows = [];
    for (let n = 10; n <= 19; n++) {
        histRows.push(`<div>${n}: <span id="dbg-chain-${n}">0</span></div>`);
    }

    panel.innerHTML = `
        <div style="font-weight:bold;margin-bottom:6px;">AI debug</div>
        <div>最大連鎖数: <span id="dbg-max-chain">0</span></div>
        <div>経過ターン数: <span id="dbg-turns">0</span></div>
        <div>平均連鎖長: <span id="dbg-avg-chain">0.00</span></div>
        <div style="margin-top:6px;">10～19連鎖回数</div>
        <div style="display:grid;grid-template-columns:repeat(2,1fr);gap:2px 8px;margin-top:4px;">
            ${histRows.join('')}
        </div>
    `;

    document.body.appendChild(panel);
    debugUpdatePanel();
}

function debugUpdatePanel() {
    if (!DEBUG_MODE) return;

    const maxEl = document.getElementById('dbg-max-chain');
    const turnsEl = document.getElementById('dbg-turns');
    const avgEl = document.getElementById('dbg-avg-chain');

    if (maxEl) maxEl.textContent = String(DEBUG_STATS.maxChain);
    if (turnsEl) turnsEl.textContent = String(DEBUG_STATS.turns);

    const avg = DEBUG_STATS.chainEpisodes > 0
        ? (DEBUG_STATS.chainSum / DEBUG_STATS.chainEpisodes)
        : 0;

    if (avgEl) avgEl.textContent = avg.toFixed(2);

    for (let n = 10; n <= 19; n++) {
        const el = document.getElementById(`dbg-chain-${n}`);
        if (el) el.textContent = String(DEBUG_STATS.hist[n]);
    }
}

function debugRecordTurn() {
    if (!DEBUG_MODE) return;
    DEBUG_STATS.turns++;
    debugUpdatePanel();
}

function debugRecordChainEpisode(chainLength) {
    if (!DEBUG_MODE) return;
    if (!Number.isFinite(chainLength) || chainLength <= 0) return;

    DEBUG_STATS.maxChain = Math.max(DEBUG_STATS.maxChain, chainLength);
    DEBUG_STATS.chainSum += chainLength;
    DEBUG_STATS.chainEpisodes++;

    if (chainLength >= 10 && chainLength <= 19) {
        DEBUG_STATS.hist[chainLength]++;
    }

    debugUpdatePanel();
}

// ゲームの状態管理
var board = [];
var currentPuyo = null;
var nextQueue = [];
var queueIndex = 0;

var score = 0;
var chainCount = 0;
var gameState = 'playing'; // 'playing', 'chaining', 'gameover', 'editing', 'setting'
let currentEditColor = COLORS.EMPTY;
let editingNextPuyos = [];
let nextEdited = false;

// おじゃま管理
let pendingOjama = 0; // 受け取って待機中のおじゃま（個数）
let chainAttackScoreBuffer = 0; // 現在の連鎖で発生した攻撃の raw score
let nuisancePointBuffer = 0; // 端数持ち越し

// 履歴スタック（Undo / Redo）
let historyStack = [];
let redoStack = [];

// 落下ループ
let dropInterval = 1000; // ms
let dropTimer = null;
let autoDropEnabled = false;

// 連鎖速度設定
let gravityWaitTime = 300;
let chainWaitTime = 300;

// クイックターン
let lastFailedRotation = { type: null, timestamp: 0 };
const QUICK_TURN_WINDOW = 300; // ms

// 連鎖非同期制御
let chainTimer = null;
let chainAbortFlag = false;

// ---------- ユーティリティ関数 ----------
function copyBoard(srcBoard) {
    return srcBoard.map(row => row.slice());
}

function copyNextQueue(srcQueue) {
    return srcQueue.map(pair => pair.slice());
}

function shuffleArray(arr) {
    for (let i = arr.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [arr[i], arr[j]] = [arr[j], arr[i]];
    }
    return arr;
}

function hasFourUniqueColorsAcrossFirstTwoPairs(batch) {
    if (!batch || batch.length < 2) return false;
    const s = new Set([...batch[0], ...batch[1]]);
    return s.size === 4;
}

// 128組分の次ぷよを、4色64個ずつ均等になるように作る
function buildBalancedNextBatch(batchPairs = NEXT_QUEUE_BALANCE_BATCH_SIZE) {
    const totalPuyos = batchPairs * 2;
    const perColor = totalPuyos / NEXT_QUEUE_COLORS.length;

    const pool = [];
    NEXT_QUEUE_COLORS.forEach(color => {
        for (let i = 0; i < perColor; i++) {
            pool.push(color);
        }
    });

    shuffleArray(pool);

    const batch = [];
    for (let i = 0; i < pool.length; i += 2) {
        batch.push([pool[i], pool[i + 1]]);
    }
    return batch;
}

function appendBalancedNextBatch() {
    let batch = buildBalancedNextBatch();
    let guard = 0;

    while (hasFourUniqueColorsAcrossFirstTwoPairs(batch) && guard < 100) {
        batch = buildBalancedNextBatch();
        guard++;
    }

    nextQueue.push(...batch);
}

// sleep helper that registers chainTimer so it can be cancelled
function sleep(ms) {
    return new Promise(resolve => {
        if (chainTimer) {
            clearTimeout(chainTimer);
            chainTimer = null;
        }
        chainTimer = setTimeout(() => {
            chainTimer = null;
            resolve();
        }, ms);
    });
}

function stopChain() {
    if (chainTimer) {
        clearTimeout(chainTimer);
        chainTimer = null;
    }
    chainAbortFlag = true;
    chainAttackScoreBuffer = 0;
}

function updateOjamaUI() {
    const el = document.getElementById('ojama-count');
    if (el) el.textContent = pendingOjama;
}

// score をおじゃま個数へ換算
function scoreToOjama(scoreValue) {
    return Math.floor(Math.max(0, scoreValue) / NUISANCE_TARGET_POINTS);
}

function flushChainOjamaBuffer() {
    nuisancePointBuffer += chainAttackScoreBuffer / NUISANCE_TARGET_POINTS;
    const attackOjama = Math.floor(nuisancePointBuffer);
    nuisancePointBuffer -= attackOjama;

    const outgoingOjama = consumeOjamaForAttack(attackOjama);
    chainAttackScoreBuffer = 0;

    if (outgoingOjama > 0 && typeof window.sendOjama === 'function') {
        window.sendOjama(outgoingOjama);
    }

    return outgoingOjama;
}

// online.js から受け取る入口
window.addIncomingOjama = function(amount) {
    const n = Math.max(0, Math.floor(Number(amount) || 0));
    pendingOjama += n;
    updateOjamaUI();
};

// 互換用
window.receiveOjama = window.addIncomingOjama;

// おじゃま攻撃分を自分の保留おじゃまで相殺して、残りを返す
function consumeOjamaForAttack(attackOjama) {
    const attack = Math.max(0, Math.floor(Number(attackOjama) || 0));
    const canceled = Math.min(pendingOjama, attack);
    pendingOjama -= canceled;
    updateOjamaUI();
    return attack - canceled;
}

// おじゃまを盤面に積む
function dropOjamaToBoard(count) {
    const amount = Math.max(0, Math.min(MAX_OJAMA_DROP_PER_TURN, Math.floor(Number(count) || 0)));
    if (amount === 0) return true;

    const emptyCells = board.reduce((sum, row) => sum + row.filter(c => c === COLORS.EMPTY).length, 0);
    if (amount > emptyCells) {
        return false;
    }

    const fullRounds = Math.floor(amount / WIDTH);
    const remainder = amount % WIDTH;

    const placeOne = (x) => {
        let stackHeight = 0;
        for (let y = 0; y < HEIGHT; y++) {
            if (board[y][x] !== COLORS.EMPTY) stackHeight++;
        }
        if (stackHeight >= HEIGHT) return false;
        board[stackHeight][x] = COLORS.GARBAGE;
        return true;
    };

    for (let round = 0; round < fullRounds; round++) {
        const cols = Array.from({ length: WIDTH }, (_, i) => i);
        shuffleArray(cols);
        for (const x of cols) {
            if (!placeOne(x)) return false;
        }
    }

    if (remainder > 0) {
        const cols = Array.from({ length: WIDTH }, (_, i) => i);
        shuffleArray(cols);
        for (let i = 0; i < remainder; i++) {
            if (!placeOne(cols[i])) return false;
        }
    }

    gravity();
    return true;
}

function applyPendingOjamaToBoard() {
    if (pendingOjama <= 0) return true;

    const amount = Math.min(MAX_OJAMA_DROP_PER_TURN, pendingOjama);
    pendingOjama -= amount;
    updateOjamaUI();

    const ok = dropOjamaToBoard(amount);
    renderBoard();
    updateUI();

    if (!ok) {
        triggerGameOver();
        return false;
    }
    return true;
}

function triggerGameOver() {
    if (gameState === 'gameover') return;

    gameState = 'gameover';
    document.body.classList.add(BOARD_GAMEOVER_CLASS);
    clearInterval(dropTimer);

    updateUI();
    renderBoard();

    // ゲームオーバー状態も履歴に保存
    saveState(true);

    if (typeof window.notifyGameOver === 'function') {
        window.notifyGameOver();
    } else {
        alert('ゲームオーバーです！');
    }
}

// ---------- NextQueue 管理 ----------
function generateInitialNextQueue() {
    nextQueue = [];
    queueIndex = 0;

    let batch = buildBalancedNextBatch();
    let guard = 0;

    while (hasFourUniqueColorsAcrossFirstTwoPairs(batch) && guard < 100) {
        batch = buildBalancedNextBatch();
        guard++;
    }

    nextQueue.push(...batch);
}

function ensureNextQueueCapacity() {
    const threshold = 40;
    while (nextQueue.length - queueIndex < threshold) {
        appendBalancedNextBatch();
    }
}

function consumeNextPair() {
    if (queueIndex >= nextQueue.length) {
        appendBalancedNextBatch();
    }
    const pair = nextQueue[queueIndex];
    queueIndex++;
    ensureNextQueueCapacity();
    return [pair[0], pair[1]];
}

// ---------- DOM 初期化 / 描画 ----------
function createBoardDOM() {
    const boardElement = document.getElementById('puyo-board');
    if (!boardElement) return;
    boardElement.innerHTML = '';

    for (let y = HEIGHT - 1; y >= 0; y--) {
        for (let x = 0; x < WIDTH; x++) {
            const cell = document.createElement('div');
            cell.id = `cell-${x}-${y}`;
            cell.className = 'puyo-cell';

            const puyo = document.createElement('div');
            puyo.className = 'puyo puyo-0';
            puyo.setAttribute('data-color', 0);

            cell.appendChild(puyo);
            boardElement.appendChild(cell);
        }
    }
}

function renderBoard() {
    const boardElement = document.getElementById('puyo-board');
    if (!boardElement) return;

    boardElement.innerHTML = '';
    for (let y = HEIGHT - 1; y >= 0; y--) {
        for (let x = 0; x < WIDTH; x++) {
            const cell = document.createElement('div');
            cell.id = `cell-${x}-${y}`;
            cell.className = 'puyo-cell';

            const puyo = document.createElement('div');
            puyo.className = 'puyo puyo-0';
            puyo.setAttribute('data-color', 0);

            cell.appendChild(puyo);
            boardElement.appendChild(cell);
        }
    }

    for (let y = 0; y < HEIGHT; y++) {
        for (let x = 0; x < WIDTH; x++) {
            const cellElement = document.getElementById('cell-' + x + '-' + y);
            if (!cellElement) continue;
            const puyoElement = cellElement.firstChild;
            if (!puyoElement) continue;
            const color = board[y][x];
            puyoElement.className = 'puyo puyo-' + color;
            puyoElement.setAttribute('data-color', color);
        }
    }

    if (currentPuyo && gameState === 'playing') {
        renderCurrentPuyo();
    }

    if (gameState === 'playing') {
        renderPlayNextPuyo();
    } else if (gameState === 'editing') {
        renderEditNextPuyos();
    }
}

function renderCurrentPuyo() {
    if (!currentPuyo) return;
    const currentCoords = getPuyoCoords();
    const ghostCoords = getGhostFinalPositions();

    for (let y = 0; y < HEIGHT; y++) {
        for (let x = 0; x < WIDTH; x++) {
            const cell = document.getElementById(`cell-${x}-${y}`);
            if (!cell) continue;
            const puyo = cell.firstChild;
            if (!puyo) continue;

            let cellColor = board[y][x];
            let puyoClasses = 'puyo puyo-' + cellColor;

            const inFlight = currentCoords.find(p => p.x === x && p.y === y);
            if (inFlight) {
                cellColor = inFlight.color;
                puyoClasses = 'puyo puyo-' + cellColor;
            } else {
                const ghost = ghostCoords.find(p => p.x === x && p.y === y);
                if (ghost) {
                    cellColor = ghost.color;
                    puyoClasses = 'puyo puyo-' + cellColor + ' puyo-ghost';
                }
            }

            puyo.className = puyoClasses;
            puyo.setAttribute('data-color', cellColor);
        }
    }
}

function renderPlayNextPuyo() {
    const next1Element = document.getElementById('next-puyo-1');
    const next2Element = document.getElementById('next-puyo-2');
    if (!next1Element || !next2Element) return;

    const createPuyo = (color) => {
        const el = document.createElement('div');
        el.className = 'puyo puyo-' + color;
        return el;
    };

    const pairs = [
        nextQueue[queueIndex] || [COLORS.EMPTY, COLORS.EMPTY],
        nextQueue[queueIndex + 1] || [COLORS.EMPTY, COLORS.EMPTY]
    ];

    [next1Element, next2Element].forEach((slot, idx) => {
        slot.innerHTML = '';
        const pair = pairs[idx];
        if (pair) {
            slot.appendChild(createPuyo(pair[1]));
            slot.appendChild(createPuyo(pair[0]));
        }
    });
}

function updateUI() {
    const scoreElement = document.getElementById('score');
    const chainElement = document.getElementById('chain-count');
    if (scoreElement) scoreElement.textContent = score;
    if (chainElement) chainElement.textContent = chainCount;
    updateOjamaUI();
    updateHistoryButtons();
}

// ---------- ステージコード化 / 復元 ----------
window.copyStageCode = function() {
    if (gameState !== 'editing') {
        alert("ステージコード化はエディットモードでのみ実行できます。");
        return;
    }

    let dataArray = [];
    for (let y = 0; y < HEIGHT; y++) {
        for (let x = 0; x < WIDTH; x++) {
            dataArray.push(board[y][x]);
        }
    }
    editingNextPuyos.forEach(pair => {
        dataArray.push(pair[0]); // sub
        dataArray.push(pair[1]); // main
    });

    let binaryString = "";
    dataArray.forEach(color => {
        binaryString += color.toString(2).padStart(3, '0');
    });

    let byteString = "";
    for (let i = 0; i < binaryString.length; i += 8) {
        const byte = binaryString.substring(i, i + 8).padEnd(8, '0');
        byteString += String.fromCharCode(parseInt(byte, 2));
    }

    const stageCode = btoa(byteString);
    const codeInput = document.getElementById('stage-code-input');
    if (codeInput) {
        codeInput.value = stageCode;
        codeInput.select();
        document.execCommand('copy');
        alert('現在のステージコードをクリップボードにコピーしました！');
    }
};

window.loadStageCode = function() {
    if (gameState !== 'editing') {
        alert("ステージコードの読み込みはエディットモードでのみ実行できます。");
        return;
    }
    const codeInput = document.getElementById('stage-code-input');
    const stageCode = codeInput ? codeInput.value.trim() : "";
    if (!stageCode) {
        alert("ステージコードが入力されていません。");
        return;
    }

    try {
        const byteString = atob(stageCode);
        let binaryString = "";
        for (let i = 0; i < byteString.length; i++) {
            binaryString += byteString.charCodeAt(i).toString(2).padStart(8, '0');
        }

        let dataArray = [];
        for (let i = 0; i < binaryString.length; i += 3) {
            const colorBinary = binaryString.substring(i, i + 3);
            if (colorBinary.length === 3) {
                dataArray.push(parseInt(colorBinary, 2));
            }
        }

        const required = HEIGHT * WIDTH + MAX_NEXT_PUYOS * 2;
        if (dataArray.length < required) {
            throw new Error("データが不足しています。");
        }

        let idx = 0;
        for (let y = 0; y < HEIGHT; y++) {
            board[y] = board[y] || Array(WIDTH).fill(COLORS.EMPTY);
            for (let x = 0; x < WIDTH; x++) {
                board[y][x] = dataArray[idx++];
            }
        }

        editingNextPuyos = [];
        for (let i = 0; i < MAX_NEXT_PUYOS; i++) {
            const subColor = dataArray[idx++];
            const mainColor = dataArray[idx++];
            editingNextPuyos.push([subColor, mainColor]);
        }

        renderBoard();
        renderEditNextPuyos();
        alert('ステージコードを正常に読み込みました。');
    } catch (e) {
        console.error("ステージコードの復元中にエラー:", e);
        alert('ステージコードが無効です。形式を確認してください。');
    }
};

// ---------- 履歴（Undo / Redo） ----------
function saveState(clearRedoStack = true) {
    const state = {
        board: copyBoard(board),
        nextQueue: copyNextQueue(nextQueue),
        queueIndex: queueIndex,
        score: score,
        chainCount: chainCount,
        pendingOjama: pendingOjama,
        nuisancePointBuffer: nuisancePointBuffer,
        gameState: gameState,
        currentPuyo: currentPuyo ? {
            mainColor: currentPuyo.mainColor,
            subColor: currentPuyo.subColor,
            mainX: currentPuyo.mainX,
            mainY: currentPuyo.mainY,
            rotation: currentPuyo.rotation
        } : null
    };

    historyStack.push(state);

    while (historyStack.length > MAX_HISTORY_SIZE) {
        historyStack.shift();
    }

    if (clearRedoStack) redoStack = [];
    updateHistoryButtons();
}

function restoreState(state) {
    if (!state) return;

    stopChain();

    board = copyBoard(state.board);
    nextQueue = copyNextQueue(state.nextQueue);
    queueIndex = state.queueIndex;
    score = state.score;
    chainCount = state.chainCount;
    pendingOjama = state.pendingOjama || 0;
    nuisancePointBuffer = state.nuisancePointBuffer || 0;
    updateOjamaUI();

    if (state.currentPuyo) {
        currentPuyo = { ...state.currentPuyo };
    } else {
        currentPuyo = null;
    }

    gameState = state.gameState || 'playing';
    document.body.classList.toggle(BOARD_GAMEOVER_CLASS, gameState === 'gameover');

    clearInterval(dropTimer);

    if (gameState === 'gameover') {
        updateUI();
        renderBoard();
        return;
    }

    gravity();

    const groups = findConnectedPuyos();
    if (groups.length > 0) {
        gameState = 'chaining';
        chainCount = 0;
        chainAttackScoreBuffer = 0;
        runChain();
    } else {
        startPuyoDropLoop();
    }

    updateUI();
    renderBoard();
}

window.undoMove = function() {
    if (gameState !== 'playing' && gameState !== 'chaining' && gameState !== 'gameover') return;
    if (historyStack.length <= 1) return;

    stopChain();

    const currentState = historyStack.pop();
    redoStack.push(currentState);
    const previousState = historyStack[historyStack.length - 1];
    restoreState(previousState);
    updateHistoryButtons();
};

window.redoMove = function() {
    if (gameState !== 'playing' && gameState !== 'chaining' && gameState !== 'gameover') return;
    if (redoStack.length === 0) return;

    stopChain();

    const nextState = redoStack.pop();
    historyStack.push(nextState);
    restoreState(nextState);
    updateHistoryButtons();
};

function updateHistoryButtons() {
    const undoButton = document.getElementById('undo-button');
    const redoButton = document.getElementById('redo-button');
    if (undoButton) undoButton.disabled = historyStack.length <= 1;
    if (redoButton) redoButton.disabled = redoStack.length === 0;
}

// ---------- ゲームループ / 落下 ----------
function startPuyoDropLoop() {
    if (dropTimer) clearInterval(dropTimer);
    if (gameState === 'playing' && autoDropEnabled) {
        dropTimer = setInterval(dropPuyo, dropInterval);
    }
}

function dropPuyo() {
    if (gameState !== 'playing' || !currentPuyo) return;
    const moved = movePuyo(0, -1, undefined, true);
    if (!moved) {
        clearInterval(dropTimer);
        lockPuyo();
    }
}

// ---------- エディットモード ----------
function setupEditModeListeners() {
    const palette = document.getElementById('color-palette');
    if (palette) {
        palette.querySelectorAll('.palette-color').forEach(p => {
            p.addEventListener('click', () => {
                const color = parseInt(p.getAttribute('data-color'), 10);
                selectPaletteColor(color);
            });
        });
    }
}

function selectPaletteColor(color) {
    currentEditColor = color;
    document.querySelectorAll('.palette-color').forEach(el => el.classList.remove('selected'));
    const selectedPuyo = document.querySelector(`.palette-color[data-color="${color}"]`);
    if (selectedPuyo) selectedPuyo.classList.add('selected');
}

function handleBoardClickEditMode(event) {
    if (gameState !== 'editing') return;
    const boardElement = document.getElementById('puyo-board');
    if (!boardElement) return;
    const rect = boardElement.getBoundingClientRect();
    const cellSize = rect.width / WIDTH;
    let x = Math.floor((event.clientX - rect.left) / cellSize);
    let y_dom = Math.floor((event.clientY - rect.top) / cellSize);
    let y = HEIGHT - 1 - y_dom;
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        board[y][x] = currentEditColor;
        renderBoard();
    }
}

window.applyNextPuyos = function() {
    if (gameState === 'editing') {
        nextQueue = copyNextQueue(editingNextPuyos.slice(0, Math.max(editingNextPuyos.length, 1)));
        nextEdited = true;
        queueIndex = 0;
        ensureNextQueueCapacity();
        alert('ネクストぷよの設定を保存しました。プレイモードで適用されます。');
    }
};

window.clearEditNext = function() {
    if (gameState !== 'editing') return;
    editingNextPuyos = [];
    editingNextPuyos.push(getRandomPair());
    for (let i = 1; i < MAX_NEXT_PUYOS; i++) {
        let newPair, retries = 0;
        const MAX_RETRIES = 100;
        do {
            newPair = getRandomPair();
            retries++;
            if (retries > MAX_RETRIES) {
                console.warn("clearEditNext: Max retries reached.");
                break;
            }
        } while (hasFourUniqueColorsAcrossFirstTwoPairs([editingNextPuyos[i - 1], newPair]));
        editingNextPuyos.push(newPair);
    }
    renderEditNextPuyos();
    alert('ネクストぷよリストをランダムで再生成しました。');
};

// ---------- ぷよ生成 / 座標系 ----------
function getRandomColor() {
    return Math.floor(Math.random() * 4) + 1;
}

function getRandomPair() {
    return [getRandomColor(), getRandomColor()];
}

let _initializedOnce = false;

function initializeGame() {
    createBoardDOM();
    for (let y = 0; y < HEIGHT; y++) board[y] = Array(WIDTH).fill(COLORS.EMPTY);

    score = 0;
    chainCount = 0;
    pendingOjama = 0;
    chainAttackScoreBuffer = 0;
    nuisancePointBuffer = 0;
    updateOjamaUI();

    gameState = 'playing';
    document.body.classList.remove(BOARD_GAMEOVER_CLASS);

    generateInitialNextQueue();
    editingNextPuyos = copyNextQueue(nextQueue.slice(0, MAX_NEXT_PUYOS));
    currentEditColor = COLORS.EMPTY;

    const modeToggleButton = document.querySelector('.mode-toggle-btn');
    if (modeToggleButton) modeToggleButton.textContent = 'edit';
    const infoPanel = document.getElementById('info-panel');
    if (infoPanel) infoPanel.classList.remove('edit-mode-active');

    const autoDropButton = document.getElementById('auto-drop-toggle-button');
    if (autoDropButton) {
        if (autoDropEnabled) {
            autoDropButton.textContent = '自動落下: ON';
            autoDropButton.classList.remove('disabled');
        } else {
            autoDropButton.textContent = '自動落下: OFF';
            autoDropButton.classList.add('disabled');
        }
    }

    currentPuyo = null;
    ensureNextQueueCapacity();
    generateNewPuyo();

    startPuyoDropLoop();
    updateUI();

    if (!document.initializedKeyHandler) {
        document.addEventListener('keydown', handleInput);
        document.addEventListener('keydown', (event) => {
            const key = event.key.toLowerCase();
            if (key === 'u') { event.preventDefault(); undoMove(); }
            else if (key === 'y') { event.preventDefault(); redoMove(); }
            else if (key === 'r') { event.preventDefault(); resetGame(); }
            else if (key === 'e') { event.preventDefault(); toggleMode(); }
        });

        const btnLeft = document.getElementById('btn-left');
        const btnRight = document.getElementById('btn-right');
        const btnRotateCW = document.getElementById('btn-rotate-cw');
        const btnRotateCCW = document.getElementById('btn-rotate-ccw');
        const btnHardDrop = document.getElementById('btn-hard-drop');
        const btnSoftDrop = document.getElementById('btn-soft-drop');

        if (btnLeft) btnLeft.addEventListener('click', () => movePuyo(-1, 0));
        if (btnRight) btnRight.addEventListener('click', () => movePuyo(1, 0));
        if (btnRotateCW) btnRotateCW.addEventListener('click', window.rotatePuyoCW);
        if (btnRotateCCW) btnRotateCCW.addEventListener('click', window.rotatePuyoCCW);
        if (btnHardDrop) btnHardDrop.addEventListener('click', hardDrop);
        if (btnSoftDrop) btnSoftDrop.addEventListener('click', () => {
            if (gameState === 'playing') {
                clearInterval(dropTimer);
                movePuyo(0, -1);
                if (autoDropEnabled) startPuyoDropLoop();
            }
        });

        setupEditModeListeners();
        document.initializedKeyHandler = true;
    }

    checkMobileControlsVisibility();
    renderBoard();
    
    debugEnsurePanel();

    if (!_initializedOnce) {
        saveState(false);
        _initializedOnce = true;
    }
}

function generateNewPuyo() {
    if (gameState !== 'playing') return;

    // 受け取っていたおじゃまがあるなら、次の操作ぷよ生成前に落とす
    if (pendingOjama > 0) {
        if (!applyPendingOjamaToBoard()) {
            return;
        }
    }

    ensureNextQueueCapacity();
    const [sub, main] = consumeNextPair();

    currentPuyo = {
        mainColor: main,
        subColor: sub,
        mainX: 2,
        mainY: HEIGHT - 2,
        rotation: 0
    };

    const startingCoords = getCoordsFromState(currentPuyo);
    const isOverlappingTarget = startingCoords.some(
        p => p.x === 2 && p.y === (HEIGHT - 3) && board[p.y][p.x] !== COLORS.EMPTY
    );

    if (checkCollision(startingCoords) || isOverlappingTarget) {
        triggerGameOver();
        return;
    }
}

function getCoordsFromState(puyoState) {
    const { mainX, mainY, rotation } = puyoState;
    let subX = mainX;
    let subY = mainY;
    if (rotation === 0) subY = mainY + 1;
    else if (rotation === 1) subX = mainX - 1;
    else if (rotation === 2) subY = mainY - 1;
    else if (rotation === 3) subX = mainX + 1;
    return [{ x: mainX, y: mainY }, { x: subX, y: subY }];
}

function getPuyoCoords() {
    if (!currentPuyo) return [];
    const coords = getCoordsFromState(currentPuyo);
    coords[0].color = currentPuyo.mainColor;
    coords[1].color = currentPuyo.subColor;
    return coords;
}

// ゴースト計算（隠し行は最終的にフィルタで除外）
function getGhostFinalPositions() {
    if (!currentPuyo || gameState !== 'playing') return [];
    let tempBoard = board.map(row => [...row]);
    let tempPuyo = { ...currentPuyo };

    while (true) {
        let testPuyo = { ...tempPuyo, mainY: tempPuyo.mainY - 1 };
        const testCoords = getCoordsFromState(testPuyo);
        if (checkCollision(testCoords)) break;
        tempPuyo.mainY -= 1;
    }

    const finalCoordsBeforeGravity = getCoordsFromState(tempPuyo);
    const puyoColors = [tempPuyo.mainColor, tempPuyo.subColor];

    finalCoordsBeforeGravity.forEach(p => {
        if (p.y >= 0 && p.y < HEIGHT) {
            const color = (p.x === tempPuyo.mainX && p.y === tempPuyo.mainY) ? tempPuyo.mainColor : tempPuyo.subColor;
            tempBoard[p.y][p.x] = color;
        }
    });

    simulateGravity(tempBoard);

    let ghostPositions = [];
    let puyoCount = 0;
    for (let y = 0; y < HEIGHT; y++) {
        for (let x = 0; x < WIDTH; x++) {
            const tempColor = tempBoard[y][x];
            const originalColor = board[y][x];
            if (originalColor === COLORS.EMPTY && puyoColors.includes(tempColor) && puyoCount < 2) {
                ghostPositions.push({ x, y, color: tempColor });
                puyoCount++;
            }
        }
    }

    return ghostPositions.filter(p => p.y < HEIGHT - HIDDEN_ROWS);
}

// 衝突判定
function checkCollision(coords) {
    for (const puyo of coords) {
        if (puyo.x < 0 || puyo.x >= WIDTH || puyo.y < 0 || puyo.y > HEIGHT) return true;
        if (puyo.y < HEIGHT && board[puyo.y][puyo.x] !== COLORS.EMPTY) return true;
    }
    return false;
}

// 移動（成功なら true）
function movePuyo(dx, dy, newRotation, shouldRender = true) {
    if (gameState !== 'playing' || !currentPuyo) return false;
    const { mainX, mainY, rotation } = currentPuyo;
    const test = {
        mainX: mainX + dx,
        mainY: mainY + dy,
        rotation: newRotation !== undefined ? newRotation : rotation
    };
    const testCoords = getCoordsFromState(test);
    if (!checkCollision(testCoords)) {
        currentPuyo.mainX = test.mainX;
        currentPuyo.mainY = test.mainY;
        if (newRotation !== undefined) currentPuyo.rotation = newRotation;
        if (shouldRender) renderBoard();
        return true;
    }
    return false;
}

// 回転（CW / CCW） — 簡易ウォールキック + クイックターン
window.rotatePuyoCW = function() {
    if (gameState !== 'playing' || !currentPuyo) return false;
    if (autoDropEnabled && dropTimer) { clearInterval(dropTimer); startPuyoDropLoop(); }

    const newRotation = (currentPuyo.rotation + 1) % 4;
    const oldRotation = currentPuyo.rotation;
    let rotationSuccess = movePuyo(0, 0, newRotation);
    if (!rotationSuccess) {
        if (oldRotation === 0 || oldRotation === 2) {
            if (newRotation === 1) {
                rotationSuccess = movePuyo(1, 0, newRotation) || movePuyo(0, 1, newRotation);
            } else if (newRotation === 3) {
                rotationSuccess = movePuyo(-1, 0, newRotation) || movePuyo(0, 1, newRotation);
            }
        } else {
            rotationSuccess = movePuyo(0, 1, newRotation);
        }
    }

    if (rotationSuccess) {
        lastFailedRotation.type = null;
        return true;
    }

    const now = Date.now();
    if (lastFailedRotation.type === 'CW' && (now - lastFailedRotation.timestamp) < QUICK_TURN_WINDOW) {
        [currentPuyo.mainColor, currentPuyo.subColor] = [currentPuyo.subColor, currentPuyo.mainColor];
        lastFailedRotation.type = null;
        renderBoard();
        return true;
    }

    lastFailedRotation.type = 'CW';
    lastFailedRotation.timestamp = now;
    return false;
};

window.rotatePuyoCCW = function() {
    if (gameState !== 'playing' || !currentPuyo) return false;
    if (autoDropEnabled && dropTimer) { clearInterval(dropTimer); startPuyoDropLoop(); }

    const newRotation = (currentPuyo.rotation - 1 + 4) % 4;
    const oldRotation = currentPuyo.rotation;
    let rotationSuccess = movePuyo(0, 0, newRotation);

    if (!rotationSuccess) {
        if (oldRotation === 0 || oldRotation === 2) {
            if (newRotation === 1) {
                rotationSuccess = movePuyo(1, 0, newRotation) || movePuyo(0, 1, newRotation);
            } else if (newRotation === 3) {
                rotationSuccess = movePuyo(-1, 0, newRotation) || movePuyo(0, 1, newRotation);
            }
        } else {
            rotationSuccess = movePuyo(0, 1, newRotation);
        }
    }

    if (rotationSuccess) {
        lastFailedRotation.type = null;
        return true;
    }

    const now = Date.now();
    if (lastFailedRotation.type === 'CCW' && (now - lastFailedRotation.timestamp) < QUICK_TURN_WINDOW) {
        [currentPuyo.mainColor, currentPuyo.subColor] = [currentPuyo.subColor, currentPuyo.mainColor];
        lastFailedRotation.type = null;
        renderBoard();
        return true;
    }

    lastFailedRotation.type = 'CCW';
    lastFailedRotation.timestamp = now;
    return false;
};

// ハードドロップ（即固定）
function hardDrop() {
    if (gameState !== 'playing' || !currentPuyo) return;
    clearInterval(dropTimer);
    while (movePuyo(0, -1, undefined, false)) { }
    renderBoard();
    lockPuyo();
}

// lockPuyo: 設置 -> gravity -> 上端行クリア -> 連鎖開始
function lockPuyo() {
    if (gameState !== 'playing' || !currentPuyo) return;
    debugRecordTurn();
    const coords = getPuyoCoords();
    coords.forEach(p => {
        if (p.y >= 0 && p.y < HEIGHT && p.x >= 0 && p.x < WIDTH) {
            board[p.y][p.x] = p.color;
        }
    });

    currentPuyo = null;

    gravity();

    for (let x = 0; x < WIDTH; x++) {
        board[HEIGHT - 1][x] = COLORS.EMPTY;
    }

    renderBoard();
    updateUI();

    gameState = 'chaining';
    chainCount = 0;
    chainAttackScoreBuffer = 0;
    runChain();
}

// ---------- 連結検出（上部 HIDDEN_ROWS を除外） ----------
function findConnectedPuyos() {
    const MAX_SEARCH_Y = HEIGHT - HIDDEN_ROWS;
    let visited = Array(HEIGHT).fill(0).map(() => Array(WIDTH).fill(false));
    let groups = [];

    for (let y = 0; y < MAX_SEARCH_Y; y++) {
        for (let x = 0; x < WIDTH; x++) {
            const color = board[y][x];
            if (color === COLORS.EMPTY || color === COLORS.GARBAGE || visited[y][x]) continue;

            let stack = [{ x, y }];
            visited[y][x] = true;
            let group = [];

            while (stack.length > 0) {
                const cur = stack.pop();
                group.push(cur);

                [[0,1],[0,-1],[1,0],[-1,0]].forEach(([dx,dy]) => {
                    const nx = cur.x + dx;
                    const ny = cur.y + dy;
                    if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < MAX_SEARCH_Y &&
                        !visited[ny][nx] && board[ny][nx] === color) {
                        visited[ny][nx] = true;
                        stack.push({ x: nx, y: ny });
                    }
                });
            }

            if (group.length >= 4) groups.push({ group, color });
        }
    }

    return groups;
}

// おじゃま消去（消した色に隣接するゴミを消す）
function clearGarbagePuyos(erasedCoords) {
    let clearedCount = 0;
    const garbageToClear = new Set();
    erasedCoords.forEach(({ x, y }) => {
        [[0,1],[0,-1],[1,0],[-1,0]].forEach(([dx,dy]) => {
            const nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                if (board[ny][nx] === COLORS.GARBAGE) {
                    garbageToClear.add(`${nx}-${ny}`);
                }
            }
        });
    });

    garbageToClear.forEach(k => {
        const [nx, ny] = k.split('-').map(Number);
        board[ny][nx] = COLORS.EMPTY;
        clearedCount++;
    });
    return clearedCount;
}

// スコア計算
function calculateScore(groups, currentChain) {
    let totalPuyos = 0;
    let colorSet = new Set();
    let bonusTotal = 0;

    groups.forEach(({ group, color }) => {
        totalPuyos += group.length;
        colorSet.add(color);

        const idx = Math.min(group.length, BONUS_TABLE.GROUP.length - 1);
        bonusTotal += BONUS_TABLE.GROUP[idx];
    });

    // 1連鎖目は CHAIN[0] を使う
    const chainIdx = Math.max(0, Math.min(currentChain - 1, BONUS_TABLE.CHAIN.length - 1));
    bonusTotal += BONUS_TABLE.CHAIN[chainIdx];

    const colorIdx = Math.min(colorSet.size, BONUS_TABLE.COLOR.length - 1);
    bonusTotal += BONUS_TABLE.COLOR[colorIdx];

    // A+B+C が 0 のときは 1 にする
    const bonusMultiplier = (bonusTotal === 0) ? 1 : bonusTotal;

    // X = 10 × 消したぷよの数
    // score = X × (A + B + C)
    return (10 * totalPuyos) * bonusMultiplier;
}

// 連鎖処理（async）
async function runChain() {
    chainAbortFlag = false;

    gravity();
    renderBoard();

    const groups = findConnectedPuyos();

    if (groups.length === 0) {
        debugRecordChainEpisode(chainCount);
        if (checkBoardEmpty()) {
            score += ALL_CLEAR_SCORE_BONUS;
            chainAttackScoreBuffer += ALL_CLEAR_SCORE_BONUS;
            updateUI();
        }

        const gameOverLineY = HEIGHT - 3;
        const checkX = 2;
        const isGameOver = board[gameOverLineY][checkX] !== COLORS.EMPTY;
        if (isGameOver) {
            triggerGameOver();
            return;
        }

        flushChainOjamaBuffer();

        if (!applyPendingOjamaToBoard()) {
            return;
        }

        gameState = 'playing';

        if (!currentPuyo) {
            ensureNextQueueCapacity();
            generateNewPuyo();
        }

        startPuyoDropLoop();
        checkMobileControlsVisibility();
        renderBoard();

        saveState(true);
        return;
    }

    await sleep(chainWaitTime);
    if (chainAbortFlag) return;

    chainCount++;
    const chainScore = calculateScore(groups, chainCount);
    score += chainScore;

    chainAttackScoreBuffer += chainScore;

    let erasedCoords = [];
    groups.forEach(({ group }) => {
        group.forEach(({ x, y }) => {
            board[y][x] = COLORS.EMPTY;
            erasedCoords.push({ x, y });
        });
    });

    clearGarbagePuyos(erasedCoords);
    renderBoard();
    updateUI();

    await sleep(gravityWaitTime);
    if (chainAbortFlag) return;

    gravity();
    renderBoard();

    const nextGroups = findConnectedPuyos();
    if (nextGroups.length === 0) {
        debugRecordChainEpisode(chainCount);
        gameState = 'playing';

        flushChainOjamaBuffer();

        if (!applyPendingOjamaToBoard()) {
            return;
        }

        if (!currentPuyo) {
            ensureNextQueueCapacity();
            generateNewPuyo();
        }

        startPuyoDropLoop();
        checkMobileControlsVisibility();
        renderBoard();

        saveState(true);
    } else {
        await runChain();
    }
}

// ---------- 重力 ----------
function simulateGravity(targetBoard) {
    for (let x = 0; x < WIDTH; x++) {
        let newCol = [];
        for (let y = 0; y < HEIGHT; y++) {
            if (targetBoard[y][x] !== COLORS.EMPTY) newCol.push(targetBoard[y][x]);
        }
        for (let y = 0; y < HEIGHT; y++) {
            targetBoard[y][x] = y < newCol.length ? newCol[y] : COLORS.EMPTY;
        }
    }
}

function gravity() {
    simulateGravity(board);
}

// 盤面が空かチェック
function checkBoardEmpty() {
    for (let y = 0; y < HEIGHT; y++) {
        for (let x = 0; x < WIDTH; x++) {
            if (board[y][x] !== COLORS.EMPTY) return false;
        }
    }
    return true;
}

// ---------- 入力処理 ----------
function handleInput(event) {
    if (gameState !== 'playing') return;
    switch (event.key) {
        case 'ArrowLeft':
            movePuyo(-1, 0);
            break;
        case 'ArrowRight':
            movePuyo(1, 0);
            break;
        case 'z':
        case 'Z':
            window.rotatePuyoCW();
            break;
        case 'x':
        case 'X':
            window.rotatePuyoCCW();
            break;
        case 'ArrowDown':
            clearInterval(dropTimer);
            movePuyo(0, -1);
            if (autoDropEnabled) startPuyoDropLoop();
            break;
        case ' ':
            event.preventDefault();
            hardDrop();
            break;
    }
}

// ---------- エディット用 NEXT 表示 ----------
function renderEditNextPuyos() {
    const listContainer = document.getElementById('edit-next-list-container');
    const visibleSlots = [document.getElementById('edit-next-1'), document.getElementById('edit-next-2')];
    if (!listContainer || !visibleSlots[0] || !visibleSlots[1]) return;

    const createEditablePuyo = (color, listIndex, puyoIndex) => {
        let puyo = document.createElement('div');
        puyo.className = `puyo puyo-${color}`;
        puyo.addEventListener('pointerdown', (ev) => {
            ev.stopPropagation();
            if (gameState !== 'editing') return;
            if (editingNextPuyos.length > listIndex) {
                editingNextPuyos[listIndex][puyoIndex] = currentEditColor;
                nextEdited = true;
                renderEditNextPuyos();
            }
        });
        return puyo;
    };

    visibleSlots.forEach((slot, idx) => {
        slot.innerHTML = '';

        if (editingNextPuyos.length > idx) {
            const [c_main, c_sub] = editingNextPuyos[idx];

            slot.appendChild(createEditablePuyo(c_sub, idx, 1)); // 上
            slot.appendChild(createEditablePuyo(c_main, idx, 0)); // 下
        }
    });

    listContainer.innerHTML = '';
    for (let i = NUM_VISIBLE_NEXT_PUYOS; i < MAX_NEXT_PUYOS; i++) {
        if (editingNextPuyos.length <= i) break;
        const pairContainer = document.createElement('div');
        pairContainer.className = 'next-puyo-slot-pair';
        const countSpan = document.createElement('span');
        countSpan.textContent = `N${i + 1}`;
        pairContainer.appendChild(countSpan);
        const puyoRow = document.createElement('div');
        puyoRow.className = 'next-puyo-row';
        const [c_main, c_sub] = editingNextPuyos[i];
        puyoRow.appendChild(createEditablePuyo(c_sub, i, 1));
        puyoRow.appendChild(createEditablePuyo(c_main, i, 0));
        pairContainer.appendChild(puyoRow);
        listContainer.appendChild(pairContainer);
    }
}

// ---------- モバイル表示 / モード切替 ----------
function checkMobileControlsVisibility() {
    const mobileControls = document.getElementById('mobile-controls');
    if (!mobileControls) return;
    if ((gameState === 'playing' || gameState === 'gameover') && window.innerWidth <= 650) {
        mobileControls.classList.add('visible');
        document.body.classList.remove('edit-mode-active');
    } else if (gameState === 'editing') {
        mobileControls.classList.remove('visible');
        document.body.classList.add('edit-mode-active');
    } else {
        mobileControls.classList.remove('visible');
        document.body.classList.remove('edit-mode-active');
    }
}

let previousGameState = 'playing';

window.toggleSettingMode = function() {
    const overlay = document.getElementById('setting-overlay');
    if (!overlay) return;
    if (gameState !== 'setting') {
        previousGameState = gameState;
        gameState = 'setting';
        overlay.style.display = 'flex';
    } else {
        gameState = previousGameState;
        overlay.style.display = 'none';
    }
    checkMobileControlsVisibility();
};

window.toggleMode = function() {
    const infoPanel = document.getElementById('info-panel');
    const modeToggleButton = document.querySelector('.mode-toggle-btn');
    const boardElement = document.getElementById('puyo-board');

    if (gameState === 'playing' || gameState === 'gameover') {
        clearInterval(dropTimer);
        gameState = 'editing';
        document.body.classList.remove(BOARD_GAMEOVER_CLASS);
        if (infoPanel) infoPanel.classList.add('edit-mode-active');
        document.body.classList.add('edit-mode-active');
        if (modeToggleButton) modeToggleButton.textContent = 'play';
        checkMobileControlsVisibility();
        if (boardElement) boardElement.addEventListener('click', handleBoardClickEditMode);
        selectPaletteColor(COLORS.EMPTY);
        renderEditNextPuyos();
        renderBoard();
    } else if (gameState === 'editing') {
        gameState = 'playing';
        document.body.classList.remove(BOARD_GAMEOVER_CLASS);
        if (infoPanel) infoPanel.classList.remove('edit-mode-active');
        document.body.classList.remove('edit-mode-active');
        if (modeToggleButton) modeToggleButton.textContent = 'edit';
        checkMobileControlsVisibility();
        if (boardElement) boardElement.removeEventListener('click', handleBoardClickEditMode);

        if (nextEdited) {
            currentPuyo = null;
            ensureNextQueueCapacity();
            generateNewPuyo();
            nextEdited = false;
        }

        if (autoDropEnabled) startPuyoDropLoop();
        renderBoard();
    }
};

// 速度設定 UI
window.updateGravityWait = function(value) {
    gravityWaitTime = parseInt(value);
    const display = document.getElementById('gravity-wait-value');
    if (display) display.textContent = gravityWaitTime + 'ms';
};

window.updateChainWait = function(value) {
    chainWaitTime = parseInt(value);
    const display = document.getElementById('chain-wait-value');
    if (display) display.textContent = chainWaitTime + 'ms';
};

// 自動落下の切替
window.toggleAutoDrop = function() {
    const button = document.getElementById('auto-drop-toggle-button');
    if (!button) return;
    autoDropEnabled = !autoDropEnabled;
    if (autoDropEnabled) {
        button.textContent = '自動落下: ON';
        button.classList.remove('disabled');
        if (gameState === 'playing') startPuyoDropLoop();
    } else {
        button.textContent = '自動落下: OFF';
        button.classList.add('disabled');
        if (dropTimer) clearInterval(dropTimer);
    }
};

// リセット
window.resetGame = function() {
    clearInterval(dropTimer);
    initializeGame();
};

// 連戦用の公開フック
window.prepareForRematch = function() {
    window.resetGame();
};
window.requestRematch = window.prepareForRematch;
window.resetForRematch = window.prepareForRematch;

// ---------- ユーティリティ / その他 ----------
function getDropY(x, startY = 0) {
    if (x < 0 || x >= WIDTH) return -1;
    let y = Math.max(0, startY);
    while (y < HEIGHT && board[y][x] !== COLORS.EMPTY) y++;
    return y < HEIGHT ? y : HEIGHT - 1;
}

// raisePuyoOneRow（デバッグ用）: 境界チェックを HIDDEN_ROWS で統一
(function() {
    'use strict';
    try {
        window.raisePuyoOneRow = function() {
            try {
                if (typeof gameState === 'undefined') { alert('エラー: gameState が取得できません'); return; }
                if (gameState !== 'playing') { alert('プレイ中のみ使用できます。'); return; }
                if (typeof currentPuyo === 'undefined' || !currentPuyo) { alert('操作中のぷよがありません。'); return; }

                const mainX = currentPuyo.mainX;
                const mainY = currentPuyo.mainY;
                const rotation = currentPuyo.rotation;
                let subX = mainX;
                let subY = mainY;
                if (rotation === 0) subY = mainY + 1;
                else if (rotation === 1) subX = mainX - 1;
                else if (rotation === 2) subY = mainY - 1;
                else if (rotation === 3) subX = mainX + 1;

                const newMainY = mainY + 1;
                const newSubY = subY + 1;

                // main は 13 まで、sub は 14 まで許可
                if (newMainY >= HEIGHT || newSubY > HEIGHT) {
                    alert('これ以上上に移動できません。');
                    return;
                }

                let canMove = true;

                // board の占有判定は可視領域（y < HEIGHT - HIDDEN_ROWS）に対して実施
                if (newMainY < HEIGHT - HIDDEN_ROWS) {
                    if (board[newMainY][mainX] !== COLORS.EMPTY) canMove = false;
                }
                if (newSubY < HEIGHT && newSubY < HEIGHT - HIDDEN_ROWS) {
                    if (board[newSubY][subX] !== COLORS.EMPTY) canMove = false;
                }

                if (!canMove) { alert('移動先にぷよがあるため、上に移動できません。'); return; }

                currentPuyo.mainY = newMainY;
                if (typeof renderBoard === 'function') renderBoard();
            } catch (e) {
                console.error('raisePuyoOneRow error:', e);
                alert('エラーが発生しました: ' + e.message);
            }
        };

        // キーバインドは外して、Undo の U と衝突しないようにする
        // 1段上げはボタンクリックのみ使用
    } catch (e) {
        console.error('raisePuyoOneRow init error:', e);
    }
})();

// オンライン・履歴・おじゃま同期用の公開API
window.getNextQueue = function() {
    return copyNextQueue(nextQueue);
};

window.setNextQueue = function(newQueue) {
    if (!Array.isArray(newQueue)) return;
    nextQueue = JSON.parse(JSON.stringify(newQueue));
    queueIndex = 0;
    ensureNextQueueCapacity();
    renderBoard();
};

window.getPendingOjama = function() {
    return pendingOjama;
};

window.applyPendingOjamaToBoard = applyPendingOjamaToBoard;
window.triggerGameOver = triggerGameOver;

// --- AI / 外部連携フック ---
window.getBoardSnapshot = function() {
    return copyBoard(board);
};

window.getGameState = function() {
    return gameState;
};

window.getCurrentPuyoState = function() {
    return currentPuyo ? { ...currentPuyo } : null;
};

window.getUpcomingPairs = function(count = 3) {
    return nextQueue.slice(queueIndex, queueIndex + count).map(pair => pair.slice());
};

// AI が狙った向き・位置へ一瞬で寄せてから落とすためのフック
window.__aiApplyPlacement = function(mainX, rotation) {
    if (gameState !== 'playing' || !currentPuyo) return false;

    currentPuyo.mainX = mainX;
    currentPuyo.mainY = HEIGHT - 2; // AI はここから最適位置へ hardDrop する
    currentPuyo.rotation = rotation;

    if (typeof renderBoard === 'function') renderBoard();
    return true;
};

// 明示的に外から使えるようにしておく
window.hardDrop = hardDrop;

// 初期化
document.addEventListener('DOMContentLoaded', () => {
    initializeGame();
    window.addEventListener('resize', checkMobileControlsVisibility);
});

// AI連携用のグローバルエクスポート
(function() {
    setInterval(() => {
        if (typeof currentPuyo !== 'undefined' && currentPuyo) {
            window.currentPuyo = currentPuyo;
            window.mainX = currentPuyo.mainX;
            window.rotation = currentPuyo.rotation;
        }
        if (typeof board !== 'undefined') window.board = board;
        if (typeof gameState !== 'undefined') window.gameState = gameState;
        if (typeof nextQueue !== 'undefined') window.nextQueue = nextQueue;
        if (typeof queueIndex !== 'undefined') window.queueIndex = queueIndex;
        if (typeof hardDrop === 'function') window.hardDrop = hardDrop;
        if (typeof initGame === 'function') window.initGame = initGame;
    }, 100);
})();
