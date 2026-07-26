#pragma once

#include <cstdint>

#include "mdsim/types.hpp"

namespace mdsim {

class FeeModel {
public:
    virtual ~FeeModel() = default;

    virtual Money fee_for_notional(Money gross_notional) const = 0;
};

class NoFeeModel final : public FeeModel {
public:
    Money fee_for_notional(Money gross_notional) const override;
};

class BasisPointFeeModel final : public FeeModel {
public:
    explicit BasisPointFeeModel(uint32_t fee_bps);

    uint32_t fee_bps() const;
    Money fee_for_notional(Money gross_notional) const override;

private:
    uint32_t fee_bps_;
};

}  // namespace mdsim