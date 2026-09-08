/* Debug-mode settings and benchmark controls. */
(function (global) {
    'use strict';

    const STORAGE_KEY = 'puyoAI.debugMode';
    const DEFAULTS = {
        games: 5,
        turns: 60,
        seed: 20260908,
        depth: 6,
        beamWidth: 12
    };

    const STATE = {
        debugMode: false,
        worker: null,
        ready: false,
        running: false
    };

    function $(id) { return document.getElementById(id); }

    function readBool() {
        return localStorage.getItem(STORAGE_KEY) === 'true';
    }

    function setDebugMode(enabled) {
        STATE.debugMode = !!enabled;
        localStorage.setItem(STORAGE_KEY, STATE.debugMode ? 'true' : 'false');
        const checkbox = $('debug-mode-checkbox');
        if (checkbox) checkbox.checked = STATE.debugMode;
        const panel = $('debug-panel');
        if (panel) panel.hidden = !STATE.debugMode;
        const badge = $('debug-mode-badge');
        if (badge) badge.hidden = !STATE.debugMode;
    }

    function setStatus(text) {
        const el = $('benchmark-status');
        if (el) el.textContent = text;
    }

    function setRunning(running) {
        STATE.running = running;
        const button = $('run-benchmark-button');
        if (button) {
            button.disabled = running || !STATE.ready;
            button.textContent = running ? '測定中…' : '最大連鎖ベンチマーク開始';
        }
    }

    function readConfig() {
        const read = (id, fallback, min, max) => {
            const n = Number.parseInt($(id)?.value ?? fallback, 10);
            if (!Number.isFinite(n)) return fallback;
            return Math.max(min, Math.min(max, n));
        };
        return {
            games: read('benchmark-games', DEFAULTS.games, 1, 5000),
            turns: read('benchmark-turns', DEFAULTS.turns, 1, 500),
            seed: read('benchmark-seed', DEFAULTS.seed, -2147483648, 2147483647),
            depth: read('benchmark-depth', DEFAULTS.depth, 1, 8),
            beamWidth: read('benchmark-beam', DEFAULTS.beamWidth, 1, 128)
        };
    }

    function formatPercent(count, games) {
        return `${((count / games) * 100).toFixed(1)}% (${count}/${games})`;
    }

    function renderResult(result) {
        const el = $('benchmark-result');
        if (!el) return;
        el.innerHTML = `
            <div class="benchmark-summary-grid">
                <div><span>平均最大連鎖</span><strong>${result.averageMaxChain.toFixed(2)}</strong></div>
                <div><span>中央値</span><strong>${result.medianMaxChain.toFixed(2)}</strong></div>
                <div><span>90%点</span><strong>${result.p90MaxChain.toFixed(2)}</strong></div>
                <div><span>最大</span><strong>${result.maxChain}</strong></div>
            </div>
            <table class="benchmark-table">
                <tbody>
                    <tr><th>5連鎖以上</th><td>${formatPercent(result.atLeast5, result.games)}</td></tr>
                    <tr><th>8連鎖以上</th><td>${formatPercent(result.atLeast8, result.games)}</td></tr>
                    <tr><th>10連鎖以上</th><td>${formatPercent(result.atLeast10, result.games)}</td></tr>
                    <tr><th>12連鎖以上</th><td>${formatPercent(result.atLeast12, result.games)}</td></tr>
                    <tr><th>平均スコア</th><td>${result.averageScore.toFixed(1)}</td></tr>
                    <tr><th>平均生存ターン</th><td>${result.averageTurns.toFixed(1)} / ${result.turns}</td></tr>
                    <tr><th>平均思考時間</th><td>${result.averageThinkMs.toFixed(2)} ms / 手</td></tr>
                    <tr><th>測定時間</th><td>${(result.totalWallMs / 1000).toFixed(2)} s</td></tr>
                    <tr><th>設定</th><td>depth ${result.depth} / beam ${result.beamWidth}</td></tr>
                    <tr><th>Seed</th><td>${result.seed}</td></tr>
                </tbody>
            </table>
            <p class="benchmark-note">同じ Seed・試行数・ターン数なら、異なるAI設定でも同じツモ列が使われます。</p>
        `;
    }

    function initWorker() {
        if (STATE.worker) return;
        STATE.worker = new Worker('./benchmark-worker.js', { type: 'module' });
        STATE.worker.onmessage = (event) => {
            const msg = event.data || {};
            if (msg.type === 'ready') {
                STATE.ready = true;
                setRunning(false);
                setStatus('ベンチマーク準備完了');
                return;
            }
            if (msg.type === 'started') {
                setRunning(true);
                setStatus('同一ツモ列で測定しています…');
                return;
            }
            if (msg.type === 'result') {
                try {
                    const result = JSON.parse(msg.resultJson);
                    renderResult(result);
                    setRunning(false);
                    setStatus('測定完了');
                } catch (error) {
                    setRunning(false);
                    setStatus('結果の解析に失敗しました');
                    console.error(error);
                }
                return;
            }
            if (msg.type === 'error') {
                setRunning(false);
                setStatus(msg.message || 'ベンチマークエラー');
                console.error(msg.message);
            }
        };
    }

    global.toggleDebugMode = function () {
        const checkbox = $('debug-mode-checkbox');
        setDebugMode(!!checkbox?.checked);
        if (STATE.debugMode) initWorker();
    };

    global.runChainBenchmark = function () {
        if (!STATE.debugMode) {
            setStatus('設定でデバッグモードをONにしてください');
            return;
        }
        if (STATE.running) return;
        initWorker();
        if (!STATE.ready) {
            setStatus('WASMベンチマークを初期化中です。少し待ってください。');
            return;
        }

        const config = readConfig();
        setRunning(true);
        setStatus('測定開始…');
        $('benchmark-result').innerHTML = '<div class="benchmark-empty">結果を計算中…</div>';
        STATE.worker.postMessage({ type: 'run', ...config });
    };

    global.initializeDebugMode = function () {
        setDebugMode(readBool());
        const checkbox = $('debug-mode-checkbox');
        if (checkbox) checkbox.checked = STATE.debugMode;
        for (const [id, value] of Object.entries({
            'benchmark-games': DEFAULTS.games,
            'benchmark-turns': DEFAULTS.turns,
            'benchmark-seed': DEFAULTS.seed,
            'benchmark-depth': DEFAULTS.depth,
            'benchmark-beam': DEFAULTS.beamWidth
        })) {
            const input = $(id);
            if (input && !input.value) input.value = value;
        }
        if (STATE.debugMode) initWorker();
    };

    document.addEventListener('DOMContentLoaded', global.initializeDebugMode);
})(window);
