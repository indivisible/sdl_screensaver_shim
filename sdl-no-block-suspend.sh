#!/bin/sh

OUTPUT_DIR=$(dirname $(realpath $0))
LD_PRELOAD="$OUTPUT_DIR/sdl_screensaver_shim_lib32.so $OUTPUT_DIR/sdl_screensaver_shim_lib64.so" exec "$@"
