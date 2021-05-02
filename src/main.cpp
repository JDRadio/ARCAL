#include "rtl-sdr.h"
#include <iostream>
#include <fmt/core.h>
#include <exception>
#include <stdexcept>

class Device
{
public:
    struct Exception : std::exception
    {
        int code_;
        char const* message_;

        Exception(int code, char const* message) noexcept :
            std::exception{},
            code_{code},
            message_{message}
        {
        }

        virtual ~Exception(void) noexcept
        {
        }

        char const* what(void) const noexcept override
        {
            return message_;
        }

        int code(void) const noexcept
        {
            return code_;
        }
    };

    struct OpenException : Exception
    {
        OpenException(int code) noexcept :
            Exception(code, "Failed to open device")
        {
        }
    };

    Device(void) noexcept :
        dev_{nullptr}
    {
    }

    Device(Device&& other) noexcept :
        dev_{other.dev_}
    {
        other.dev_ = nullptr;
    }

    Device& operator=(Device&& other) noexcept
    {
        dev_ = other.dev_;
        other.dev_ = nullptr;

        return *this;
    }

    Device(unsigned int index) :
        dev_{nullptr}
    {
        int result = rtlsdr_open(&dev_, index);

        if (result < 0) {
            throw OpenException{result};
        }
    }

    ~Device(void) noexcept
    {
        if (dev_) {
            rtlsdr_close(dev_);
        }
    }

    static std::vector<std::tuple<unsigned int, std::string, std::string, std::string, std::string>> listDevices(void) noexcept
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

    bool setCenterFrequency(unsigned int freq) noexcept
    {
        if (! dev_) {
            return false;
        }

        return rtlsdr_set_center_freq(dev_, freq) >= 0;
    }

    bool setSampleRate(unsigned int rate) noexcept
    {
        if (! dev_) {
            return false;
        }

        return rtlsdr_set_sample_rate(dev_, rate) >= 0;
    }

    bool readSync(std::vector<std::uint8_t>& out) noexcept
    {
        if (! dev_) {
            return false;
        }

        int num_read = 0;
        int result = rtlsdr_read_sync(dev_, out.data(), out.size(), &num_read);

        std::cout << result << std::endl;

        out.resize(num_read);

        return result == 0;
    }

    bool resetBuffer(void) noexcept
    {
        if (! dev_) {
            return false;
        }

        int result = rtlsdr_reset_buffer(dev_);

        if (result < 0) {
            std::cerr << result << std::endl;
        }

        return result == 0;
    }

    bool readAsync(void) noexcept
    {
        if (! dev_) {
            return false;
        }

        int result = rtlsdr_read_async(dev_, &Device::callback, this, 0, 0);

        if (result < 0) {
            std::cerr << result << std::endl;
        }

        return result == 0;
    }

    bool cancelAsync(void) noexcept
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

    void onCallback(std::vector<std::uint8_t> const& buffer)
    {
        std::cout << fmt::format("Read {} sample{}", buffer.size(), buffer.size() == 1 ? "" : "s") << std::endl;
    }

private:
    static void callback(std::uint8_t* buf, std::uint32_t len, void* ctx)
    {
        if (buf == nullptr || len == 0 || ctx == nullptr) {
            return;
        }

        reinterpret_cast<Device*>(ctx)->onCallback(std::vector<std::uint8_t>(buf, buf + len));
    }

    rtlsdr_dev_t* dev_;
};

int main(int, char**)
{
    auto devices = Device::listDevices();
    auto const count = devices.size();

    std::cout << fmt::format("There {} {} device{} available", count == 1 ? "is" : "are", count, count == 1 ? "" : "s") << std::endl;

    if (count == 0) {
        return 1;
    }

    for (auto const& device : devices) {
        std::cout << fmt::format("Device #{}:", std::get<0>(device)) << std::endl;
        std::cout << fmt::format("  Name: {}", std::get<1>(device)) << std::endl;
        std::cout << fmt::format("  Manufacturer: {}", std::get<2>(device)) << std::endl;
        std::cout << fmt::format("  Product: {}", std::get<3>(device)) << std::endl;
        std::cout << fmt::format("  Serial: {}", std::get<4>(device)) << std::endl;
    }

    Device dev;

    try {
        dev = Device{0};
    }
    catch (Device::Exception const& ex) {
        std::cerr << fmt::format("Error {}: {}", ex.code(), ex.what()) << std::endl;
    }

    if (! dev.setCenterFrequency(94'300'000U)) {
        std::cerr << "Failed to set center frequency" << std::endl;
        return 1;
    }

    if (! dev.setSampleRate(1'024'000U)) {
        std::cerr << "Failed to set sample rate" << std::endl;
        return 1;
    }

    if (! dev.resetBuffer()) {
        std::cerr << "Failed to reset buffer" << std::endl;
        return 1;
    }

    if (! dev.readAsync()) {
        std::cerr << "Failed to start reading samples" << std::endl;
        return 1;
    }

    return 0;
}
