CXX      = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -Isrc
TARGET   = scanner
TESTBIN  = tests/run_tests

SRCS = src/main.cpp src/Scanner.cpp src/Parser.cpp
OBJS = $(SRCS:.cpp=.o)

# автотесты синтаксического анализатора
TEST_SRCS = tests/tests.cpp src/Parser.cpp src/Scanner.cpp

HEADERS = src/defs.h src/Scanner.h src/Parser.h

FILE ?= input.txt

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# сборка автотестов синтаксического анализатора
$(TESTBIN): $(TEST_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRCS)

run: $(TARGET)
	./$(TARGET) $(FILE)

test: $(TESTBIN)
	./$(TESTBIN)

clean:
	rm -f $(OBJS) $(TARGET) $(TESTBIN)

.PHONY: all run test clean
