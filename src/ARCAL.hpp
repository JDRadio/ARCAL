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
#include <string>
#include <vector>
#include <array>

class ARCAL
{
public:
    ARCAL(void) noexcept;

    void showBasicInfo(void) noexcept;
    void showDeviceInfo(void) noexcept;
    void run(void) noexcept;
    unsigned int mapPowerLevel(float lvl, float in_min, float in_max, unsigned int out_min, unsigned int out_max);
    char getWeightCharacter(float val);
    int getWeightColor(float val);
    std::string getWeightColorString(float val);
    void pushToAverage(unsigned int index, float val);
    std::vector<float> convertSamples(std::vector<std::uint8_t> const& in, bool block_dc);
    void calculateFFT(std::vector<float> const& samples);
    void displayFFT(void);
    void onSamples(std::vector<std::uint8_t>&& in);

private:
    Device dev_;
    FFT fft_;
    DCBlocker dc_blocker_;
    static constexpr unsigned int FFT_SIZE = 256;
    static constexpr unsigned int NUM_SAMPLES_PER_AVERAGE = 100;
    unsigned int fft_count_;
    std::array<std::vector<float>, FFT_SIZE> averages_;
    std::vector<float> noise_floor_;
    unsigned int noise_floor_head_;
};

#endif
