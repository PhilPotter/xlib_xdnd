# xlib_xdnd
xlib_xdnd - a demo program to show the use of the X Drag-and-Drop Protocol for the X11 Windowing System.

This code has just enough of an XDND implementation (v5) to allow dragging of a file ("text/uri-list") type from one application to another. The program forks off two processes, and uses both of them to allow passing of state from one to the other and back, in the form of a small red square. Left-click the square and drag, then drop on the other window and it will appear there and disappear from the original. Press 'a' on your keyboard to toggle the square colour between red and green.

Behind the scenes, the active window's process stores a colour value to a temporary file, then uses XDND to pass that file URI to the other window, which then opens the file and reads the colour value out to set the square's colour (and visibility). The layout of the file is a single int32_t with either 0 (red) or 1 (blue). Yes, there are easier ways to send such a small amount of data to another window, but the motivation behind this approach is that a very good friend of mine (Stuart Barnes) is writing a program that will allow dragging of MIDI and other musical data between windows, and in order to help out I had to get to grips with XDND first.

This is not a complete XDND 5 implementation, just enough to demonstrate the concepts - for example, XdndProxy support is not included, and it just uses XdndActionCopy as the action. A consequence of this design is that if you drag a file with the described layout onto the empty window from a file browser such as Nautilus, it will create a second square with the file thanks to XDND. The program operates under the assumption that there is only one square at a time, but as it is multi-process this little trick works to show the protocol in action. Debug messages will also be printed in the terminal window from which the program is executed.

To build xlib_xdnd, just make sure you have the libX11 development files + headers installed for your distribution, then run:
```
make
```

This will build an xlib_xdnd_test binary in the same directory, which you can execute with:
```
./xlib_xdnd_test
```

I hope this brings some understanding to people and is of some use - there are lots of great documentation sources on the web, but writing this helped solidify my understanding of the concepts and protocols for myself.
