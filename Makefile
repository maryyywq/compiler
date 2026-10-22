CXX      = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -Isrc
TARGET   = compiler
TESTBIN  = tests/run_tests
SEMBIN   = tests/run_sem_tests

SRCS = src/main.cpp src/Scanner.cpp src/Parser.cpp src/SemTree.cpp
OBJS = $(SRCS:.cpp=.o)

# автотесты синтаксического анализатора
TEST_SRCS = tests/tests.cpp src/Parser.cpp src/Scanner.cpp src/SemTree.cpp

# автотесты семантического анализатора и семантического дерева
SEM_SRCS  = tests/sem_tests.cpp src/Parser.cpp src/Scanner.cpp src/SemTree.cpp

HEADERS = src/defs.h src/Scanner.h src/Parser.h src/SemTree.h

FILE ?= input.txt

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# сборка автотестов синтаксического анализатора
$(TESTBIN): $(TEST_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRCS)

# сборка автотестов семантического анализатора
$(SEMBIN): $(SEM_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SEM_SRCS)

run: $(TARGET)
	./$(TARGET) $(FILE)

test: $(TESTBIN) $(SEMBIN)
	./$(TESTBIN)
	./$(SEMBIN)

clean:
	rm -f $(OBJS) $(TARGET) $(TESTBIN) $(SEMBIN)

.PHONY: all run test clean
