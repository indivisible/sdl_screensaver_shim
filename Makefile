CC=gcc
CFLAGS=-shared -fPIC -ldl -D_GNU_SOURCE

%lib64.so: sdl_screensaver_shim.c
	$(CC) -o $@ $^ $(CFLAGS) -m64

%lib32.so: sdl_screensaver_shim.c
	$(CC) -o $@ $^ $(CFLAGS) -m32 -I/usr/i686-linux-gnu/include

main: sdl_screensaver_shim_lib64.so sdl_screensaver_shim_lib32.so
	@

.PHONY: clean

clean:
	rm -f *.so

