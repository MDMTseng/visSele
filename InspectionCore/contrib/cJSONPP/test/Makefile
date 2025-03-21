BUILDDIR=./build
ODIR=$(BUILDDIR)/obj/test
BINDIR=$(BUILDDIR)/bin/test
CJSONPP_LIB=../cJSONPP.a
CJSON_LIB=../../cJSON/cJSON.a

# Source and object files
TEST_SRCS = $(wildcard *.cpp)
TEST_OBJS = $(patsubst %.cpp,$(ODIR)/%.o,$(TEST_SRCS))
TEST_BINS = $(patsubst %.cpp,$(BINDIR)/%,$(TEST_SRCS))

# Compiler settings
CXX = g++
CXXFLAGS = -I.. -I../../ -g -Wall -std=c++11
LDFLAGS = $(CJSONPP_LIB) $(CJSON_LIB)

.PHONY: all clean test

all: $(TEST_BINS)

$(ODIR)/%.o: %.cpp
	@mkdir -p $(ODIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BINDIR)/%: $(ODIR)/%.o
	@mkdir -p $(BINDIR)
	$(CXX) $< $(LDFLAGS) -o $@

test: all
	@echo "Running cJSONPP tests..."
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test; \
		if [ $$? -ne 0 ]; then \
			echo "TEST FAILED: $$test"; \
			exit 1; \
		fi; \
	done
	@echo "All tests passed!"

clean:
	rm -rf $(ODIR) $(BINDIR) 