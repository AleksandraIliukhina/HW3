CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
LDFLAGS = -lX11 -lm

3D-floating-shape: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o 3D-floating-shape $(LDFLAGS)

clean:
	rm -f 3D-floating-shape
