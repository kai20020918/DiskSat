# 1. 最小バージョンの指定 (必須)
cmake_minimum_required(VERSION 3.13)

set(PICO_TARGET_CHIP "rp2350" CACHE STRING "PICO Build platform" FORCE)

# RP2350のボード定義（必須でないが、より正確）
set(PICO_BOARD "pico2" CACHE STRING "PICO target board" FORCE)

# 2. SDKを読み込むための設定 (必須)
# 同じディレクトリに pico_sdk_import.cmake があることが前提です
include(pico_sdk_import.cmake)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
# set(PICO_PLATFORM rp2350) # これを project() 
# 3. プロジェクトの宣言 (必須)
project(blink C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# 4. SDKの初期化 (必須)
pico_sdk_init()

# --- ここから既存のコード ---

add_executable(blink
    blink.c
)

# pull in common dependencies
target_link_libraries(blink pico_stdlib)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(blink pico_cyw43_arch_none)
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(blink)

# add url via pico_set_program_url
# ※この関数はSDK標準ではない場合があるため、エラーが出るならコメントアウトしてください
# example_auto_set_url(blink)