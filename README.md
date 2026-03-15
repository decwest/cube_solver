# cube_solver

5x5 ルービックキューブの実盤面状態を入力として解く CLI です。

実際の探索は `dwalton76/rubiks-cube-NxNxN-solver` を backend として使い、C++ 側は入力検証と起動制御を担当します。純粋な C++ 単独実装ではありませんが、任意盤面から解く reduction solver として動作します。

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Backend Setup

初回は backend の clone と C helper の build が必要です。

```bash
./build/cube_solver --setup-backend
```

`--setup-backend` を省略しても、backend が無ければ自動でセットアップします。

注意:

- 初回の「実際の solve」時には lookup table が追加でダウンロードされます。
- lookup table の初回取得は時間がかかります。
- warm cache 後の速度は環境依存です。平均 1 秒を常に保証するものではありません。

## Input Format

入力は 150 文字の 5x5 state です。

- 面の並び順は `URFDLB`
- 各面 25 文字
- `URFDLB` 表記をそのまま受け付けます
- 標準色 `W R G Y O B` も `U R F D L B` に正規化して受け付けます

例: solved state

```text
UUUUUUUUUUUUUUUUUUUUUUUUURRRRRRRRRRRRRRRRRRRRRRRRRFFFFFFFFFFFFFFFFFFFFFFFFFDDDDDDDDDDDDDDDDDDDDDDDDDLLLLLLLLLLLLLLLLLLLLLLLLLBBBBBBBBBBBBBBBBBBBBBBBBB
```

## Usage

標準入力:

```bash
printf '%s\n' 'UUUUUUUUUUUUUUUUUUUUUUUUURRRRRRRRRRRRRRRRRRRRRRRRRFFFFFFFFFFFFFFFFFFFFFFFFFDDDDDDDDDDDDDDDDDDDDDDDDDLLLLLLLLLLLLLLLLLLLLLLLLLBBBBBBBBBBBBBBBBBBBBBBBBB' | ./build/cube_solver
```

引数で渡す:

```bash
./build/cube_solver --state UUUUUUUUUUUUUUUUUUUUUUUUURRRRRRRRRRRRRRRRRRRRRRRRRFFFFFFFFFFFFFFFFFFFFFFFFFDDDDDDDDDDDDDDDDDDDDDDDDDLLLLLLLLLLLLLLLLLLLLLLLLLBBBBBBBBBBBBBBBBBBBBBBBBB
```

## Files

- `src/main.cpp`: C++ frontend
- `scripts/setup_backend.sh`: backend clone/build
- `tools/solve_5x5_state.py`: backend bridge

## Notes

- backend commit は `rubiks-cube-NxNxN-solver@c776db79314db3d98cc3dd99685ca85766656937`
- 3x3 phase solver は `kociemba@e1959d275e59c845ab63a8d29a3f9b3835d54eea`
