#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
import time
from datetime import datetime, timezone


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate and replay a deterministic benchmark workload")
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--events", type=int, default=100_000)
    parser.add_argument("--output-dir", type=pathlib.Path, default=None)
    args = parser.parse_args()

    root_dir = args.root.resolve()
    output_dir = (args.output_dir or root_dir / "benchmarks" / "workloads").resolve()
    result_dir = root_dir / "results" / "benchmarks"
    output_dir.mkdir(parents=True, exist_ok=True)
    result_dir.mkdir(parents=True, exist_ok=True)

    workload_path = output_dir / "100k-events.ndjson"
    summary_path = result_dir / "100k-summary.json"
    books_path = result_dir / "100k-books.json"
    report_path = result_dir / "100k-benchmark.json"

    subprocess.run(
        [sys.executable, str(root_dir / "tools" / "generate_benchmark_workload.py"), "--output", str(workload_path), "--events", str(args.events)],
        cwd=root_dir,
        check=True,
    )

    start = time.perf_counter()
    subprocess.run(
        [
            str(root_dir / "build" / "dev" / "mdsim"),
            "replay",
            "--input",
            str(workload_path),
            "--summary",
            str(summary_path),
            "--final-books",
            str(books_path),
        ],
        cwd=root_dir,
        check=True,
    )
    elapsed_seconds = time.perf_counter() - start

    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    workload_bytes = workload_path.read_bytes()
    line_count = sum(1 for _ in workload_path.open("r", encoding="utf-8"))

    report = {
        "generated_at_utc": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "command": "replay",
        "workload": {
            "path": str(workload_path),
            "event_count": line_count,
            "size_bytes": workload_path.stat().st_size,
            "sha256": hashlib.sha256(workload_bytes).hexdigest(),
        },
        "timing": {
            "wall_clock_seconds": round(elapsed_seconds, 6),
            "events_per_second": round(line_count / elapsed_seconds, 2) if elapsed_seconds > 0 else 0.0,
        },
        "summary": summary,
        "outputs": {
            "summary_path": str(summary_path),
            "books_path": str(books_path),
        },
    }
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
