////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#include "Waterfall.hpp"
#include <iostream>
#include <sstream>
#include <numeric>
#include <functional>
#include <fmt/format.h>

template<class ForwardIt>
ForwardIt max_element(ForwardIt first, ForwardIt last)
{
    if (first == last) return last;
    ForwardIt largest = first;
    ++first;
    for (; first != last; ++first) {
        if (*largest < *first) {
            largest = first;
        }
    }
    return largest;
}

template<class T>
constexpr const T& clamp( const T& v, const T& lo, const T& hi )
{
    if (v <= lo) { return lo; }
    if (v >= hi) { return hi; }
    return v;
}

Waterfall::Waterfall(void) noexcept :
    fft_{},
    fft_length_{},
    average_length_{},
    fft_count_{0},
    averages_{},
    reference_level_{},
    scale_{},
    show_timestamp_{true},
    show_max_power_{true},
    show_total_power_{true},
    show_timestamp_every_n_seconds_{5},
    last_timestamp_{0}
{
    setFFTLength(256);
    setAverageLength(64);
    setReferenceLevel(-80);
    setScale(10);
}

void Waterfall::setFFTLength(unsigned int len)
{
    fft_length_ = len;

    fft_.setLength(fft_length_);

    averages_.clear();
    averages_.resize(fft_length_);

    fft_count_ = 0;
}

void Waterfall::setAverageLength(unsigned int len)
{
    average_length_ = len;

    averages_.clear();
    averages_.resize(fft_length_);

    fft_count_ = 0;
}

void Waterfall::setReferenceLevel(float ref)
{
    reference_level_ = ref;
}

void Waterfall::setScale(float scale)
{
    scale_ = scale;
}

unsigned int Waterfall::mapPowerLevel(float lvl, float in_min, float in_max, unsigned int out_min, unsigned int out_max)
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

char Waterfall::getWeightCharacter(float val)
{
    static char const* greyscale_lo_hi = " .:-=+*#%@";

    return greyscale_lo_hi[mapPowerLevel(clamp(val, 0.f, scale_ * 9), 0.f, scale_ * 9, 0, 9)];
}

int Waterfall::getWeightColor(float val)
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

    return color_lo_hi[mapPowerLevel(clamp(val, 0.f, scale_ * 9), 0.f, scale_ * 9, 0, 9)];
}

std::string Waterfall::getWeightColorString(float val)
{
    return fmt::format("\033[0;{}m{}", getWeightColor(val), getWeightCharacter(val));
}

void Waterfall::pushToAverage(unsigned int index, float val)
{
    auto& vec = averages_[index];
    vec.push_back(val);
}

void Waterfall::calculateFFT(std::vector<float> const& samples)
{
    auto spec = fft_.execute(samples);
    unsigned int const spec_size = spec.size();

    for (unsigned int i = 0; i < spec_size; i += 2) {
        if (i % (fft_length_ * 2) == 0) {
            ++fft_count_;
        }

        pushToAverage((i/2) % fft_length_, spec[i]*spec[i] + spec[i+1]*spec[i+1]);
    }
}

void Waterfall::displayFFT(void)
{
    while (fft_count_ >= average_length_) {
        fft_count_ -= average_length_;

        std::stringstream sb;

        std::vector<float> bin_power(fft_length_);

        for (unsigned int i = 0; i < fft_length_; ++i) {
            auto& vec = averages_[i];
            float pwr = std::accumulate(std::begin(vec), std::begin(vec) + average_length_, 0.f) / static_cast<float>(average_length_);
            bin_power[i] = pwr;
            vec.erase(std::begin(vec), std::begin(vec) + average_length_);
            sb << getWeightColorString(10.f * std::log10(pwr) - reference_level_);
        }

        if (show_timestamp_) {
            auto now = std::time(nullptr);

            if (now - last_timestamp_ >= show_timestamp_every_n_seconds_) {
                last_timestamp_ = now;
                std::tm* cur_time = std::gmtime(&now);
                std::cout << fmt::format(
                    "\033[0;0m[{:02}:{:02}:{:02}]    ",
                    cur_time->tm_hour,
                    cur_time->tm_min,
                    cur_time->tm_sec
                );
            }
            else {
                std::cout << "              ";
            }
        }

        std::cout << sb.str();

        if (show_max_power_) {
            float max_power = 10.f * std::log10(*max_element(std::begin(bin_power), std::end(bin_power)));
            std::cout << fmt::format("    \033[0;0mMax: \033[0;{}m{:+0.4f}", getWeightColor(max_power - reference_level_), max_power);
        }

        if (show_total_power_) {
            float total_power = 10.f * std::log10(std::accumulate(std::begin(bin_power), std::end(bin_power), 0.f));
            std::cout << fmt::format("    \033[0;0mTotal: \033[0;{}m{:+0.4f}", getWeightColor(total_power - reference_level_), total_power);
        }

        std::cout << std::endl;
    }
}

void Waterfall::onSamples(std::vector<float> const& samples)
{
    calculateFFT(samples);
    displayFFT();
}
