////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#ifndef JDRADIO_ARCAL_HPP
#define JDRADIO_ARCAL_HPP

#include "Device.hpp"
#include "FFT.hpp"
#include "DCBlocker.hpp"
#include "Waterfall.hpp"
#include <string>
#include <vector>
#include <array>
#include <utility>
#include <chrono>
#include <set>

class ARCAL
{
public:
    ARCAL(void) noexcept;

    void showBasicInfo(void) noexcept;
    void showDeviceInfo(void) noexcept;
    void run(void) noexcept;
    void onSamples(std::vector<std::uint8_t>&& in);

private:
    std::vector<float> convertSamples(std::vector<std::uint8_t> const& in, bool block_dc);
    float calculateDCOffset(std::vector<std::uint8_t> const& in);
    void click(void);
    void detectClicks(std::vector<float> const& fft_samples);
    void verifyClicks(void);
    void onRemoteActivation(void);

    Device dev_;
    DCBlocker dc_blocker_;
    Waterfall waterfall_;
    std::pair<bool, float> dc_offset_;
    bool filter_dc_;
    unsigned int frequency_;
    unsigned int sample_rate_;
    bool agc_enabled_;
    float rf_gain_;
    bool signal_present_;
    std::set<std::chrono::steady_clock::time_point> clicks_;
    FFT fft_;
    bool show_waterfall_;
};

#endif
