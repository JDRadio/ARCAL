////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#include "DCBlocker.hpp"

DCBlocker::DCBlocker(void) noexcept :
    r_{0.998f},
    xi_{0.f},
    xq_{0.f},
    yi_{0.f},
    yq_{0.f}
{
}

void DCBlocker::execute(float& i, float& q) noexcept
{
    yi_ = i - xi_ + r_ * yi_;
    xi_ = i;
    i = yi_;

    yq_ = q - xq_ + r_ * yq_;
    xq_ = q;
    q = yq_;
}
