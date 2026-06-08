#!/usr/bin/env bash
# Build and run the headless Document/EditHistory fuzz harness under ASan/UBSan.
# Exercises mass BPM edits, bar lines, channel insert/move/destroy, note
# updates, resolution conversion, modal note edits, and undo/redo storms
# (issue #4 clusters). Reuses a sanitizer build's object files; see
# run_fuzz_bms.sh for how to produce build-asan.
#
# Usage (from repo root):  tools/run_fuzz_history.sh build-asan
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
    ../tools/fuzz_history.cpp -o fuzz_history.o

# Link every object except the other fuzz harness; fuzz_history's main() wins
# over main.o's via --allow-multiple-definition (App lives in main.o).
OBJS="$(ls ./*.o | grep -vE '/fuzz_(bms|history)\.o$' | tr '\n' ' ')"
# shellcheck disable=SC2086
g++ -fsanitize=address -fsanitize=undefined -Wl,--allow-multiple-definition \
    -o fuzz_history fuzz_history.o $OBJS \
    /usr/lib/x86_64-linux-gnu/libQt5Multimedia.so \
    /usr/lib/x86_64-linux-gnu/libQt5Widgets.so \
    /usr/lib/x86_64-linux-gnu/libQt5Gui.so \
    /usr/lib/x86_64-linux-gnu/libQt5Concurrent.so \
    /usr/lib/x86_64-linux-gnu/libQt5Network.so \
    /usr/lib/x86_64-linux-gnu/libQt5Core.so -lGL -lpthread

QT_QPA_PLATFORM=offscreen \
ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:print_stacktrace=1 \
UBSAN_OPTIONS=print_stacktrace=1 \
    ./fuzz_history
