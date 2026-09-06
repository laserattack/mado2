CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -O0

SOURCES = test.cpp token.cpp ../common/test/test_utils.cpp
TARGET = test_interpreter

test: $(SOURCES) token.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: test clean
