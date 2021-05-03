////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#include "ARCAL.hpp"
#include <iostream>
#include <sstream>
#include <numeric>
#include <fmt/format.h>

ARCAL::ARCAL(void) noexcept :
    dev_{},
    fft_{}
{
    fft_.setLength(FFT_SIZE);
}

void ARCAL::showBasicInfo(void) noexcept
{
    auto devices = Device::listDevices();
    auto const count = devices.size();

    std::cout << fmt::format("There {} {} device{} available", count == 1 ? "is" : "are", count, count == 1 ? "" : "s") << std::endl;

    if (count == 0) {
        return;
    }

    for (auto const& device : devices) {
        std::cout << fmt::format("Device #{}:", std::get<0>(device)) << std::endl;
        std::cout << fmt::format("  Name: {}", std::get<1>(device)) << std::endl;
        std::cout << fmt::format("  Manufacturer: {}", std::get<2>(device)) << std::endl;
        std::cout << fmt::format("  Product: {}", std::get<3>(device)) << std::endl;
        std::cout << fmt::format("  Serial: {}", std::get<4>(device)) << std::endl;
    }
}

void ARCAL::showDeviceInfo(void) noexcept
{
    auto gains = dev_.listGains();

    if (! gains) {
        std::cerr << "Failed to list available gains" << std::endl;
    }
    else {
        std::cout << fmt::format("Available gain values: {}", fmt::join(gains.value(), ", ")) << std::endl;
    }
}

void ARCAL::run(void) noexcept
{
    showBasicInfo();

    try {
        dev_ = Device{0};
    }
    catch (Device::Exception const& ex) {
        std::cerr << fmt::format("Error {}: {}", ex.code(), ex.what()) << std::endl;
        return;
    }

    showDeviceInfo();

    if (! dev_.setCenterFrequency(146'430'000U)) {
        std::cerr << "Failed to set center frequency" << std::endl;
        return;
    }

    if (! dev_.setSampleRate(1'024'000U)) {
        std::cerr << "Failed to set sample rate" << std::endl;
        return;
    }

    if (! dev_.setAgcMode(false)) {
        std::cerr << "Failed to set AGC" << std::endl;
    }

    if (! dev_.setGain(28.f)) {
        std::cerr << "Failed to set gain" << std::endl;
    }

    if (! dev_.resetBuffer()) {
        std::cerr << "Failed to reset buffer" << std::endl;
        return;
    }

    if (! dev_.readAsync([this] (auto&& buffer) { onSamples(std::move(buffer)); })) {
        std::cerr << "Failed to start reading samples" << std::endl;
        return;
    }
}

unsigned int ARCAL::mapPowerLevel(float lvl, float in_min, float in_max, unsigned int out_min, unsigned int out_max)
{
    if (lvl >= in_max) {
        return out_max;
    }

    if (lvl <= in_min) {
        return out_min;
    }

    float r = in_max - in_min;
    float x = lvl - in_min;

    return (out_max - out_min) * x / r + out_min;
}

char ARCAL::getWeightCharacter(float val)
{
    static char const* greyscale_lo_hi = " .:-=+*#%@";

    return greyscale_lo_hi[mapPowerLevel(val, 0.f, 90.f, 0, 9)];
}

int ARCAL::getWeightColor(float val)
{
    static int color_lo_hi[] = {
        34,
        34,
        34,
        34,
        32,
        32,
        33,
        33,
        31,
        31
    };

    return color_lo_hi[mapPowerLevel(val, 0.f, 90.f, 0, 9)];
}

std::string ARCAL::getWeightColorString(float val)
{
    return fmt::format("\033[0;{}m{}", getWeightColor(val), getWeightCharacter(val));
}

void ARCAL::pushToAverage(unsigned int index, float val)
{
    auto& vec = averages_[index];
    vec.push_back(val);
}

std::vector<float> ARCAL::convertSamples(std::vector<std::uint8_t> const& in, bool block_dc)
{
    unsigned int const in_size = in.size();
    std::vector<float> samples(in_size);

    for (unsigned int n = 0; n < in_size; n += 2) {
        float i = in[n] * 1.f/256.f - 1.f;
        float q = in[n+1] * 1.f/256.f - 1.f;

        if (block_dc) {
            dc_blocker_.execute(i, q);
        }

        samples[n] = i;
        samples[n+1] = q;
    }

    return samples;
}

void ARCAL::calculateFFT(std::vector<float> const& samples)
{
    auto spec = fft_.execute(samples);
    unsigned int const spec_size = spec.size();

    for (unsigned int i = 0; i < spec_size; i += 2) {
        if (i % (FFT_SIZE * 2) == 0) {
            ++fft_count_;
        }

        pushToAverage((i/2) % FFT_SIZE, spec[i]*spec[i] + spec[i+1]*spec[i+1]);
    }
}

void ARCAL::displayFFT(void)
{
    float const ref_level = -50.f;

    while (fft_count_ >= NUM_SAMPLES_PER_AVERAGE) {
        fft_count_ -= NUM_SAMPLES_PER_AVERAGE;

        std::stringstream sb;

        float total_power = 0;

        for (unsigned int i = 0; i < FFT_SIZE; ++i) {
            auto& vec = averages_[i];
            float pwr = std::accumulate(std::begin(vec), std::begin(vec) + NUM_SAMPLES_PER_AVERAGE, 0.f) / NUM_SAMPLES_PER_AVERAGE;
            total_power += pwr;
            vec.erase(std::begin(vec), std::begin(vec) + NUM_SAMPLES_PER_AVERAGE);
            sb << getWeightColorString(10 * std::log10(pwr) - ref_level);
        }

        total_power = 10 * std::log10(total_power);
        std::cout << fmt::format("{}  \033[0;{}m{:.4f}", sb.str(), getWeightColor(total_power - ref_level), total_power) << std::endl;
    }
}

void ARCAL::onSamples(std::vector<std::uint8_t>&& in)
{
    auto samples = convertSamples(in, true);

    calculateFFT(samples);
    displayFFT();
}
