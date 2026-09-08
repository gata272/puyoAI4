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

The current production baseline is depth 6 / beam width 12 with six total
pairs available to the search. Benchmark settings can override this. Results
should be compared using identical `seed`, game count, turn count, depth and
beam width except for the single variable being tested.

The primary research metric is maximum chain per game. Average maximum chain,
median, p90, threshold rates (5/8/10/12), survival, score and thinking time
are secondary metrics.
