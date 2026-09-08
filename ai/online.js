/* online.js */
(function() {
    let peer = null;
    let conn = null;
    let myId = '';
    let isHost = false;

    let winTarget = 0;
    let myWins = 0;
    let oppWins = 0;
    let isMatchActive = false;

    let peerInitialized = false;
    let peerInitializing = false;
    let reconnectAttempts = 0;
    const MAX_RECONNECT_ATTEMPTS = 3;

    let oppScore = 0;
    let oppChainCount = 0;
    let oppOjamaPending = 0;

    let boardSyncTimer = null;
    const BOARD_SYNC_INTERVAL = 100;

    let rematchRequested = false;
    let rematchPendingFromOpponent = false;

    function updateCopyIdButtons() {
        const copyBtn = document.getElementById('copy-my-id-btn');
        if (copyBtn) {
            copyBtn.disabled = !myId || myId === '----';
            copyBtn.textContent = myId && myId !== '----' ? 'IDをコピー' : 'ID未生成';
        }

        const miniBtn = document.querySelector('.mini-copy-btn');
        if (miniBtn) {
            miniBtn.disabled = !myId || myId === '----';
        }
    }

    window.copyMyPeerId = async function() {
        if (!myId || myId === '----') {
            alert('まだIDが生成されていません。');
            return;
        }

        try {
            await navigator.clipboard.writeText(myId);

            const buttons = [
                document.getElementById('copy-my-id-btn'),
                document.querySelector('.mini-copy-btn')
            ];

            buttons.forEach(btn => {
                if (!btn) return;
                const original = btn.textContent;
                btn.textContent = 'コピー済み';
                setTimeout(() => {
                    btn.textContent = original || 'IDをコピー';
                }, 1200);
            });
        } catch (err) {
            console.error('Clipboard copy failed:', err);
            prompt('IDをコピーしてください', myId);
        }
    };

    // ---------- UI ----------
    window.showOnlineOverlay = function() {
        const overlay = document.getElementById('online-overlay');
        if (overlay) {
            overlay.style.display = 'flex';
            if (!peerInitialized && !peerInitializing) {
                initPeer();
            }
            updateCopyIdButtons();
        }
    };

    window.hideOnlineOverlay = function() {
        const overlay = document.getElementById('online-overlay');
        if (overlay) {
            overlay.style.display = 'none';
        }
    };

    window.proposeMatch = function() {
        const select = document.getElementById('match-win-target-select');
        const count = parseInt(select ? select.value : '1', 10);

        if (conn && conn.open) {
            conn.send({ type: 'PROPOSE_MATCH', winTarget: count });
            const content = document.getElementById('proposal-content');
            const actions = document.getElementById('proposal-actions');
            if (content) {
                content.innerHTML = `<p>${count}本先取の提案を送信しました。相手の承認を待っています...</p>`;
            }
            if (actions) actions.innerHTML = '';
        }
    };

    window.acceptMatch = function(target) {
        if (conn && conn.open) {
            conn.send({ type: 'ACCEPT_MATCH', winTarget: target });
            startMatch(target);
        }
    };

    window.rejectMatch = function() {
        const overlay = document.getElementById('match-proposal-overlay');
        if (overlay) overlay.style.display = 'none';
    };

    window.connectToOpponent = function() {
        const input = document.getElementById('opponent-id-input');
        const targetId = input ? input.value.trim() : '';

        if (!targetId) {
            alert('相手のIDを入力してください');
            return;
        }

        if (!peer || !myId) {
            alert('PeerJSがまだ初期化されていません。少々お待ちください。');
            return;
        }

        if (conn && conn.open) {
            alert('既に接続済みです');
            return;
        }

        const status = document.getElementById('online-status');
        if (status) status.textContent = '接続中...';

        reconnectAttempts = 0;
        attemptConnection(targetId);
    };

    window.surrenderMatch = function() {
        if (isMatchActive && conn && conn.open) {
            if (confirm('対戦を降参しますか？（シリーズ敗北となります）')) {
                conn.send({ type: 'OPPONENT_SURRENDERED' });
                showMatchResult('シリーズ敗北...');
                endMatch();
            }
        }
    };

    window.requestRematch = function() {
        if (!conn || !conn.open) {
            alert('接続がありません。');
            return;
        }
        if (isMatchActive) {
            alert('対戦中は連戦を申し込めません。');
            return;
        }
        if (rematchRequested) {
            return;
        }

        rematchRequested = true;
        conn.send({
            type: 'REMATCH_REQUEST',
            winTarget: winTarget
        });

        const content = document.getElementById('result-content');
        const actions = document.getElementById('result-actions');
        if (content) {
            content.innerHTML = `
                <p>連戦の申し込みを送信しました。</p>
                <p style="font-size: 1.0em; margin-top: 10px;">相手の返答を待っています...</p>
            `;
        }
        if (actions) {
            actions.innerHTML = `
                <button class="online-btn secondary" onclick="location.reload()">終了</button>
            `;
        }
    };

    window.acceptRematch = function(target) {
        if (!conn || !conn.open) return;

        conn.send({
            type: 'REMATCH_ACCEPT',
            winTarget: target || winTarget
        });

        const overlay = document.getElementById('match-proposal-overlay');
        if (overlay) overlay.style.display = 'none';

        rematchPendingFromOpponent = false;
        rematchRequested = false;

        startMatch(target || winTarget);
    };

    window.rejectRematch = function() {
        if (!conn || !conn.open) return;

        conn.send({ type: 'REMATCH_REJECT' });

        const overlay = document.getElementById('match-proposal-overlay');
        if (overlay) overlay.style.display = 'none';

        rematchPendingFromOpponent = false;
    };

    // ---------- DOM ----------
    function initOnlineUI() {
        if (!document.getElementById('online-overlay')) {
            const overlay = document.createElement('div');
            overlay.id = 'online-overlay';
            overlay.innerHTML = `
                <div class="online-box">
                    <h2>オンライン対戦</h2>
                    <div id="online-status">PeerJSを初期化中...</div>
                    <div id="my-id-display" style="margin: 10px 0; font-size: 0.9em; color: #aaa;">
                        あなたのID: <span id="my-peer-id" style="color: #fff; font-weight: bold;">----</span>
                        <button class="online-btn secondary copy-id-btn" id="copy-my-id-btn" onclick="copyMyPeerId()" disabled>ID未生成</button>
                    </div>
                    <input type="text" id="opponent-id-input" placeholder="相手のIDを入力">
                    <button class="online-btn" onclick="connectToOpponent()">接続する</button>
                    <button class="online-btn secondary" onclick="hideOnlineOverlay()">キャンセル</button>
                </div>
            `;
            document.body.appendChild(overlay);
        }

        if (!document.getElementById('match-proposal-overlay')) {
            const proposalOverlay = document.createElement('div');
            proposalOverlay.id = 'match-proposal-overlay';
            proposalOverlay.innerHTML = `
                <div class="online-box">
                    <h2 id="proposal-title">対戦の提案</h2>
                    <div id="proposal-content"></div>
                    <div id="proposal-actions" style="margin-top: 15px;"></div>
                </div>
            `;
            document.body.appendChild(proposalOverlay);
        }

        if (!document.getElementById('match-result-overlay')) {
            const resultOverlay = document.createElement('div');
            resultOverlay.id = 'match-result-overlay';
            resultOverlay.innerHTML = `
                <div class="online-box">
                    <h2 id="result-title">対戦終了</h2>
                    <div id="result-content" style="margin: 20px 0; font-size: 1.1em;"></div>
                    <div id="result-actions" style="margin-top: 15px;"></div>
                </div>
            `;
            document.body.appendChild(resultOverlay);
        }

        if (!document.getElementById('win-count-container')) {
            const playStatsInfo = document.getElementById('play-stats-info');
            if (playStatsInfo) {
                const winContainer = document.createElement('div');
                winContainer.id = 'win-count-container';
                winContainer.className = 'stat-item';
                winContainer.innerHTML = `
                    <span class="stat-label">勝利数</span>
                    <span id="win-count-display" class="stat-value">0 - 0</span>
                    <button class="mini-copy-btn" onclick="copyMyPeerId()" disabled>ID</button>
                `;
                playStatsInfo.appendChild(winContainer);
            }
        }

        if (!document.getElementById('opponent-board-container')) {
            const playStatsInfo = document.getElementById('play-stats-info');
            if (playStatsInfo) {
                const oppContainer = document.createElement('div');
                oppContainer.id = 'opponent-board-container';
                oppContainer.innerHTML = `
                    <h3>相手の盤面</h3>
                    <div id="opponent-board"></div>
                    <div id="opponent-info" style="margin-top: 3px; display: flex; justify-content: space-around; font-size: 0.6em; color: #aaa;">
                        <div>スコア: <span id="opp-score">0</span></div>
                        <div>連鎖: <span id="opp-chain">0</span></div>
                        <div>おじゃま: <span id="opp-ojama">0</span></div>
                    </div>
                `;
                playStatsInfo.appendChild(oppContainer);
                createOpponentBoardDOM();
            }
        }
    }

    function createOpponentBoardDOM() {
        const boardElement = document.getElementById('opponent-board');
        if (!boardElement) return;
        boardElement.innerHTML = '';
        for (let y = 13; y >= 0; y--) {
            for (let x = 0; x < 6; x++) {
                const cell = document.createElement('div');
                cell.id = `opp-cell-${x}-${y}`;
                const puyo = document.createElement('div');
                puyo.className = 'puyo puyo-0';
                cell.appendChild(puyo);
                boardElement.appendChild(cell);
            }
        }
    }

    // ---------- Peer ----------
    function initPeer() {
        if (peerInitialized || peerInitializing) return;
        peerInitializing = true;

        try {
            peer = new Peer({
                debug: 0,
                config: {
                    iceServers: [
                        { urls: ['stun:stun.l.google.com:19302'] },
                        { urls: ['stun:stun1.l.google.com:19302'] },
                        { urls: ['stun:stun2.l.google.com:19302'] }
                    ]
                }
            });

            peer.on('open', (id) => {
                myId = id;
                peerInitialized = true;
                peerInitializing = false;

                const idEl = document.getElementById('my-peer-id');
                if (idEl) idEl.textContent = id;

                const status = document.getElementById('online-status');
                if (status) status.textContent = '接続待機中...';

                updateCopyIdButtons();
            });

            peer.on('connection', (connection) => {
                if (conn && conn.open) {
                    connection.close();
                    return;
                }

                conn = connection;
                setupConnection();
                isHost = true;
                showMatchProposal();
            });

            peer.on('error', (err) => {
                console.error('PeerJS Error:', err);
                peerInitializing = false;

                const status = document.getElementById('online-status');
                if (err.type === 'unavailable-id') {
                    if (status) status.textContent = 'IDの生成に失敗しました。';
                } else if (err.type === 'disconnected') {
                    if (status) status.textContent = 'サーバーから切断されました。再接続中...';
                    setTimeout(() => { if (!peerInitialized) initPeer(); }, 2000);
                } else if (err.type === 'network') {
                    if (status) status.textContent = 'ネットワークエラーが発生しました。';
                } else {
                    if (status) status.textContent = `エラー: ${err.type}`;
                }

                updateCopyIdButtons();
            });

            peer.on('disconnected', () => {
                peerInitialized = false;
                const status = document.getElementById('online-status');
                if (status) status.textContent = 'サーバーから切断されました。再接続中...';
                setTimeout(() => {
                    if (!peerInitialized && peer) peer.reconnect();
                }, 2000);
            });
        } catch (err) {
            console.error('Failed to initialize Peer:', err);
            peerInitializing = false;
        }
    }

    function attemptConnection(targetId) {
        try {
            conn = peer.connect(targetId, { reliable: true });
            setupConnection();
            isHost = false;
        } catch (err) {
            console.error('Connection attempt failed:', err);
            if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
                reconnectAttempts++;
                const status = document.getElementById('online-status');
                if (status) {
                    status.textContent = `接続中... (再試行 ${reconnectAttempts}/${MAX_RECONNECT_ATTEMPTS})`;
                }
                setTimeout(() => attemptConnection(targetId), 1000);
            } else {
                const status = document.getElementById('online-status');
                if (status) status.textContent = '接続失敗。もう一度お試しください。';
                alert('接続に失敗しました。相手のIDが正しいか確認してください。');
            }
        }
    }

    function setupConnection() {
        conn.on('open', () => {
            window.hideOnlineOverlay();

            const status = document.getElementById('online-status');
            if (status) status.textContent = '接続済み';

            if (isHost) showMatchProposal();
        });

        conn.on('data', (data) => {
            handleReceivedData(data);
        });

        conn.on('close', () => {
            alert('対戦相手との接続が切れました。');
            endMatch();
            conn = null;
        });

        conn.on('error', (err) => {
            alert('接続エラーが発生しました: ' + err.type);
        });
    }

    function handleReceivedData(data) {
        switch (data.type) {
            case 'PROPOSE_MATCH':
                showApprovalUI(data.winTarget);
                break;

            case 'ACCEPT_MATCH':
                startMatch(data.winTarget);
                break;

            case 'BOARD_UPDATE':
                updateOpponentBoard(data.board, data.currentPuyo, data.gameState);
                oppScore = data.score || 0;
                oppChainCount = data.chainCount || 0;
                oppOjamaPending = data.ojamaPending || 0;
                updateOpponentInfo();
                break;

            case 'SYNC_NEXT':
                if (window.setNextQueue) {
                    window.setNextQueue(data.nextPuyos);
                }
                break;

            case 'OJAMA':
                if (window.addIncomingOjama) {
                    window.addIncomingOjama(data.amount);
                } else if (window.receiveOjama) {
                    window.receiveOjama(data.amount);
                }
                break;

            case 'OPPONENT_LOST':
                endMatchWithWinner(true);
                break;

            case 'OPPONENT_SURRENDERED':
                showMatchResult('シリーズ勝利！');
                endMatch();
                break;

            case 'REMATCH_REQUEST':
                if (isMatchActive) break;
                rematchPendingFromOpponent = true;
                showRematchProposal(data.winTarget);
                break;

            case 'REMATCH_ACCEPT':
                if (isMatchActive) break;
                rematchRequested = false;
                startMatch(data.winTarget || winTarget);
                break;

            case 'REMATCH_REJECT':
                if (isMatchActive) break;
                rematchRequested = false;
                showMatchResult('連戦は拒否されました。');
                break;
        }
    }

    // ---------- 対戦UI ----------
    function showMatchProposal() {
        const overlay = document.getElementById('match-proposal-overlay');
        const content = document.getElementById('proposal-content');
        const actions = document.getElementById('proposal-actions');
        const title = document.getElementById('proposal-title');

        if (!overlay || !content || !actions || !title) return;

        overlay.style.display = 'flex';
        title.textContent = '対戦設定';
        content.innerHTML = `
            <p>何本先取にしますか？</p>
            <select id="match-win-target-select" style="width: 100%; padding: 10px; margin-bottom: 10px; background: #222; color: white; border: 1px solid #444; border-radius: 5px;">
                ${[1,2,3,4,5,6,7,8,9,10].map(n => `<option value="${n}">${n}本先取</option>`).join('')}
            </select>
        `;
        actions.innerHTML = `<button class="online-btn" onclick="proposeMatch()">提案を送る</button>`;
    }

    function showApprovalUI(target) {
        const overlay = document.getElementById('match-proposal-overlay');
        const content = document.getElementById('proposal-content');
        const actions = document.getElementById('proposal-actions');
        const title = document.getElementById('proposal-title');

        if (!overlay || !content || !actions || !title) return;

        overlay.style.display = 'flex';
        title.textContent = '対戦の誘い';
        content.innerHTML = `<p>相手から <strong>${target}本先取</strong> の対戦提案が届きました。</p>`;
        actions.innerHTML = `
            <button class="online-btn" onclick="acceptMatch(${target})">承認して開始</button>
            <button class="online-btn secondary" onclick="rejectMatch()">拒否</button>
        `;
    }

    function showRematchProposal(target) {
        const overlay = document.getElementById('match-proposal-overlay');
        const content = document.getElementById('proposal-content');
        const actions = document.getElementById('proposal-actions');
        const title = document.getElementById('proposal-title');

        if (!overlay || !content || !actions || !title) return;

        overlay.style.display = 'flex';
        title.textContent = '連戦の申し込み';
        content.innerHTML = `<p>相手から <strong>${target || winTarget}本先取</strong> の連戦申し込みが届きました。</p>`;
        actions.innerHTML = `
            <button class="online-btn" onclick="acceptRematch(${target || winTarget})">承認して再戦</button>
            <button class="online-btn secondary" onclick="rejectRematch()">拒否</button>
        `;
    }

    function startMatch(target) {
        winTarget = target;
        myWins = 0;
        oppWins = 0;
        oppScore = 0;
        oppChainCount = 0;
        oppOjamaPending = 0;
        isMatchActive = true;
        rematchRequested = false;
        rematchPendingFromOpponent = false;

        const overlay = document.getElementById('match-proposal-overlay');
        if (overlay) overlay.style.display = 'none';

        const resultOverlay = document.getElementById('match-result-overlay');
        if (resultOverlay) resultOverlay.style.display = 'none';

        document.body.classList.add('online-match-active');
        ensureSurrenderButton();

        if (window.updateGravityWait) window.updateGravityWait(300);
        if (window.updateChainWait) window.updateChainWait(300);

        if (typeof autoDropEnabled !== 'undefined' && !autoDropEnabled) {
            if (window.toggleAutoDrop) window.toggleAutoDrop();
        }

        if (window.prepareForRematch) {
            window.prepareForRematch();
        } else if (window.resetGame) {
            window.resetGame();
        }

        updateWinCountDisplay();
        startBoardSync();

        if (isHost) {
            setTimeout(() => syncNextPuyos(), 500);
        }
    }

    function ensureSurrenderButton() {
        let surrenderBtn = document.getElementById('surrender-button');
        if (!surrenderBtn) {
            const playControls = document.getElementById('play-controls');
            if (playControls) {
                surrenderBtn = document.createElement('button');
                surrenderBtn.id = 'surrender-button';
                surrenderBtn.onclick = window.surrenderMatch;
                surrenderBtn.style.cssText = 'width: 100%; padding: 8px; border: none; border-radius: 5px; font-size: 0.85em; font-weight: bold; background-color: #d9534f; color: white; margin-top: 5px; display: none;';
                surrenderBtn.textContent = '降参';
                playControls.appendChild(surrenderBtn);
            }
        }
        if (surrenderBtn) surrenderBtn.style.display = 'block';
    }

    function updateWinCountDisplay() {
        const winDisplay = document.getElementById('win-count-display');
        if (winDisplay) {
            winDisplay.textContent = `${myWins} - ${oppWins}`;
        }
    }

    function updateOpponentBoard(oppBoard, oppCurrentPuyo, oppGameState) {
        if (!oppBoard) return;

        for (let y = 0; y < 14; y++) {
            for (let x = 0; x < 6; x++) {
                const cell = document.getElementById(`opp-cell-${x}-${y}`);
                if (!cell) continue;
                const puyo = cell.firstChild;
                if (!puyo) continue;

                let color = oppBoard[y] ? oppBoard[y][x] : 0;

                if (oppGameState === 'playing' && oppCurrentPuyo) {
                    const { mainX, mainY, rotation, mainColor, subColor } = oppCurrentPuyo;
                    let subX = mainX, subY = mainY;

                    if (rotation === 0) subY = mainY + 1;
                    if (rotation === 1) subX = mainX - 1;
                    if (rotation === 2) subY = mainY - 1;
                    if (rotation === 3) subX = mainX + 1;

                    if (x === mainX && y === mainY) color = mainColor;
                    if (x === subX && y === subY) color = subColor;
                }

                puyo.className = `puyo puyo-${color}`;
            }
        }
    }

    function updateOpponentInfo() {
        const scoreSpan = document.getElementById('opp-score');
        const chainSpan = document.getElementById('opp-chain');
        const ojamaSpan = document.getElementById('opp-ojama');

        if (scoreSpan) scoreSpan.textContent = oppScore;
        if (chainSpan) chainSpan.textContent = oppChainCount;
        if (ojamaSpan) ojamaSpan.textContent = oppOjamaPending;
    }

    function sendBoardData() {
        if (!isMatchActive || !conn || !conn.open) return;

        try {
            if (typeof board !== 'undefined') {
                conn.send({
                    type: 'BOARD_UPDATE',
                    board: board,
                    currentPuyo: typeof currentPuyo !== 'undefined' ? currentPuyo : null,
                    gameState: typeof gameState !== 'undefined' ? gameState : 'playing',
                    score: typeof score !== 'undefined' ? score : 0,
                    chainCount: typeof chainCount !== 'undefined' ? chainCount : 0,
                    ojamaPending: typeof pendingOjama !== 'undefined' ? pendingOjama : 0
                });
            }
        } catch (err) {
            console.error('Failed to send board data:', err);
        }
    }

    function startBoardSync() {
        stopBoardSync();
        boardSyncTimer = setInterval(sendBoardData, BOARD_SYNC_INTERVAL);
    }

    function stopBoardSync() {
        if (boardSyncTimer) {
            clearInterval(boardSyncTimer);
            boardSyncTimer = null;
        }
    }

    function syncNextPuyos() {
        if (window.getNextQueue && conn && conn.open) {
            conn.send({
                type: 'SYNC_NEXT',
                nextPuyos: window.getNextQueue()
            });
        }
    }

    window.sendOjama = function(amount) {
        const n = Math.max(0, Math.floor(Number(amount) || 0));
        if (!n) return;

        if (conn && conn.open) {
            conn.send({
                type: 'OJAMA',
                amount: n
            });
        }
    };

    window.sendBoardData = sendBoardData;

    window.notifyGameOver = function() {
        if (isMatchActive && conn && conn.open) {
            conn.send({ type: 'OPPONENT_LOST' });
            endMatchWithWinner(false);
        }
    };

    function endMatchWithWinner(iWon) {
        if (iWon) {
            myWins++;
        } else {
            oppWins++;
        }

        updateWinCountDisplay();

        if (myWins >= winTarget) {
            showMatchResult('シリーズ勝利！');
            endMatch();
        } else if (oppWins >= winTarget) {
            showMatchResult('シリーズ敗北...');
            endMatch();
        } else {
            setTimeout(() => {
                if (window.prepareForRematch) {
                    window.prepareForRematch();
                } else if (window.resetGame) {
                    window.resetGame();
                }
                if (isHost) setTimeout(() => syncNextPuyos(), 500);
            }, 2000);
        }
    }

    function showMatchResult(message) {
        const overlay = document.getElementById('match-result-overlay');
        const content = document.getElementById('result-content');
        const actions = document.getElementById('result-actions');
        if (!overlay || !content || !actions) return;

        overlay.style.display = 'flex';
        rematchRequested = false;
        rematchPendingFromOpponent = false;

        content.innerHTML = `
            <p>${message}</p>
            <p style="font-size: 1.2em; margin-top: 10px;">最終スコア: ${myWins} - ${oppWins}</p>
        `;

        if (conn && conn.open) {
            actions.innerHTML = `
                <button class="online-btn" onclick="requestRematch()">連戦を申し込む</button>
                <button class="online-btn secondary" onclick="location.reload()">終了</button>
            `;
        } else {
            actions.innerHTML = `<button class="online-btn secondary" onclick="location.reload()">終了</button>`;
        }
    }

    function endMatch() {
        isMatchActive = false;
        document.body.classList.remove('online-match-active');
        stopBoardSync();

        const surrenderBtn = document.getElementById('surrender-button');
        if (surrenderBtn) surrenderBtn.style.display = 'none';
    }

    window.setNextQueue = function(newNext) {
        if (typeof newNext !== 'undefined') {
            nextQueue = JSON.parse(JSON.stringify(newNext));
            queueIndex = 0;
            if (window.renderBoard) window.renderBoard();
        }
    };

    // ---------- 起動 ----------
    function autoInitPeer() {
        if (!peerInitialized && !peerInitializing) initPeer();
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => {
            initOnlineUI();
            autoInitPeer();
        });
    } else {
        initOnlineUI();
        autoInitPeer();
    }
})();