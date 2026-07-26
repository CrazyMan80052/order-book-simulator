#include "mdsim/fee_model.hpp"

#include <limits>
#include <stdexcept>

namespace mdsim {
namespace {

__extension__ using Int128 = __int128;

Money checked_money(Int128 value) {
    if (value < std::numeric_limits<Money>::min() || value > std::numeric_limits<Money>::max()) {
        throw std::overflow_error("fee overflow");
    }
    return static_cast<Money>(value);
}

}  // namespace

Money NoFeeModel::fee_for_notional(Money) const {
    return 0;
}

BasisPointFeeModel::BasisPointFeeModel(uint32_t fee_bps) : fee_bps_(fee_bps) {}

uint32_t BasisPointFeeModel::fee_bps() const {
    return fee_bps_;
}

Money BasisPointFeeModel::fee_for_notional(Money gross_notional) const {
    const Int128 notional = gross_notional < 0 ? -static_cast<Int128>(gross_notional)
                                               : static_cast<Int128>(gross_notional);
    const Int128 rounded = (notional * static_cast<Int128>(fee_bps_) + 9'999) / 10'000;
    return checked_money(rounded);
}

}  // namespace mdsim