cmake_minimum_required(VERSION 3.16)
project(dji_rpi_telemetry C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT DJI_PSDK_DIR)
    message(FATAL_ERROR "Set DJI_PSDK_DIR to the Payload-SDK checkout")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(DJI_TOOLCHAIN_DIR "aarch64-linux-gnu-gcc")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(DJI_TOOLCHAIN_DIR "x86_64-linux-gnu-gcc")
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(DJI_LINUX_COMMON
    "${DJI_PSDK_DIR}/samples/sample_c/platform/linux/common")

add_executable(dji_rpi_telemetry
    src/main.c
    src/telemetry.c
    src/hal_uart.c
    "${DJI_LINUX_COMMON}/osal/osal.c"
    "${DJI_LINUX_COMMON}/osal/osal_fs.c")

target_compile_definitions(dji_rpi_telemetry PRIVATE
    _GNU_SOURCE
    SYSTEM_ARCH_LINUX)

target_include_directories(dji_rpi_telemetry PRIVATE
    src
    "${CMAKE_BINARY_DIR}/generated"
    "${DJI_PSDK_DIR}/psdk_lib/include"
    "${DJI_LINUX_COMMON}")

target_link_libraries(dji_rpi_telemetry PRIVATE
    "${DJI_PSDK_DIR}/psdk_lib/lib/${DJI_TOOLCHAIN_DIR}/libpayloadsdk.a"
    pthread
    m)
