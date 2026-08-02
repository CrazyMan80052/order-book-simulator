#!/usr/bin/env python3
import argparse
import json
import random
from pathlib import Path


def format_price(value: float) -> str:
    return f"{value:.3f}"


def generate_workload(output_path: Path, event_count: int) -> None:
    if event_count < 2:
        raise ValueError("event_count must be at least 2")

    rng = random.Random(20260801)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    bids = [
        ("0.480", 20),
        ("0.485", 22),
        ("0.490", 24),
        ("0.495", 26),
        ("0.500", 28),
    ]
    asks = [
        ("0.505", 18),
        ("0.510", 20),
        ("0.515", 22),
        ("0.520", 24),
        ("0.525", 26),
    ]

    current_bids = {price: quantity for price, quantity in bids}
    current_asks = {price: quantity for price, quantity in asks}

    lines = []
    snapshot = {
        "schema_version": 1,
        "source": "generated-benchmark",
        "market_id": "bench-market",
        "sequence": 1,
        "exchange_timestamp_ns": 1000,
        "receive_timestamp_ns": 1001,
        "type": "snapshot",
        "payload": {
            "bids": [{"price": price, "quantity": str(quantity)} for price, quantity in bids],
            "asks": [{"price": price, "quantity": str(quantity)} for price, quantity in asks],
        },
    }
    lines.append(json.dumps(snapshot, separators=(",", ":")))

    for index in range(2, event_count + 1):
        side = "bid" if index % 2 == 0 else "ask"
        if side == "bid":
            price_candidates = [price for price in current_bids.keys()]
            if not price_candidates:
                price_candidates = ["0.495"]
            price = price_candidates[(index + rng.randint(0, 2)) % len(price_candidates)]
            if index % 7 == 0 and price in current_bids:
                new_quantity = 0
                del current_bids[price]
            else:
                current_quantity = current_bids.get(price, 0)
                if current_quantity == 0:
                    new_quantity = 10 + (index % 12)
                    current_bids[price] = new_quantity
                else:
                    new_quantity = max(1, current_quantity + (1 if index % 3 == 0 else -1))
                    current_bids[price] = new_quantity
            change = {
                "side": side,
                "price": price,
                "new_quantity": str(new_quantity),
            }
        else:
            price_candidates = [price for price in current_asks.keys()]
            if not price_candidates:
                price_candidates = ["0.515"]
            price = price_candidates[(index + rng.randint(0, 2)) % len(price_candidates)]
            if index % 5 == 0 and price in current_asks:
                new_quantity = 0
                del current_asks[price]
            else:
                current_quantity = current_asks.get(price, 0)
                if current_quantity == 0:
                    new_quantity = 8 + (index % 10)
                    current_asks[price] = new_quantity
                else:
                    new_quantity = max(1, current_quantity + (1 if index % 4 == 0 else -1))
                    current_asks[price] = new_quantity
            change = {
                "side": side,
                "price": price,
                "new_quantity": str(new_quantity),
            }

        event = {
            "schema_version": 1,
            "source": "generated-benchmark",
            "market_id": "bench-market",
            "sequence": index,
            "exchange_timestamp_ns": 1000 + index,
            "receive_timestamp_ns": 1001 + index,
            "type": "level_update",
            "payload": {"changes": [change]},
        }
        lines.append(json.dumps(event, separators=(",", ":")))

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a deterministic 100K-event benchmark workload")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--events", type=int, default=100_000)
    args = parser.parse_args()

    generate_workload(args.output, args.events)
    print(f"wrote {args.events} events to {args.output}")


if __name__ == "__main__":
    main()
