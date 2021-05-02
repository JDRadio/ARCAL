add_executable(arcal
    main.cpp
)

target_link_libraries(arcal
    rtlsdr
    usb
    fmt
)
