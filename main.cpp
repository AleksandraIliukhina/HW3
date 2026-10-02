#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/time.h>

// Assignment 3: software 3D renderer using Xlib.
// The program follows the coordinate-system, projection, rotation,
// framebuffer, input, and frame-rate ideas from the in-class notes.
// Source: in-class lecture notes supplied with the assignment.

const int WIN_WIDTH = 900;
const int WIN_HEIGHT = 700;
const double FOCAL_LENGTH = 500.0;
const double CAMERA_X = 5.0;
const double CAMERA_Y = 7.0;
const double CAMERA_Z = 70.0;
const double FPS_TIME = 16000.0; // about 16 ms per frame = about 60 FPS

struct Point3D {
    double x, y, z;
};

struct Edge {
    int a, b;
    uint32_t color;
};

struct ScreenPoint {
    int x, y;
};

struct Shape3D {
    std::vector<Point3D> points;
    std::vector<Edge> edges;
    double x, y, z;
};

long long currentTimeMicros() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<long long>(tv.tv_sec) * 1000000LL + tv.tv_usec;
}

Point3D rotateY(const Point3D& p, double angle) {
    // Rotation around the y-axis changes x and z while y stays the same.
    double newX = p.x * std::cos(angle) + p.z * std::sin(angle);
    double newZ = -p.x * std::sin(angle) + p.z * std::cos(angle);
    return {newX, p.y, newZ};
}

Point3D rotateZ(const Point3D& p, double angle) {
    // Rotation around z changes x and y while z stays the same.
    double newX = p.x * std::cos(angle) - p.y * std::sin(angle);
    double newY = p.x * std::sin(angle) + p.y * std::cos(angle);
    return {newX, newY, p.z};
}

Point3D rotatePoint(const Point3D& p, double angleY, double angleZ) {
    return rotateZ(rotateY(p, angleY), angleZ);
}

bool project(const Point3D& world, int& screenX, int& screenY,
              double sceneOffsetX = 0.0, double sceneOffsetY = 0.0) {
    // Move the point into camera/view coordinates first.
    double x = world.x - CAMERA_X;
    double y = world.y - CAMERA_Y;
    double z = world.z - CAMERA_Z;

    // The camera looks in the -z direction, so use positive depth here.
    double depth = -z;
    if (depth <= 1.0) return false;

    // Perspective projection: objects farther away look smaller.
    double projectedX = (x * FOCAL_LENGTH) / depth;
    double projectedY = (y * FOCAL_LENGTH) / depth;

    screenX = static_cast<int>(projectedX + WIN_WIDTH / 2.0 + sceneOffsetX);
    // Screen y increases downward, so negate the projected y coordinate.
    screenY = static_cast<int>(-projectedY + WIN_HEIGHT / 2.0 + sceneOffsetY);
    return true;
}

void putPixel(uint32_t* buffer, int x, int y, uint32_t color) {
    if (x < 0 || x >= WIN_WIDTH || y < 0 || y >= WIN_HEIGHT) return;
    buffer[y * WIN_WIDTH + x] = color;
}

void drawLine(uint32_t* buffer, int x0, int y0, int x1, int y1, uint32_t color) {
    // Integer line drawing keeps the renderer independent of a graphics library.
    int dx = std::abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        putPixel(buffer, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void drawShape(const Shape3D& shape, uint32_t* buffer,
               double angleY, double angleZ,
               double sceneOffsetX, double sceneOffsetY) {
    std::vector<Point3D> transformed;
    std::vector<ScreenPoint> projected;

    for (const Point3D& point : shape.points) {
        Point3D p = rotatePoint(point, angleY, angleZ);
        p.x += shape.x;
        p.y += shape.y;
        p.z += shape.z;
        transformed.push_back(p);

        int sx, sy;
        if (project(p, sx, sy, sceneOffsetX, sceneOffsetY)) {
            projected.push_back({sx, sy});
        } else {
            projected.push_back({-10000, -10000});
        }
    }

    // The surfaces stay white/transparent because the assignment view is a
    // wireframe-style model. The colored edges make the 3D rotation easier
    // to see without filling the faces.
    for (const Edge& edge : shape.edges) {
        if (projected[edge.a].x == -10000 || projected[edge.b].x == -10000) {
            continue;
        }
        drawLine(buffer,
                 projected[edge.a].x, projected[edge.a].y,
                 projected[edge.b].x, projected[edge.b].y,
                 edge.color);
    }
}

void drawAxes(uint32_t* buffer, double sceneOffsetX, double sceneOffsetY) {
    // The x, y and z axes are the 3D coordinate system projected onto the screen.
    // Because the camera looks along -z, the z-axis appears slightly diagonal
    // instead of being a perfectly horizontal or vertical screen line.
    const uint32_t axisColor = 0x00000000;

    int originX, originY;
    if (!project({0.0, 0.0, 0.0}, originX, originY, sceneOffsetX, sceneOffsetY)) return;

    // X axis: y = 0, z = 0.
    int xStart, xStartY, xEnd, xEndY;
    project({-60.0, 0.0, 0.0}, xStart, xStartY, sceneOffsetX, sceneOffsetY);
    project({60.0, 0.0, 0.0}, xEnd, xEndY, sceneOffsetX, sceneOffsetY);
    drawLine(buffer, xStart, xStartY, xEnd, xEndY, axisColor);
    drawLine(buffer, xEnd, xEndY, xEnd - 10, xEndY - 5, axisColor);
    drawLine(buffer, xEnd, xEndY, xEnd - 10, xEndY + 5, axisColor);

    // Y axis: x = 0, z = 0.
    int yStartX, yStart, yEndX, yEnd;
    project({0.0, -45.0, 0.0}, yStartX, yStart, sceneOffsetX, sceneOffsetY);
    project({0.0, 45.0, 0.0}, yEndX, yEnd, sceneOffsetX, sceneOffsetY);
    drawLine(buffer, yStartX, yStart, yEndX, yEnd, axisColor);
    drawLine(buffer, yEndX, yEnd, yEndX - 4, yEnd + 10, axisColor);
    drawLine(buffer, yEndX, yEnd, yEndX + 4, yEnd + 10, axisColor);

    // Z axis: x = 0, y = 0. It is shown in 3D and then projected to 2D.
    int zStartX, zStartY, zEndX, zEndY;
    project({0.0, 0.0, -55.0}, zStartX, zStartY, sceneOffsetX, sceneOffsetY);
    project({0.0, 0.0, 55.0}, zEndX, zEndY, sceneOffsetX, sceneOffsetY);
    drawLine(buffer, zStartX, zStartY, zEndX, zEndY, axisColor);

    // Arrowhead at the positive z direction.
    double dx = static_cast<double>(zEndX - zStartX);
    double dy = static_cast<double>(zEndY - zStartY);
    double length = std::sqrt(dx * dx + dy * dy);
    if (length > 0.0) {
        double ux = dx / length;
        double uy = dy / length;
        int ax1 = static_cast<int>(zEndX - ux * 12 - uy * 5);
        int ay1 = static_cast<int>(zEndY - uy * 12 + ux * 5);
        int ax2 = static_cast<int>(zEndX - ux * 12 + uy * 5);
        int ay2 = static_cast<int>(zEndY - uy * 12 - ux * 5);
        drawLine(buffer, zEndX, zEndY, ax1, ay1, axisColor);
        drawLine(buffer, zEndX, zEndY, ax2, ay2, axisColor);
    }

    // Small cross at the origin.
    drawLine(buffer, originX - 4, originY, originX + 4, originY, axisColor);
    drawLine(buffer, originX, originY - 4, originX, originY + 4, axisColor);
}

Shape3D makeCube(double x, double y, double z) {
    Shape3D s;
    s.x = x; s.y = y; s.z = z;

    s.points = {
        {-10,-10,-10}, {10,-10,-10}, {10,10,-10}, {-10,10,-10},
        {-10,-10,10},  {10,-10,10},  {10,10,10},  {-10,10,10}
    };

    // Each edge has its own color. The faces themselves are not filled.
    s.edges = {
        {0,1, 0x00FF3333}, {1,2, 0x0000AAFF}, {2,3, 0x0000AA55}, {3,0, 0x00AA00AA},
        {4,5, 0x00FF7700}, {5,6, 0x00CC00CC}, {6,7, 0x00008888}, {7,4, 0x00AA5500},
        {0,4, 0x00FF0088}, {1,5, 0x000066FF}, {2,6, 0x0000AA00}, {3,7, 0x00FF8800}
    };



    return s;
}

Shape3D makeSquarePyramid(double x, double y, double z) {
    Shape3D s;
    s.x = x; s.y = y; s.z = z;

    // Four base corners plus one top point make a square-based pyramid.
    s.points = {
        {-10,-8,-10}, {10,-8,-10}, {10,-8,10}, {-10,-8,10},
        {0,12,0}
    };

    // Only the edges are colored; the surfaces stay white.
    s.edges = {
        {0,1, 0x00FF3333}, {1,2, 0x0000AAFF}, {2,3, 0x0000AA55}, {3,0, 0x00AA00AA},
        {0,4, 0x00FF7700}, {1,4, 0x00CC00CC}, {2,4, 0x00008888}, {3,4, 0x00AA5500}
    };

    return s;
}

Shape3D makeTriangularPyramid(double x, double y, double z) {
    Shape3D s;
    s.x = x; s.y = y; s.z = z;

    // Three base points plus one top point make a triangular pyramid
    // (a tetrahedron).
    s.points = {
        {-12,-8,-8}, {12,-8,-8}, {0,-8,12},
        {0,12,0}
    };

    // A triangular pyramid has six edges: three around the base and
    // three from the base vertices to the top.
    s.edges = {
        {0,1, 0x00FF3333},
        {1,2, 0x0000AAFF},
        {2,0, 0x0000AA55},
        {0,3, 0x00AA00AA},
        {1,3, 0x00FF7700},
        {2,3, 0x00CC00CC}
    };

    return s;
}

void handleInput(Display* display, Window window,
                 double& sceneOffsetX, double& sceneOffsetY,
                 int& selectedShape, bool& showAxes, bool& running) {
    while (XPending(display) > 0) {
        XEvent event;
        XNextEvent(display, &event);

        if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);

            // Arrow keys move the whole picture on the screen.
            if (key == XK_Left) sceneOffsetX -= 10.0;
            if (key == XK_Right) sceneOffsetX += 10.0;
            if (key == XK_Up) sceneOffsetY -= 10.0;
            if (key == XK_Down) sceneOffsetY += 10.0;

            // Number keys choose one shape at a time.
            if (key == XK_1) selectedShape = 1;
            if (key == XK_2) selectedShape = 2;
            if (key == XK_3) selectedShape = 3;

            // S stands for the requested "space" control and switches shapes.
            if (key == XK_s) {
                selectedShape++;
                if (selectedShape > 3) selectedShape = 1;
            }

            // X toggles all three coordinate axes on or off.
            if (key == XK_x) showAxes = !showAxes;

            if (key == XK_Escape || key == XK_q) running = false;
        }

        if (event.type == ClientMessage) running = false;
    }
    (void)window;
}

int main() {
    std::cout << "Software Renderer Starting!" << std::endl;

    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Could not open X11 display! Exiting..." << std::endl;
        return 1;
    }

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen), 100, 100,
        WIN_WIDTH, WIN_HEIGHT, 1,
        BlackPixel(display, screen), WhitePixel(display, screen));

    XStoreName(display, window, "Assignment 3 - 3D Floating Shape");
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, window);

    GC gc = XCreateGC(display, window, 0, nullptr);

    // White is the background because the assignment demo is easier to read
    // when the colored faces and coordinate axes contrast with it.
    std::vector<uint32_t> framebuffer(WIN_WIDTH * WIN_HEIGHT, 0x00FFFFFF);

    // The assignment requires two pyramids and one cube.
    Shape3D cube = makeCube(0, 0, 0);
    Shape3D pyramid1 = makeSquarePyramid(0, 0, 0);
    Shape3D pyramid2 = makeTriangularPyramid(0, 0, 0);

    bool running = true;
    double sceneOffsetX = 0.0;
    double sceneOffsetY = 0.0;
    double angle = 0.0;
    int selectedShape = 1; // 1 = cube, 2 = square-based pyramid, 3 = triangular pyramid
    bool showAxes = true;

    while (running) {
        long long frameStart = currentTimeMicros();
        handleInput(display, window, sceneOffsetX, sceneOffsetY,
                    selectedShape, showAxes, running);

        // The object stays in one place; only its rotation changes over time.
        angle += 0.025;

        // White background for every new frame.
        std::fill(framebuffer.begin(), framebuffer.end(), 0x00FFFFFF);

        // Draw the coordinate axes only when the user has enabled them.
        if (showAxes) {
            drawAxes(framebuffer.data(), sceneOffsetX, sceneOffsetY);
        }

        // Only one object is displayed at a time.
        if (selectedShape == 1) {
            drawShape(cube, framebuffer.data(), angle, angle * 0.6, sceneOffsetX, sceneOffsetY);
        } else if (selectedShape == 2) {
            drawShape(pyramid1, framebuffer.data(), angle, angle * 0.6, sceneOffsetX, sceneOffsetY);
        } else {
            drawShape(pyramid2, framebuffer.data(), angle, angle * 0.6, sceneOffsetX, sceneOffsetY);
        }

        XImage* image = XCreateImage(
            display,
            DefaultVisual(display, screen),
            24,
            ZPixmap,
            0,
            reinterpret_cast<char*>(framebuffer.data()),
            WIN_WIDTH,
            WIN_HEIGHT,
            32,
            0);

        if (!image) {
            std::cerr << "Error creating XImage. Exiting..." << std::endl;
            break;
        }

        XPutImage(display, window, gc, image, 0, 0, 0, 0, WIN_WIDTH, WIN_HEIGHT);
        image->data = nullptr; // framebuffer owns this memory.
        XDestroyImage(image);

        // Draw axis labels only when the axes are visible. They move with the
        // rest of the picture so the coordinate system stays together.
        if (showAxes) {
            XSetForeground(display, gc, BlackPixel(display, screen));
            XDrawString(display, window, gc,
                        static_cast<int>(WIN_WIDTH - 35 + sceneOffsetX),
                        static_cast<int>(WIN_HEIGHT / 2 - 10 + sceneOffsetY),
                        const_cast<char*>("X"), 1);
            XDrawString(display, window, gc,
                        static_cast<int>(WIN_WIDTH / 2 + 10 + sceneOffsetX),
                        static_cast<int>(25 + sceneOffsetY),
                        const_cast<char*>("Y"), 1);
            XDrawString(display, window, gc,
                        static_cast<int>(WIN_WIDTH / 2 - 70 + sceneOffsetX),
                        static_cast<int>(WIN_HEIGHT / 2 + 35 + sceneOffsetY),
                        const_cast<char*>("Z"), 1);
        }

        XFlush(display);

        long long elapsed = currentTimeMicros() - frameStart;
        if (elapsed < static_cast<long long>(FPS_TIME)) {
            usleep(static_cast<useconds_t>(FPS_TIME - elapsed));
        }
    }

    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    std::cout << "Software Renderer Exiting!" << std::endl;
    return 0;
}
