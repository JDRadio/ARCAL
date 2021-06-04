////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#include "Device.hpp"
#include <iostream>

Device::Device(void) noexcept :
    mutex_{},
    dev_{nullptr},
    handler_{nullptr}
{
}

Device::Device(Device&& other) noexcept :
    mutex_{},
    dev_{nullptr},
    handler_{nullptr}
{
    std::unique_lock<std::mutex> our_lock{mutex_, std::defer_lock};
    std::unique_lock<std::mutex> other_lock{other.mutex_, std::defer_lock};

    std::lock(our_lock, other_lock);

    dev_ = std::move(other.dev_);
    handler_ = std::move(other.handler_);

    other.dev_ = nullptr;
    other.handler_ = nullptr;
}

Device& Device::operator=(Device&& other) noexcept
{
    std::unique_lock<std::mutex> our_lock{mutex_, std::defer_lock};
    std::unique_lock<std::mutex> other_lock{other.mutex_, std::defer_lock};

    std::lock(our_lock, other_lock);

    dev_ = std::move(other.dev_);
    handler_ = std::move(other.handler_);

    other.dev_ = nullptr;
    other.handler_ = nullptr;

    return *this;
}

Device::Device(unsigned int index) :
    mutex_{},
    dev_{nullptr},
    handler_{nullptr}
{
    int result = rtlsdr_open(&dev_, index);

    if (result < 0) {
        throw OpenException{result};
    }
}

Device::~Device(void) noexcept
{
    if (dev_) {
        rtlsdr_close(dev_);
    }
}

std::vector<std::tuple<unsigned int, std::string, std::string, std::string, std::string>> Device::listDevices(void) noexcept
{
    auto count = rtlsdr_get_device_count();

    if (count == 0) {
        return {};
    }

    decltype(listDevices()) out;

    for (unsigned int i = 0; i < count; ++i) {
        auto name = rtlsdr_get_device_name(i);

        char manufact[256] = {0};
        char product[256] = {0};
        char serial[256] = {0};

        if (rtlsdr_get_device_usb_strings(i, manufact, product, serial) < 0) {
            out.push_back(std::make_tuple(i, name, std::string{}, std::string{}, std::string{}));
        }
        else {
            out.push_back(std::make_tuple(i, name, manufact, product, serial));
        }
    }

    return out;
}

bool Device::setCenterFrequency(unsigned int freq) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (! dev_) {
        return false;
    }

    return rtlsdr_set_center_freq(dev_, freq) >= 0;
}

bool Device::setSampleRate(unsigned int rate) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (! dev_) {
        return false;
    }

    return rtlsdr_set_sample_rate(dev_, rate) >= 0;
}

bool Device::readSync(std::vector<std::uint8_t>& out) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (! dev_) {
        return false;
    }

    int num_read = 0;
    int result = rtlsdr_read_sync(dev_, out.data(), out.size(), &num_read);

    std::cout << result << std::endl;

    out.resize(num_read);

    return result == 0;
}

bool Device::resetBuffer(void) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (! dev_) {
        return false;
    }

    int result = rtlsdr_reset_buffer(dev_);

    if (result < 0) {
        std::cerr << result << std::endl;
    }

    return result == 0;
}

bool Device::setAgcMode(bool on) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (! dev_) {
        return false;
    }

    int result = rtlsdr_set_agc_mode(dev_, on ? 1 : 0);

    if (result < 0) {
        std::cerr << result << std::endl;
    }

    return result == 0;
}

bool Device::setGain(float gain) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (! dev_) {
        return false;
    }

    int result = rtlsdr_set_tuner_gain(dev_, static_cast<int>(gain * 10.0f + 0.5f));

    if (result < 0) {
        std::cerr << result << std::endl;
    }

    return result == 0;
}

std::pair<bool, std::vector<float>> Device::listGains(void) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (! dev_) {
        return std::make_pair(false, std::vector<float>{});
    }

    int num_gains = rtlsdr_get_tuner_gains(dev_, nullptr);

    if (num_gains < 0) {
        std::cerr << num_gains << std::endl;
        return std::make_pair(false, std::vector<float>{});
    }

    std::vector<int> gains(num_gains);
    int result = rtlsdr_get_tuner_gains(dev_, gains.data());

    if (result < 0) {
        std::cerr << result << std::endl;
        return std::make_pair(false, std::vector<float>{});
    }

    std::vector<float> out;
    for (auto const& x : gains) {
        out.push_back(static_cast<float>(x) / 10.0f);
    }

    return std::make_pair(true, out);
}

bool Device::readAsync(std::function<void(std::vector<std::uint8_t>&&)> handler) noexcept
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (! dev_) {
        return false;
    }

    handler_ = handler;

    int result = rtlsdr_read_async(dev_, &Device::callback, this, 0, 0);

    if (result < 0) {
        std::cerr << result << std::endl;
    }

    return result == 0;
}

bool Device::cancelAsync(void) noexcept
{
    if (! dev_) {
        return false;
    }

    int result = rtlsdr_cancel_async(dev_);

    if (result < 0) {
        std::cerr << result << std::endl;
    }

    return result == 0;
}

void Device::callback(std::uint8_t* buf, std::uint32_t len, void* ctx)
{
    if (! buf || ! len || ! ctx) {
        return;
    }

    auto dev = reinterpret_cast<Device*>(ctx);

    if (dev->handler_) {
        dev->handler_(std::vector<std::uint8_t>(buf, buf + len));
    }
}
