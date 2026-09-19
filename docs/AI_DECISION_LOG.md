# AI 1手ログ（Decision Log）

デバッグモードでAIを動かすと、AIの各手について詳細なJSONログをブラウザから保存できます。

## 保存方法

1. 設定を開く。
2. 「デバッグモード」をONにする。
3. AIを自動ONにしてプレイする。
4. ゲームオーバー、または分析したい時点で設定を開く。
5. 「AI手ログを保存」を押す。
6. 出力された `puyoAI-decision-log-*.json` を分析用に渡す。

「AI手ログをコピー」を使えば、JSON全体をクリップボードへコピーできます。
「AI手ログをクリア」は現在までのログを削除します。

ゲームリセットではログ全体は消えず、新しい `gameId` が割り当てられます。複数ゲームを連続して記録できます。

## 1手の記録内容

- `gameId`: ゲーム番号
- `turn`: AIの手数
- `pieces`: AIが見えていた現在ぷよ＋NEXT
- `depth`, `beamWidth`: 探索設定
- `preBoard`: AIが考える直前の盤面
- `preHeights`: 列ごとの高さ
- `selected`: AIが実際に選んだ `(x, rotation)`
- `patternName`: GTRパターン名など
- `internalDebugLog`: WASM側のBeam Search内部ログ
- `placementBoard`: ぷよを置いた直後、連鎖解決前の盤面
- `placementHeights`: 置いた直後の列高さ
- `placementGroups`: 置いた直後に存在した4個以上のグループ
- `postBoard`: 連鎖解決・重力後の盤面
- `postHeights`: 解決後の列高さ
- `chain`: その手で発生した最大連鎖数
- `scoreBefore`, `scoreAfter`, `scoreDelta`
- `pendingOjama`
- `gameState`, `gameOver`
- `thinkMs`: AIの思考時間

## 分析時に特に見る項目

### 小連鎖

`chain <= 3` の手を抽出し、その直前の `preBoard`、`placementBoard`、`selected`、`internalDebugLog` を比較します。

特に、

- 発火点を早く消している
- 3+1 / 2+2 の準備を4個にしてしまっている
- 次の色を受けるための空間を潰している
- `virtualPotential` は高いが実際の `placementGroups` が即発火している
- Beamの上位候補が同じ危険な根手に集中している

といったパターンを調べられます。

### ゲームオーバー

`gameOver=true` の手だけでなく、その2～5手前まで遡ってください。

特に `preHeights` → `placementHeights` → `postHeights` の変化と、
`futureSafeMoves`、`next2Safe`、`rootSafe` などを `internalDebugLog` から追跡すると、
「死亡した手」ではなく「死亡を避けられなくした手」を特定できます。

### 長連鎖

`chain >= 10` の成功例と `chain <= 4` の失敗例で、同じような盤面・同じようなNEXTに対してAIがどの評価をしているかを比較してください。

14連鎖などの成功例は特に重要です。成功例の直前数十手を失敗例と比較することで、現在のAIが長連鎖を作る際に共通して維持している構造を抽出できます。
