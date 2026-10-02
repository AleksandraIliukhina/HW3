# HW3 - 3D Floating Shape

This project is a simple 3D graphics program written in C++ using Xlib. 
The program displays one 3D shape at a time. There are three shapes:a cube, a pyramid with a square base, and a pyramid with a triangular base.
The shapes are displayed using colored edges on a white background. The different edge colors make it easier to see how the 3D shape rotates.
The program uses 3D coordinates (x, y, z), perspective projection, and a framebuffer to draw the shapes on the screen.

Requirements:
the program uses:
- C++
- Xlib
- Linux / WSL
- Make

The X11 development library is required:
bash
sudo apt install libx11-dev

When running, use:
chmod +x 3D-floating-shape

To switch from one shape to another, press 'S' on a keyboard. Press 'X' to either hide or show x, y, and z axis.
