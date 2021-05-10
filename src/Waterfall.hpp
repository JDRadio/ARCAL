////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#ifndef JDRADIO_WATERFALL_HPP
#define JDRADIO_WATERFALL_HPP

#include "FFT.hpp"
#include <string>
#include <vector>
#include <array>
#include <ctime>

class Waterfall
{
public:
    Waterfall(void) noexcept;

    void onSamples(std::vector<float> const& samples);
    void setFFTLength(unsigned int len);
    void setAverageLength(unsigned int len);
    void setReferenceLevel(float ref);
    void setScale(float scale);

private:
    unsigned int mapPowerLevel(float lvl, float in_min, float in_max, unsigned int out_min, unsigned int out_max);
    char getWeightCharacter(float val);
    int getWeightColor(float val);
    std::string getWeightColorString(float val);
    void pushToAverage(unsigned int index, float val);
    std::vector<float> convertSamples(std::vector<std::uint8_t> const& in, bool block_dc);
    void calculateFFT(std::vector<float> const& samples);
    void displayFFT(void);


    FFT fft_;
    unsigned int fft_length_;
    unsigned int average_length_;
    unsigned int fft_count_;
    std::vector<std::vector<float>> averages_;
    float reference_level_;
    float scale_;
    bool show_timestamp_;
    bool show_max_power_;
    bool show_total_power_;
    unsigned int show_timestamp_every_n_seconds_;
    std::time_t last_timestamp_;
};

#endif
