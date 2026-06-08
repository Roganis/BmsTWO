#!/usr/bin/env bash
# Build and run the headless BMS importer fuzz harness under ASan/UBSan.
#
# It reuses the instrumented object files from a sanitizer build of the app
# (so it links the real BmsReader) and swaps in tools/fuzz_bms.cpp's main().
#
# Usage:
#   1) Configure a sanitizer build:
#        mkdir build-asan && cd build-asan
#        qmake ../src/bmsone.pro CONFIG+=debug CONFIG+=sanitizer \
#              CONFIG+=sanitize_address CONFIG+=sanitize_undefined
#        make -j$(nproc)
#   2) From the repo root:  tools/run_fuzz_bms.sh build-asan
set -euo pipefail

BUILD="${1:-build-asan}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QTI="$(pkg-config --variable=includedir Qt5Core 2>/dev/null || echo /usr/include/x86_64-linux-gnu/qt5)"
MKSPEC="/usr/lib/x86_64-linux-gnu/qt5/mkspecs/linux-g++"

cd "$ROOT/$BUILD"

g++ -c -pipe -std=c++17 -g -O0 -fsanitize=address -fsanitize=undefined \
    -fno-omit-frame-pointer -fPIC \
    -DQT_MULTIMEDIA_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CONCURRENT_LIB \
    -DQT_NETWORK_LIB -DQT_CORE_LIB \
    -I../src -I../src/sequence_view \
    -I"$QTI" -I"$QTI/QtCore" -I"$QTI/QtMultimedia" -I"$QTI/QtWidgets" \
    -I"$QTI/QtGui" -I"$QTI/QtConcurrent" -I"$QTI/QtNetwork" -I"$MKSPEC" \
    ../tools/fuzz_bms.cpp -o fuzz_bms.o

OBJS="$(ls ./*.o | grep -v '/fuzz_bms.o$' | tr '\n' ' ')"
# shellcheck disable=SC2086
g++ -fsanitize=address -fsanitize=undefined -Wl,--allow-multiple-definition \
    -o fuzz_bms fuzz_bms.o $OBJS \
    /usr/lib/x86_64-linux-gnu/libQt5Multimedia.so \
    /usr/lib/x86_64-linux-gnu/libQt5Widgets.so \
    /usr/lib/x86_64-linux-gnu/libQt5Gui.so \
    /usr/lib/x86_64-linux-gnu/libQt5Concurrent.so \
    /usr/lib/x86_64-linux-gnu/libQt5Network.so \
    /usr/lib/x86_64-linux-gnu/libQt5Core.so -lGL -lpthread

QT_QPA_PLATFORM=offscreen \
ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:print_stacktrace=1 \
UBSAN_OPTIONS=print_stacktrace=1 \
    ./fuzz_bms
