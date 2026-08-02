#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

python3 tools/run_benchmark.py --root "$ROOT_DIR" --events 100000

printf 'benchmark runner complete\n'
