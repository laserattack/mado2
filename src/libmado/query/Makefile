CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -O0

SOURCES = test.cpp token.cpp lexer.cpp ast.cpp parser.cpp \
          ../../common/test_utils.cpp \
          ../../common/text_utils.cpp

TARGET = test_query

test: $(SOURCES) token.hpp lexer.hpp ast.hpp parser.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: test clean
