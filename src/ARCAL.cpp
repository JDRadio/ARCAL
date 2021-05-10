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
    dc_blocker_{},
    waterfall_{},
    dc_offset_{},
    filter_dc_{false},
    frequency_{146'430'000U},
    sample_rate_{240'000U},
    agc_enabled_{false},
    rf_gain_{0.f},
    signal_present_{false},
    clicks_{}
{
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

    std::cout << std::endl;
    std::cout << fmt::format("Frequency:       {:.3f} MHz", frequency_ / 1e6) << std::endl;
    std::cout << fmt::format("Sample Rate:     {:.3f} Ksps", sample_rate_ / 1e3) << std::endl;
    std::cout << fmt::format("Hardware AGC:    {}", agc_enabled_ ? "ON" : "OFF") << std::endl;
    std::cout << fmt::format("Hardware Gain:   {:.1f} dB", rf_gain_) << std::endl;
    std::cout << fmt::format("DC Compensation: {}", ! dc_offset_ ? "ON" : "OFF") << std::endl;
    std::cout << std::endl;

    if (! dev_.setCenterFrequency(frequency_)) {
        std::cerr << "Failed to set center frequency" << std::endl;
        return;
    }

    if (! dev_.setSampleRate(sample_rate_)) {
        std::cerr << "Failed to set sample rate" << std::endl;
        return;
    }

    if (! dev_.setAgcMode(agc_enabled_)) {
        std::cerr << "Failed to set AGC" << std::endl;
    }

    if (! dev_.setGain(rf_gain_)) {
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

float ARCAL::calculateDCOffset(std::vector<std::uint8_t> const& in)
{
    unsigned int const in_size = in.size();
    std::vector<float> samples(in_size);

    for (unsigned int n = 0; n < in_size; n += 2) {
        // The weird value here is to compensate for DC offset
        samples[n] = (static_cast<float>(in[n]) - 127.5f);
        samples[n+1] = (static_cast<float>(in[n+1]) - 127.5f);
    }

    return std::accumulate(std::begin(samples), std::end(samples), 0.f) / static_cast<float>(in_size);
}

std::vector<float> ARCAL::convertSamples(std::vector<std::uint8_t> const& in, bool block_dc)
{
    unsigned int const in_size = in.size();
    std::vector<float> samples(in_size);

    float const offset_value = 127.5f + dc_offset_.value();

    for (unsigned int n = 0; n < in_size; n += 2) {
        // The weird value here is to compensate for DC offset
        samples[n] = (static_cast<float>(in[n]) - offset_value) * (1.f / 128.f);
        samples[n+1] = (static_cast<float>(in[n+1]) - offset_value) * (1.f / 128.f);
    }

    if (block_dc) {
        for (unsigned int n = 0; n < in_size; n += 2) {
            dc_blocker_.execute(samples[n], samples[n+1]);
        }
    }

    return samples;
}

void ARCAL::onRemoteActivation(void)
{
    std::cout << "\033[1;31mREMOTE ACTIVATION DETECTED!!" << std::endl;
}

void ARCAL::verifyClicks(void)
{
    auto now = std::chrono::steady_clock::now();
    auto it = std::begin(clicks_);

    while (it != std::end(clicks_)) {
        // Remove clicks older than 5 seconds
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *it).count() > 5000) {
            it = clicks_.erase(it);
            continue;
        }

        ++it;
    }

    if (clicks_.size() >= 5) {
        clicks_.clear();
        onRemoteActivation();
    }
}

void ARCAL::click(void)
{
    clicks_.insert(std::chrono::steady_clock::now());
    verifyClicks();
}

void ARCAL::detectClicks(std::vector<float> const& samples)
{
    //! \todo 2021-05-09: add dynamic threshold over noise
    static float const detection_threshold_ = std::pow(10.f, 10.f / 10.f);
    //! \todo 2021-05-09: add hold time
    static unsigned int hold_ = 0;
    //! \todo 2021-05-09: add noise level estimation
    static float const noise_level_ = std::pow(10.f, -39.f / 10.f);
    //! \todo 2021-05-09: consider a click as a transmission of not more than X milliseconds
    static unsigned int on_time_ = 0;

    unsigned int samples_size = samples.size();
    auto const* ptr = samples.data();

    for (unsigned int n = 0; n < samples_size; n += 2) {
        float power = ptr[n]*ptr[n] + ptr[n+1]*ptr[n+1];

        bool signal_detected = (power >= noise_level_ * detection_threshold_);

        if (! signal_detected) {
            if (hold_ > 0) {
                --hold_;
                signal_detected = true;
            }
        }
        else {
            ++on_time_;
            hold_ = 4800;   // 20 ms @ 240 kHz
            // hold_ = 160;   // 20 ms @ 8 kHz
        }

        if (signal_detected && ! signal_present_) {
            std::cout << fmt::format("Incoming signal, power: {:.3f} dBFS", 10.f * std::log10(power)) << std::endl;
        }
        else if (! signal_detected && signal_present_) {
            std::cout << fmt::format(
                "Signal lost, duration: {:.1f} ms",
                on_time_ * 1.f / 240.f  // 240 kHz
                // on_time_ * 1.f / 8.f  // 8 kHz
            ) << std::endl;

            if (on_time_ >= 12000) { // 50 ms @ 240 kHz
            // if (on_time_ >= 400) { // 50 ms @ 8 kHz
                click();
            }
        }

        if (! signal_detected) {
            on_time_ = 0;
        }

        signal_present_ = signal_detected;
    }
}

void ARCAL::onSamples(std::vector<std::uint8_t>&& in)
{
    if (! dc_offset_) {
        dc_offset_ = calculateDCOffset(in);
    }

    auto samples_240khz = convertSamples(in, filter_dc_);

    // auto samples_24khz = decimate_by_10(samples_240khz);
    // auto samples_corrected_24khz = coarse_frequency_correction(samples_24khz);
    // auto samples_8khz = decimate_by_3(samples_24khz);
    // auto signal_detected = detect_power(samples_8khz);
    detectClicks(samples_240khz);

    waterfall_.onSamples(samples_240khz);
}
