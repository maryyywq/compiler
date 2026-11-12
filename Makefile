CXX      = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -Isrc
TARGET   = compiler
TESTBIN  = tests/run_tests
SEMBIN   = tests/run_sem_tests
INTERPBIN = tests/run_interp_tests

SRCS = src/main.cpp src/Scanner.cpp src/Parser.cpp src/SemTree.cpp src/Interp.cpp
OBJS = $(SRCS:.cpp=.o)

# автотесты синтаксического анализатора
TEST_SRCS = tests/tests.cpp src/Parser.cpp src/Scanner.cpp src/SemTree.cpp src/Interp.cpp

# автотесты семантического анализатора и семантического дерева
SEM_SRCS  = tests/sem_tests.cpp src/Parser.cpp src/Scanner.cpp src/SemTree.cpp src/Interp.cpp

# автотесты интерпретатора
INTERP_SRCS = tests/interp_tests.cpp src/Parser.cpp src/Scanner.cpp src/SemTree.cpp src/Interp.cpp

HEADERS = src/defs.h src/Scanner.h src/Parser.h src/SemTree.h src/Interp.h

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

# сборка автотестов интерпретатора
$(INTERPBIN): $(INTERP_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(INTERP_SRCS)

run: $(TARGET)
	./$(TARGET) $(FILE)

test: $(TESTBIN) $(SEMBIN) $(INTERPBIN)
	./$(TESTBIN)
	./$(SEMBIN)
	./$(INTERPBIN)

clean:
	rm -f $(OBJS) $(TARGET) $(TESTBIN) $(SEMBIN) $(INTERPBIN)

.PHONY: all run test clean
