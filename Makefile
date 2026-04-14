CXX      := c++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
INCLUDES := -I src -I vendor/scalpel

SCALPEL_SRCS := vendor/scalpel/tokenizer.cpp \
                vendor/scalpel/char_classes.cpp \
                vendor/scalpel/sentence_segmenter.cpp

LIB_SRCS := src/lambda_calc.cpp \
             src/category.cpp \
             src/ccg_rules.cpp \
             src/lexicon.cpp \
             src/tokenizer.cpp \
             src/parser.cpp \
             src/pretty_print.cpp

ALL_SRCS  := $(LIB_SRCS) $(SCALPEL_SRCS)

BUILDDIR  := build
LIB_OBJS  := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(ALL_SRCS))
CLI_OBJ   := $(BUILDDIR)/main.o
TEST_OBJ  := $(BUILDDIR)/tests/test_main.o

TARGET    := $(BUILDDIR)/lambda
TEST_BIN  := $(BUILDDIR)/test_runner

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(LIB_OBJS) $(CLI_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

-include $(shell find $(BUILDDIR) -name '*.d' 2>/dev/null)

clean:
	rm -rf $(BUILDDIR)
