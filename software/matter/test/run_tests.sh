#!/bin/sh
# Builds and runs the host-side CN105 protocol tests with the system C++ compiler.
set -e
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/esp105_cn105_tests"
c++ -std=c++17 -Wall -Wextra -Werror -I ../components/cn105/include \
    test_cn105_protocol.cpp ../components/cn105/cn105_protocol.cpp -o "$out"
"$out"
