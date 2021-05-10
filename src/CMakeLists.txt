set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter)

add_executable(arcal
    main.cpp
    ARCAL.cpp
    DCBlocker.cpp
    Device.cpp
    FFT.cpp
    Waterfall.cpp
)

target_link_libraries(arcal
    rtlsdr
    usb
    fmt
    fftw3f
    m
)
