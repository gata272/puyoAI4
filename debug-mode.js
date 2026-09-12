/* Debug-mode settings and benchmark controls. */
(function (global) {
    'use strict';

    const STORAGE_KEY = 'puyoAI.debugMode';
    const DEVELOPER_STORAGE_KEY = 'puyoAI.developerMode';
    const WEIGHTS_STORAGE_KEY = 'puyoAI.developerWeights';
    const DEFAULTS = {
        games: 5,
        turns: 60,
        seed: 20260908,
        depth: 10,
        beamWidth: 12
    };

    const STATE = {
        debugMode: false,
        worker: null,
        ready: false,
        running: false,
        developerMode: false,
        weights: []
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
            depth: read('benchmark-depth', DEFAULTS.depth, 1, 50),
            beamWidth: read('benchmark-beam', DEFAULTS.beamWidth, 1, 500)
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

    function setDeveloperMode(enabled) {
        STATE.developerMode = !!enabled;
        try { localStorage.setItem(DEVELOPER_STORAGE_KEY, STATE.developerMode ? 'true' : 'false'); } catch (_) {}
        const checkbox = $('developer-mode-checkbox');
        if (checkbox) checkbox.checked = STATE.developerMode;
        const panel = $('developer-panel');
        if (panel) panel.hidden = !STATE.developerMode;
        const badge = $('developer-mode-badge');
        if (badge) badge.hidden = !STATE.developerMode;
        if (STATE.developerMode) requestWeights();
    }

    function renderWeights(weights) {
        STATE.weights = weights || [];
        const container = $('developer-weights');
        if (!container) return;
        container.innerHTML = '';
        for (const item of STATE.weights) {
            const row = document.createElement('div');
            row.className = 'developer-weight-row';
            const label = document.createElement('label');
            label.textContent = item.name;
            const input = document.createElement('input');
            input.type = 'number';
            input.step = 'any';
            input.dataset.weightIndex = String(item.index);
            input.value = Number(item.value).toString();
            row.append(label, input);
            container.appendChild(row);
        }
        const status = $('developer-status');
        if (status) status.textContent = `${STATE.weights.length}個の重みを読み込みました`;
    }

    global.renderDeveloperWeights = renderWeights;

    function requestWeights() {
        if (typeof global.requestAIWeights === 'function') {
            global.requestAIWeights();
        } else {
            const status = $('developer-status');
            if (status) status.textContent = '通常AIワーカーを初期化中…';
            if (typeof global.toggleAI === 'function') {
                // toggleAI is not forced here; the normal worker is initialized
                // only when AI is actually enabled.
            }
        }
    }

    global.toggleDeveloperMode = function () {
        const checkbox = $('developer-mode-checkbox');
        if (!STATE.debugMode) {
            if (checkbox) checkbox.checked = false;
            setDeveloperMode(false);
            return;
        }
        setDeveloperMode(!!checkbox?.checked);
    };

    global.applyDeveloperWeights = function () {
        const inputs = document.querySelectorAll('#developer-weights input[data-weight-index]');
        const values = [];
        for (const input of inputs) {
            const index = Number.parseInt(input.dataset.weightIndex, 10);
            const value = Number(input.value);
            if (!Number.isFinite(index) || !Number.isFinite(value)) {
                const status = $('developer-status');
                if (status) status.textContent = '数値が不正な項目があります';
                return;
            }
            values[index] = value;
        }
        try { localStorage.setItem(WEIGHTS_STORAGE_KEY, JSON.stringify(values)); } catch (_) {}
        if (typeof global.applyAIWeights === 'function') {
            global.applyAIWeights(values);
        }
        const status = $('developer-status');
        if (status) status.textContent = '重みを適用・保存しました';
    };

    global.resetDeveloperWeights = function () {
        try { localStorage.removeItem(WEIGHTS_STORAGE_KEY); } catch (_) {}
        if (typeof global.resetAIWeights === 'function') {
            global.resetAIWeights();
        }
        requestWeights();
        const status = $('developer-status');
        if (status) status.textContent = 'ama基準値に戻しました';
    };

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
        try { setDeveloperMode(localStorage.getItem(DEVELOPER_STORAGE_KEY) === 'true'); } catch (_) { setDeveloperMode(false); }
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
