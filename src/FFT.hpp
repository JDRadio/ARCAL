////////////////////////////////////////////////////////////////////////////////
//! \file
//! \author Jean-Sebastien Dominique <jd@jdradio.dev>
//! \date 2021
//! \copyright JDRadio Inc.
////////////////////////////////////////////////////////////////////////////////
#ifndef JDRADIO_FFT_HPP
#define JDRADIO_FFT_HPP

#include <vector>
#include <fftw3.h>

class FFT
{
public:
    FFT(void);
    ~FFT(void);
    void setLength(unsigned int len);
    std::vector<float> execute(std::vector<float> const& in);
    unsigned int length(void) const noexcept;

private:
    unsigned int head_;
    unsigned int length_;
    fftwf_plan plan_;
    fftwf_complex* input_buffer_;
    fftwf_complex* output_buffer_;
};

#endif
