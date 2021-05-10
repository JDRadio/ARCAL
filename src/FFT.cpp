////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#include "FFT.hpp"

FFT::FFT(void) :
    head_{0},
    length_{0},
    plan_{nullptr},
    input_buffer_{nullptr},
    output_buffer_{nullptr}
{
    setLength(256);
}

FFT::~FFT(void)
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

void FFT::setLength(unsigned int len)
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

std::vector<float> FFT::execute(std::vector<float> const& in)
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
                out.push_back(output_buffer_[n][0] / length_);
                out.push_back(output_buffer_[n][1] / length_);
            }
        }
    }

    return out;
}

unsigned int FFT::length(void) const noexcept
{
    return length_;
}
