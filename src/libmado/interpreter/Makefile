CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -O0

SOURCES = test.cpp token.cpp lexer.cpp ast.cpp \
          ../../common/test_utils.cpp \
          ../../common/text_utils.cpp

TARGET = test_interpreter

test: $(SOURCES) token.hpp lexer.hpp ast.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: test clean
