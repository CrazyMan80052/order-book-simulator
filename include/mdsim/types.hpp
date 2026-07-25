#include <cstdint>

enum class Side {
    BUY,
    SELL
};

struct Order {
    uint64_t id;
    int64_t price_ticks; // price in ticks 1 tick = 0.01$
    uint32_t quantity;
    Side side;
};