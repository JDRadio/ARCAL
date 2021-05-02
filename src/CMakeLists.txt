add_subdirectory(rtl2832)

add_executable(arcal
    main.cpp
)

target_link_libraries(arcal
    rtl2832
    usb
)
