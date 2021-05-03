////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#ifndef JDRADIO_DCBLOCKER_HPP
#define JDRADIO_DCBLOCKER_HPP

class DCBlocker
{
public:
    DCBlocker(void) noexcept;
    void execute(float& i, float& q) noexcept;

private:
    float r_;
    float xi_;
    float xq_;
    float yi_;
    float yq_;
};

#endif
