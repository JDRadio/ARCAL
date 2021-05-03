////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#ifndef JDRADIO_DEVICE_HPP
#define JDRADIO_DEVICE_HPP

#include <rtl-sdr.h>
#include <vector>
#include <string>
#include <tuple>
#include <optional>
#include <functional>
#include <mutex>
#include <exception>

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

    Device(void) noexcept;
    Device(Device&& other) noexcept;
    Device& operator=(Device&& other) noexcept;
    Device(unsigned int index);
    ~Device(void) noexcept;
    static std::vector<std::tuple<unsigned int, std::string, std::string, std::string, std::string>> listDevices(void) noexcept;
    bool setCenterFrequency(unsigned int freq) noexcept;
    bool setSampleRate(unsigned int rate) noexcept;
    bool readSync(std::vector<std::uint8_t>& out) noexcept;
    bool resetBuffer(void) noexcept;
    bool setAgcMode(bool on) noexcept;
    bool setGain(float gain) noexcept;
    std::optional<std::vector<float>> listGains(void) noexcept;
    bool readAsync(std::function<void(std::vector<std::uint8_t>&&)> handler) noexcept;
    bool cancelAsync(void) noexcept;

private:
    static void callback(std::uint8_t* buf, std::uint32_t len, void* ctx);

    std::mutex mutex_;
    rtlsdr_dev_t* dev_;
    std::function<void(std::vector<std::uint8_t>&&)> handler_;
};

#endif
