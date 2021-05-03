#include <rtl-sdr.h>
#include <iostream>
#include <fmt/format.h>
#include <exception>
#include <stdexcept>
#include <complex>
#include <fftw3.h>
#include <mutex>
#include <sstream>
#include <numeric>

class FFT
{
public:
    FFT(void) :
        head_{0},
        length_{0},
        plan_{nullptr},
        input_buffer_{nullptr},
        output_buffer_{nullptr}
    {
        setLength(256);
    }

    ~FFT(void)
    {
        if (plan_) {
            fftwf_destroy_plan(plan_);
        }

        if (input_buffer_) {
            fftwf_free(input_buffer_);
        }

        if (output_buffer_) {
            fftwf_free(output_buffer_);
        }
    }

    void setLength(unsigned int len)
    {
        if (plan_) {
            fftwf_destroy_plan(plan_);
        }

        if (input_buffer_) {
            fftwf_free(input_buffer_);
        }

        if (output_buffer_) {
            fftwf_free(output_buffer_);
        }

        head_ = 0;
        length_ = len;
        input_buffer_ = fftwf_alloc_complex(len);
        output_buffer_ = fftwf_alloc_complex(len);
        plan_ = fftwf_plan_dft_1d(len, input_buffer_, output_buffer_, FFTW_FORWARD, FFTW_MEASURE | FFTW_DESTROY_INPUT);
    }

    std::vector<float> execute(std::vector<float> const& in)
    {
        unsigned int const in_size = in.size();
        std::vector<float> out;
        out.reserve((in_size / length_ + 1) * length_);

        for (unsigned int i = 0; i < in_size; i += 2) {
            input_buffer_[head_][0] = in[i] * (i % 4 ? 1 : -1);
            input_buffer_[head_][1] = in[i+1] * (i % 4 ? 1 : -1);

            if (++head_ == length_) {
                head_ = 0;
                fftwf_execute(plan_);

                for (unsigned int n = 0; n < length_; ++n) {
                    out.push_back(output_buffer_[n][0]);
                    out.push_back(output_buffer_[n][1]);
                }
            }
        }

        return out;
    }

    unsigned int length(void) const noexcept
    {
        return length_;
    }

private:
    unsigned int head_;
    unsigned int length_;
    fftwf_plan plan_;
    fftwf_complex* input_buffer_;
    fftwf_complex* output_buffer_;
};

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
        mutex_{},
        dev_{nullptr},
        handler_{nullptr}
    {
    }

    Device(Device&& other) noexcept :
        mutex_{},
        dev_{nullptr},
        handler_{nullptr}
    {
        std::unique_lock our_lock{mutex_, std::defer_lock};
        std::unique_lock other_lock{other.mutex_, std::defer_lock};

        std::lock(our_lock, other_lock);

        dev_ = std::move(other.dev_);
        handler_ = std::move(other.handler_);

        other.dev_ = nullptr;
        other.handler_ = nullptr;
    }

    Device& operator=(Device&& other) noexcept
    {
        std::unique_lock our_lock{mutex_, std::defer_lock};
        std::unique_lock other_lock{other.mutex_, std::defer_lock};

        std::lock(our_lock, other_lock);

        dev_ = std::move(other.dev_);
        handler_ = std::move(other.handler_);

        other.dev_ = nullptr;
        other.handler_ = nullptr;

        return *this;
    }

    Device(unsigned int index) :
        mutex_{},
        dev_{nullptr},
        handler_{nullptr}
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
        std::lock_guard lock{mutex_};

        if (! dev_) {
            return false;
        }

        return rtlsdr_set_center_freq(dev_, freq) >= 0;
    }

    bool setSampleRate(unsigned int rate) noexcept
    {
        std::lock_guard lock{mutex_};

        if (! dev_) {
            return false;
        }

        return rtlsdr_set_sample_rate(dev_, rate) >= 0;
    }

    bool readSync(std::vector<std::uint8_t>& out) noexcept
    {
        std::lock_guard lock{mutex_};

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
        std::lock_guard lock{mutex_};

        if (! dev_) {
            return false;
        }

        int result = rtlsdr_reset_buffer(dev_);

        if (result < 0) {
            std::cerr << result << std::endl;
        }

        return result == 0;
    }

    bool setAgcMode(bool on) noexcept
    {
        std::lock_guard lock{mutex_};

        if (! dev_) {
            return false;
        }

        int result = rtlsdr_set_agc_mode(dev_, on ? 1 : 0);

        if (result < 0) {
            std::cerr << result << std::endl;
        }

        return result == 0;
    }

    bool setGain(float gain) noexcept
    {
        std::lock_guard lock{mutex_};

        if (! dev_) {
            return false;
        }

        int result = rtlsdr_set_tuner_gain(dev_, static_cast<int>(gain * 10.0f + 0.5f));

        if (result < 0) {
            std::cerr << result << std::endl;
        }

        return result == 0;
    }

    std::optional<std::vector<float>> listGains(void) noexcept
    {
        std::lock_guard lock{mutex_};

        if (! dev_) {
            return {};
        }

        int num_gains = rtlsdr_get_tuner_gains(dev_, nullptr);

        if (num_gains < 0) {
            std::cerr << num_gains << std::endl;
            return {};
        }

        std::vector<int> gains(num_gains);
        int result = rtlsdr_get_tuner_gains(dev_, gains.data());

        if (result < 0) {
            std::cerr << result << std::endl;
            return {};
        }

        std::vector<float> out;
        for (auto const& x : gains) {
            out.push_back(static_cast<float>(x) / 10.0f);
        }

        return out;
    }

    bool readAsync(std::function<void(std::vector<std::uint8_t>&&)> handler) noexcept
    {
        std::lock_guard lock{mutex_};

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

private:
    static void callback(std::uint8_t* buf, std::uint32_t len, void* ctx)
    {
        if (! buf || ! len || ! ctx) {
            return;
        }

        auto dev = reinterpret_cast<Device*>(ctx);

        if (dev->handler_) {
            dev->handler_(std::vector<std::uint8_t>(buf, buf + len));
        }
    }

    std::mutex mutex_;
    rtlsdr_dev_t* dev_;
    std::function<void(std::vector<std::uint8_t>&&)> handler_;
};

class DCBlocker
{
public:
    DCBlocker(void) noexcept :
        r_{0.998f},
        xi_{0.f},
        xq_{0.f},
        yi_{0.f},
        yq_{0.f}
    {
    }

    void execute(float& i, float& q) noexcept
    {
        yi_ = i - xi_ + r_ * yi_;
        xi_ = i;
        i = yi_;

        yq_ = q - xq_ + r_ * yq_;
        xq_ = q;
        q = yq_;
    }

private:
    float r_;
    float xi_;
    float xq_;
    float yi_;
    float yq_;
};

class ARCAL
{
public:
    ARCAL(void) noexcept :
        dev_{},
        fft_{}
    {
        fft_.setLength(FFT_SIZE);
    }

    void showBasicInfo(void) noexcept
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

    void showDeviceInfo(void) noexcept
    {
        auto gains = dev_.listGains();

        if (! gains) {
            std::cerr << "Failed to list available gains" << std::endl;
        }
        else {
            std::cout << fmt::format("Available gain values: {}", fmt::join(gains.value(), ", ")) << std::endl;
        }
    }

    void run(void) noexcept
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

    unsigned int mapPowerLevel(float lvl, float in_min, float in_max, unsigned int out_min, unsigned int out_max)
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

    char getWeightCharacter(float val)
    {
        static char const* greyscale_lo_hi = " .:-=+*#%@";

        return greyscale_lo_hi[mapPowerLevel(val, 0.f, 90.f, 0, 9)];
    }

    int getWeightColor(float val)
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

    std::string getWeightColorString(float val)
    {
        return fmt::format("\033[0;{}m{}", getWeightColor(val), getWeightCharacter(val));
    }

    void pushToAverage(unsigned int index, float val)
    {
        auto& vec = averages_[index];
        vec.push_back(val);
    }

    std::vector<float> convertSamples(std::vector<std::uint8_t> const& in, bool block_dc) {
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

    void calculateFFT(std::vector<float> const& samples)
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

    void displayFFT(void)
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

    void onSamples(std::vector<std::uint8_t>&& in)
    {
        auto samples = convertSamples(in, true);

        calculateFFT(samples);
        displayFFT();
    }

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

int main(int, char**)
{
    ARCAL{}.run();
    return 0;
}
