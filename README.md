# cube_solver

外部ライブラリに依存しない native 実装です。

現状の対応範囲は次のとおりです。

- 任意の `3x3` 盤面状態を native two-phase solver で解く
- `5x5` のうち、すでに reduction 済みの盤面を `3x3` に落として解く
- `5x5` の centers / wing pairing は未実装

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

`5x5` は reduced state のみ受け付けます。

- 各面の 3x3 center block が揃っていること
- 12 本の edge group が wing pairing 済みであること

満たさない場合は `native 5x5 reduction phases (centers / wings) are not implemented yet` を返します。

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
- `5x5` を任意盤面から最後まで reduction する実装はまだ入っていません
