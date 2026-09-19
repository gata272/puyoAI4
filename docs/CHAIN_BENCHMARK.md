# 最大連鎖ベンチマーク

このプロジェクトでは、AI改善の主目的を「最大連鎖数の向上」として評価するため、決定論的な単独対局ベンチマークを用意しています。

## 指標

- 平均最大連鎖数: 各ゲームで到達した最大連鎖の平均
- 中央値: 最大連鎖数の50パーセンタイル
- 90%点: 最大連鎖数の90パーセンタイル
- 最大: 全ゲーム中の最大連鎖
- 5/8/10/12連鎖以上: その連鎖数以上に到達したゲームの割合
- 平均スコア・平均生存ターン: 補助指標
- 平均思考時間: 1手あたりのAI計算時間

## 再現性

`seed`, `games`, `turns` が同じなら、AI設定が変わっても同じゲーム番号に同じツモ列を与えます。したがって、depthやBeam幅を変えた比較が可能です。

## GitHub Pages

1. 設定を開く
2. 「デバッグモード」をONにする
3. 「最大連鎖ベンチマーク」の設定を入力
4. 「最大連鎖ベンチマーク開始」を押す
5. 結果が設定画面内に表示される

ベンチマークはWeb Workerで実行するため、通常のゲームUIを直接ブロックしません。

## CLI

```bash
make benchmark
```

直接指定する場合:

```bash
./puyoai_benchmark <games> <turns> <seed> <depth> <beamWidth>
```

例:

```bash
./puyoai_benchmark 100 60 20260908 3 8
```

## 注意

このベンチマークは、現状では「ランダムなツモ列に対する単独プレイ」を測定します。オンライン対戦の勝率やおじゃま相互作用は評価対象ではありません。また、ブラウザ版のJSゲームエンジンとC++研究用Simulatorには既知の実装差があるため、研究用の相対比較を主目的とします。


## Maximum-chain-focused search

The current production baseline uses the visible current pair plus the next two
pairs (three pairs total), with the normal default of depth 3 / beam width 24.
The search must not depend on hidden future pieces. Benchmark settings can
override depth/Beam, but the available piece queue remains limited by the data
passed to the AI. Results should be compared using identical `seed`, game count,
turn count, depth and beam width except for the single variable being tested.

The primary research metric is maximum chain per game. Average maximum chain,
median, p90, threshold rates (5/8/10/12), survival, score and thinking time
are secondary metrics.

## 進行表示とゲームオーバー診断

長時間のベンチマークでは、コンソールに次の形式でゲームごとの進行状況を表示します。

```text
[Benchmark] Game 47/100 | max=12 | survived=87/100 | gameOver=no_safe_move | h2=12 | maxH=12 | occupied=54 | safeMoves=0/22
```

`gameOverReasons` には、ゲームオーバーの原因として「安全な配置が残っていなかった」「安全な配置があるのに死亡手を選択した」「幾何学的に配置できなかった」「無効手」などを集計します。これにより、単に平均生存ターンを見るだけでなく、次の改善で何を直すべきかを切り分けられます。


### 死亡直前の診断

ゲームオーバーした場合、直前5ターン（死亡手を含む）について、安全手数・幾何学的合法手数・最大高さ・左から3列目（内部表現のx=2列）の高さ・総占有数を記録します。

例:

```text
[Benchmark]   death-4 | turn=47 | safeMoves=4/6 | maxH=13 | h2=11 | occupied=74
...
[Benchmark]   death-0 | turn=51 | safeMoves=0/2 | maxH=13 | h2=11 | occupied=76
```

`death-0` が死亡手を実行する直前、`death-1` がその1手前です。これにより、安全手数が急減し始める時点を調べられます。結果JSONには、死亡ゲームについての各距離の平均安全手数 (`averageSafeMovesBeforeDeath`)、平均幾何学的合法手数 (`averageGeometricMovesBeforeDeath`)、対象ゲーム数 (`diagnosticCounts`) も含まれます。

### ブラウザでの進行表示

ベンチマークはWeb Worker内でWASMを実行するため、C++の標準出力だけに依存せず、WASMからWorkerへ進行メッセージを送信します。ブラウザの開発者コンソールにはゲームごとに `Game n/N` が表示され、設定画面のステータスにも現在の進行数が表示されます。

### ログの保存・コピー

ベンチマーク完了後、結果欄に「測定ログをコピー」と「測定ログをファイル保存」が表示されます。
前者は進行ログ・死亡直前診断・最終集計をクリップボードへコピーし、後者は同じ内容をUTF-8の
`.log.txt` ファイルとして保存します。長時間の100ゲーム測定でも、コンソールから手作業でログを
回収する必要はありません。
