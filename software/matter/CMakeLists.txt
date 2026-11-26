# Matter-over-Thread firmware for the esp105 CN105 adapter, board 0.3.0.
# Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.1.0")
set(PROJECT_VER_NUMBER 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp105_matter)

# Flags the Matter SDK expects from every project that builds it
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
# RISC-V targets add -Wno-format but leave flags that depend on -Wformat
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)
