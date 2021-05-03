add_executable(arcal
    main.cpp
    ARCAL.cpp
    DCBlocker.cpp
    Device.cpp
    FFT.cpp
)

target_link_libraries(arcal
    rtlsdr
    usb
    fmt
    fftw3f
    m
)
