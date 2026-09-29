CXX      = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGET   = scanner

SRCS = main.cpp Scanner.cpp
OBJS = $(SRCS:.cpp=.o)

HEADERS = defs.h Scanner.h

FILE ?= input.txt

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(FILE)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean