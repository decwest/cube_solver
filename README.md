# cube_solver

5x5 ルービックキューブ用の C++ CLI です。

この実装は「入力が盤面そのものではなく、崩し手順列で与えられる」前提で動きます。手順列を逆順にして各手を反転することで、厳密に元の完成状態へ戻す解を出力します。100 手程度のスクランブルなら、解生成は通常 1 秒を大きく下回ります。

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Usage

標準入力にスクランブル列を渡します。

```bash
echo "Rw U2 F' 3Rw Rw' D" | ./build/cube_solver
```

`--verify` を付けると、内部 5x5 シミュレータで `scramble + solution` が solved に戻ることを確認します。

```bash
echo "Rw U2 F' 3Rw Rw' D" | ./build/cube_solver --verify
```

ランダム 100 手スクランブルでの簡易ベンチマーク:

```bash
./build/cube_solver --benchmark 1000
```

## Supported Notation

- `R U F B L D`
- `R' U2`
- `Rw Uw Fw`
- `3Rw 3Uw2`
- `r u f` (`Rw Uw Fw` として解釈)
- `x y z`

注意:

- `3R` のような入力は `3Rw` の省略形として扱います。
- 一般の「盤面状態から探索して解く 5x5 ソルバ」ではありません。入力が崩し手順列である場合に限り、正確かつ高速に解けます。
