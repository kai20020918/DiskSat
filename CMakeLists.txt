cmake_minimum_required(VERSION 3.13)

# RP2350およびPico2の設定
set(PICO_TARGET_CHIP "rp2350" CACHE STRING "PICO Build platform" FORCE)
set(PICO_BOARD "pico2" CACHE STRING "PICO target board" FORCE)

# SDKのインポート
include(pico_sdk_import.cmake)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

project(DiskSat C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# SDKの初期化
pico_sdk_init()

add_executable(DiskSat
    main.c
)

# 依存ライブラリのリンク
# hardware_i2c: I2C制御用ライブラリを追加
target_link_libraries(DiskSat 
    pico_stdlib 
    hardware_uart 
    hardware_gpio 
    hardware_i2c
)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(DiskSat pico_cyw43_arch_none)
endif()

# --- UART掃き出しのための重要設定 ---

# printfなどの標準入出力をUART経由に設定
pico_enable_stdio_uart(DiskSat 1)
# USBシリアル（CDC）経由での出力を無効（UARTに専念させる場合）
pico_enable_stdio_usb(DiskSat 0)

# バイナリ等の追加出力生成
pico_add_extra_outputs(DiskSat)