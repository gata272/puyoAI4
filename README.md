# PuyoAI3

ぷよぷよシミュレータに統合するための新しいAI実装です。

## 現在のAI

### 1. GTR構築

既存 `puyoAI2` のGTR構築ロジックを分離して保持しています。

- 3手分のツモから色を抽象化
- GTR用パターンを判定
- パターンに対応する3手の固定プランを生成

### 2. GTR後

GTR構築後は固定手順ではなく、

- 3手先のBeam Search
- amaの公開 `build` プロファイルの線形評価重み
- ama-style quiescence search（最大3個の単体ぷよ追加によるトリガー探索）
- 盤面シミュレーション

を使用します。

評価特徴量は `shape / well / bump / form / link_2 / link_3 / waste_14 / side / nuisance` と、
先読みで評価する `chain / y / key / chi`、操作由来の `tear / waste` です。

> 注意: ビットフィールド/SIMD、厳密な操作フレーム数、探索用の転置表などは独立実装です。一方、公開されているGTR/SGTR/FRON人間形、評価式、link_2/link_3の定義、quiet searchの考え方はできるだけ直接対応させています。

## Web / オンライン機能

既存の `puyoSim.js`、`online.js`、`online.css` を残しているため、PeerJSを利用したオンライン対戦機能は維持します。

ブラウザ上では、

`puyoSim.js → puyoAI.js → Worker → WASM → AI`

という経路でAIを実行します。

## ビルド

GitHub ActionsでEmscriptenを取得し、WASMを生成してGitHub Pagesへデプロイします。

ローカルでAIコアだけ確認する場合:

```bash
make test
```

## ディレクトリ

```text
ai/
  ai.cpp
  gtr/
  evaluation/
  search/
  simulation/

wasm/
  puyoAI.cpp

puyoSim.js
online.js
online.css
puyoAI.js
puyo-ai-worker-wasm.js

config/
  weights.json
  search.json

tools/
  benchmark.cpp
  tuner.cpp

docs/
  ARCHITECTURE.md
  AMA_EVALUATION.md
  MIGRATION.md
```
