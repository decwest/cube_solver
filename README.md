# cube_solver

外部ライブラリに依存しない native 実装です。

現状の対応範囲は次のとおりです。

- 任意の `3x3` 盤面状態を native two-phase solver で解く
- `5x5` の center reduction を native に進める
- center reduction 後に native edge reduction を走らせて `3x3` に落とす
- 長い `5x5` scramble では edge reduction 探索がまだ重い

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Input

state は `URFDLB` 面順です。

- `3x3`: 54 文字
- `5x5`: 150 文字
- `URFDLB` をそのまま受け付ける
- `W R G Y O B` も `U R F D L B` に正規化して受け付ける

`5x5` は任意 state を受け付けます。

- center が崩れていれば、まず native center solver を走らせる
- その後に center-preserving macro を使う native edge reduction を走らせる
- edge group が揃えば、その状態を `3x3` reduction に落とす

現在の edge reduction は短めから中程度の scramble では動きますが、長い scramble では探索が重くなることがあります。その場合は `native 5x5 edge reduction did not finish within the current search limits` を返します。

## Algorithm

現在の実装は大きく 5 段階です。

1. 共通 move engine

- `3x3` と `5x5` をどちらも sticker state として持つ
- 各 sticker を 3 次元座標つき slot に変換し、回転を slot permutation として適用する
- これにより outer turn, wide turn, whole-cube rotation を同じ仕組みで処理する

2. `3x3` solver

- sticker state を `corner permutation / corner orientation / edge permutation / edge orientation` に変換する
- two-phase で解く
- phase 1 は `corner orientation + edge orientation + E-slice` を揃える
- phase 2 は `corner permutation + UD-edge permutation + E-slice permutation` を解く
- 各 phase は pruning table 付きの探索で進める

3. `5x5` center reduction

- `5x5` center を `x-center orbit` と `t-center orbit` に分けて扱う
- 各 orbit について「ある色 4 枚が 24 slot のどこにあるか」を pattern DB にする
- `x` と `t` の 2 つの heuristic を使う IDA* で center を揃える

4. `5x5` edge reduction

- 12 本の `midge` と 24 枚の `wing` を明示的に表現する
- center を壊さない generator として `outer turn` と `wide + outer + wide'` を使う
- A* で edge reduction を進め、各 edge line について
  `midge と 2 枚の wing が同じ edge type / orientation`
  になる状態を目標にする

5. reduced `3x3` solve

- center と edge group が揃った `5x5` から outer layer を抜き出して `3x3` に縮小する
- その reduced `3x3` を上の two-phase solver に渡す
- 最後に move 列を簡約して出力する

## Complexity

入力長だけで見ると、`3x3` は 54 文字、`5x5` は 150 文字で固定なので、厳密にはどちらも `O(1)` です。ただしそれでは探索の重さが見えないので、ここでは探索木の深さと展開ノード数で書きます。

記号:

- `b`: 分岐数
- `d`: 解までの探索深さ
- `N`: A* が実際に展開したノード数
- `M`: move / generator 数

主要フェーズのオーダーは次のとおりです。

| フェーズ | 手法 | 時間計算量 | 空間計算量 | 補足 |
| --- | --- | --- | --- | --- |
| move engine | slot permutation 適用 | `O(S)` | `O(S)` | `S` は sticker 数。`3x3` で 54、`5x5` で 150 |
| `3x3` phase 1 | pruning table 付き深さ優先探索 | `O(b_1^{d_1})` | `O(d_1)` | table 自体は固定サイズ |
| `3x3` phase 2 | pruning table 付き深さ優先探索 | `O(b_2^{d_2})` | `O(d_2)` | table 自体は固定サイズ |
| `5x5` center reduction | IDA* | `O(b_c^{d_c})` | `O(d_c)` | heuristic は orbit ごとの pattern DB |
| `5x5` edge reduction | A* | `O(N_e log N_e)` | `O(N_e)` | priority queue と visited を使う |
| move 簡約 | 線形走査 | `O(L)` | `O(L)` | `L` は出力手数 |

この実装で固定テーブルとして持っている状態数は次です。

- `3x3` two-phase の pruning / move table は固定サイズ
- center reduction の pattern DB は各 orbit・各色ごとに `C(24, 4) = 10626` 状態
- edge reduction の pair heuristic は
  `12 * 2 * C(24, 2) * 2^2 = 26496`
  状態

したがって実用上の支配項は table 構築後の探索で、

- `3x3` は `phase 1 + phase 2` の指数探索
- `5x5` center は IDA* の指数探索
- `5x5` edge は A* の展開ノード数

が支配的です。

特に現在のボトルネックは `5x5` edge reduction で、長い scramble では `N_e` が大きくなりやすく、ここが実行時間の大半を占めます。

## Usage

盤面を直接渡す:

```bash
./build/cube_solver --state UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB
```

move 列から盤面を作ってそのまま解く:

```bash
./build/cube_solver --moves "R U R' U'" --size 3
./build/cube_solver --moves "R U F2" --size 5
```

self-test:

```bash
./build/cube_solver --self-test
```

標準入力も使えます:

```bash
printf '%s\n' 'UUUUUUU UURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB' | tr -d ' ' | ./build/cube_solver
```

## Notes

- `src/main.cpp` に move engine, cubie 変換, two-phase 3x3 solver, reduced-5x5 bridge をまとめています
- `5x5` の native center reduction は入っています
- `5x5` の native edge reduction も入っています
- 現状のボトルネックは long scramble に対する edge reduction 探索の重さです
