CXX =  g++
CFLAGS = -std=c++26 -Wall -Wextra

.PHONY: all clean

all: matcher

matcher: matcher.cpp
	$(CXX) $(CFLAGS) $< -o $@ && ./$@

clean:
	rm -f matcher *.o
