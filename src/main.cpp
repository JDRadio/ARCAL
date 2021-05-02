#include "rtl-sdr.h"
#include <iostream>
#include <fmt/core.h>
#include <exception>
#include <stdexcept>

class Device
{
public:
    struct OpenException : std::exception { char const* what(void) const noexcept override { return "Failed to open device"; } };

    Device(unsigned int index)
    {
        if (rtlsdr_open(&dev_, index) != 0) {
            throw OpenException{};
        }
    }

    ~Device(void) noexcept
    {
        rtlsdr_close(dev_);
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

            if (rtlsdr_get_device_usb_strings(i, manufact, product, serial) != 0) {
                out.push_back(std::make_tuple(i, name, std::string{}, std::string{}, std::string{}));
            }
            else {
                out.push_back(std::make_tuple(i, name, manufact, product, serial));
            }
        }

        return out;
    }

private:
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

    try {
        Device dev{0};
    }
    catch (Device::OpenException const& ex) {
        std::cerr << ex.what() << std::endl;
    }

    return 0;
}
