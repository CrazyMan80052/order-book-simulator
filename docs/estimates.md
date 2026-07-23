A conservative benchmark target on WSL for this project would be something like:

50K to 200K market events/sec replay throughput
<10 ms p99 end-to-end processing latency for individual events
1M+ order book updates in a deterministic replay benchmark
2x to 5x speedups from reasonable optimizations (data structures, allocations, parsing)

Resume bullets (same approximate length as your example):