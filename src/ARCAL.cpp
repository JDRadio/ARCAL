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
#include <thread>
#include <chrono>
#include <wiringPi.h>

ARCAL::ARCAL(void) noexcept :
    dev_{},
    dc_blocker_{},
    waterfall_{},
    dc_offset_{std::make_pair(false, 0)},
    filter_dc_{false},
    frequency_{118'025'000U},
    sample_rate_{256'000U},
    agc_enabled_{false},
    rf_gain_{0.f},
    signal_present_{false},
    clicks_{},
    fft_{},
    show_waterfall_{true},
    task_{}
{
    fft_.setLength(32);

    wiringPiSetup();
    pinMode(0, OUTPUT);
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

    if (! std::get<bool>(gains)) {
        std::cerr << "Failed to list available gains" << std::endl;
    }
    else {
        std::cout << "Available gain values: ";
        for (unsigned int n = 0; n < std::get<1>(gains).size(); ++n) {
            if (n > 0) {
                std::cout << ", ";
            }
            std::cout << std::get<1>(gains)[n] << std::endl;
        }
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
    std::cout << fmt::format("DC Compensation: {}", ! std::get<0>(dc_offset_) ? "ON" : "OFF") << std::endl;
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

    if (! dev_.readAsync([this] (auto&& buffer) { this->onSamples(std::move(buffer)); })) {
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

    float const offset_value = 127.5f + std::get<1>(dc_offset_);

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

    task_ = std::async(
        std::launch::async,
        [] {
            digitalWrite(0, 1);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            digitalWrite(0, 0);
        }
    );
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

void ARCAL::detectClicks(std::vector<float> const& fft_samples)
{
    //! \todo 2021-05-09: add dynamic threshold over noise
    static float const detection_threshold_ = std::pow(10.f, 10.f / 10.f);
    //! \todo 2021-05-09: add hold time
    static unsigned int hold_ = 0;
    //! \todo 2021-05-09: add noise level estimation
    static float const noise_level_ = std::pow(10.f, -58.f / 10.f);
    //! \todo 2021-05-09: consider a click as a transmission of not more than X milliseconds
    static unsigned int on_time_ = 0;

    unsigned int const fft_size = 32;
    unsigned int const num_fft = fft_samples.size() / (fft_size * 2);

    for (unsigned int k = 0; k < num_fft; ++k) {
        auto const* ptr = fft_samples.data() + k * (fft_size * 2);

        float power = 0;

        for (unsigned int bin = 15; bin <= 17; ++bin) {
            power += ptr[bin*2]*ptr[bin*2] + ptr[bin*2+1]*ptr[bin*2+1];
        }

        bool signal_detected = (power >= noise_level_ * detection_threshold_);

        if (! signal_detected) {
            if (hold_ > 0) {
                --hold_;
                signal_detected = true;
            }
        }
        else {
            ++on_time_;
            // hold_ = 480;   // 2 ms @ 240 kHz
            hold_ = 80;   // 10 ms @ 8 kHz
        }

        if (! signal_detected && signal_present_) {
            std::cout << fmt::format(
                "Signal lost, duration: {:.1f} ms / {} samples",
                on_time_ * 1.f / 8.f,
                on_time_
            ) << std::endl;

            if (on_time_ >= 160) { // 20 ms @ 8 kHz
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
    if (! std::get<0>(dc_offset_)) {
        std::get<1>(dc_offset_) = calculateDCOffset(in);
        std::get<0>(dc_offset_) = true;
    }

    auto samples = convertSamples(in, filter_dc_);
    auto fft_bins = fft_.execute(samples);

    detectClicks(fft_bins);

    if (show_waterfall_) {
        waterfall_.onSamples(samples);
    }
}
