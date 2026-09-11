# PuyoAI

ぷよぷよシミュレータに統合した研究用AIです。既存のオンライン対戦機能を維持しながら、GTR構築後をBeam Search + ama-style評価で探索します。

## AI

- 序盤3手: 既存GTR構築ロジック
- 通常探索: **3ペア（現在+次2手）に制限したBeam Search**
- 評価: amaの公開評価をベースにした線形評価 + Long Chain Potential + 発火点Transfer評価 + quiescence search
- 3個ちょうどの同色塊を「発火点候補」として保持し、`B → A` の依存関係を上方向・横方向の両方で評価
- 発火点を壊さず、次の色へ受け渡す構造 `C → B → A`、`D → C → B → A` を優先
- 研究用に探索Beam幅を変更可能（探索深度は最大3）
- Long Chain Potentialで「今の連鎖数」だけでなく、2/3連結・伸ばし先・S字形状・ツモ適合性などの潜在的な大連鎖構造を評価
- Beamの一部をLong Chain Potentialのeliteとして保持し、中連鎖への過度な収束を抑制

AIコアは `ai/` 以下に分離し、Webゲーム本体の `puyoSim.js` とは独立したC++ Simulatorを使用します。

## 最大連鎖ベンチマーク

AI改善の主目的を「最大連鎖数の向上」として、決定論的な単独対局ベンチマークを実装しています。

### ブラウザ

1. GitHub Pagesを開く
2. 「設定」を開く
3. 「デバッグモード」をON
4. 試行ゲーム数、ターン数、Seed、探索深度、Beam幅を設定
5. 「最大連鎖ベンチマーク開始」を押す
6. 結果が画面内に表示されます

ベンチマークは `benchmark-worker.js` で通常AIとは別のWeb Workerとして実行します。そのため、測定中もメインのUIスレッドを直接ブロックしません。

同じ `Seed / ゲーム数 / ターン数` であれば、depthやBeam幅を変えても同じツモ列が使用されます。これによりAI設定間の比較を再現可能にしています。

主指標:

- 平均最大連鎖数
- 中央値
- 90%点
- 全ゲーム中の最大連鎖
- 5/8/10/12連鎖以上の到達率

補助指標:

- 平均スコア
- 平均生存ターン
- 平均思考時間
- ゲームオーバー数

詳細は `docs/CHAIN_BENCHMARK.md` を参照してください。

## CLI

```bash
make test
make benchmark
```

直接ベンチマークを実行する場合:

```bash
./puyoai_benchmark <games> <turns> <seed> <depth> <beamWidth>
```

例:

```bash
./puyoai_benchmark 100 60 20260908 3 8
```

## GitHub Pages

`.github/workflows/build-wasm.yml` がEmscriptenでWASMをビルドし、GitHub Pages用artifactを生成します。WASM生成物はリポジトリにコミットせず、Actionsで毎回生成します。

## オンライン対戦

既存の `puyoSim.js`、`online.js`、`online.css` とPeerJSによるオンライン機能を維持しています。AI・デバッグ機能はオンライン機能のコードとは分離されています。

## ディレクトリ

```text
ai/
  ai.cpp / ai.h
  benchmark/
    chain_benchmark.cpp / chain_benchmark.h
  gtr/
  evaluation/
  search/
  simulation/

wasm/
  puyoAI.cpp

benchmark-worker.js
debug-mode.js
puyoAI.js
puyo-ai-worker-wasm.js
puyoSim.js
online.js
online.css

config/
  weights.json
  search.json

tools/
  chain_benchmark_cli.cpp
  tuner.cpp

docs/
  ARCHITECTURE.md
  AMA_EVALUATION.md
  CHAIN_BENCHMARK.md
  DEBUG_MODE.md
  MIGRATION.md
  RESEARCH_NEXT.md
  LONG_CHAIN_POTENTIAL.md
```

## Trigger-route large-chain construction

The current AI keeps the existing GTR + ama-style evaluation and 3-pair beam search, while reserving a small part of the beam for human-style trigger-transfer structures. Exact 3-groups are treated as latent anchors; 3+1 and 2+2 hand-offs are detected by hypothetical trigger removal plus gravity. A final-beam post-trigger simulation estimates chain-tail value. See `docs/TRIGGER_ROUTE_POLICY.md`.

## Chain Blueprint

The AI includes a sequential chain-blueprint evaluator. It models dependencies between individual board groups/events, allowing repeated colours such as `A -> B -> A -> C`, while deliberately avoiding a branch bonus for simultaneous deletions. Full recursive blueprint analysis is applied to the final beam to balance long-chain quality against search cost.
