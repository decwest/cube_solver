#!/usr/bin/env python3

import argparse
import logging
import os
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--backend-dir", required=True)
    parser.add_argument("--kociemba-bin", required=True)
    parser.add_argument("--state", required=True)
    parser.add_argument("--order", default="URFDLB")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    backend_dir = Path(args.backend_dir).resolve()
    kociemba_bin = Path(args.kociemba_bin).resolve()

    if not backend_dir.exists():
        print(f"backend directory not found: {backend_dir}", file=sys.stderr)
        return 1
    if not kociemba_bin.exists():
        print(f"kociemba binary not found: {kociemba_bin}", file=sys.stderr)
        return 1

    os.chdir(backend_dir)
    os.environ["PATH"] = str(kociemba_bin.parent) + os.pathsep + os.environ.get("PATH", "")
    sys.path.insert(0, str(backend_dir))

    from rubikscubennnsolver import SolveError, configure_logging
    from rubikscubennnsolver.RubiksCube555 import RubiksCube555

    configure_logging(logging.ERROR)

    try:
        cube = RubiksCube555(args.state, args.order)
        cube.sanity_check()
        cube.solve()
    except SolveError as exc:
        print(f"solver error: {exc}", file=sys.stderr)
        return 2
    except Exception as exc:
        print(f"unexpected solver error: {exc}", file=sys.stderr)
        return 3

    solution = [step for step in cube.solution if not step.startswith("COMMENT")]
    print(" ".join(solution))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
