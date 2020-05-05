all: xlib_xdnd_test

xlib_xdnd_test:
	cc -o xlib_xdnd_test main.c spawn_window.c square_state.c xevent_type.c phil_error.c -lX11
clean:
	rm -f xlib_xdnd_test
