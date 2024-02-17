CXX ?= $(CROSS_COMPILE)g++
CXXFLAGS = -Wall -Werror -Wextra -std=c++23
LDFLAGS =

SRCS = jtag-tool.cpp libobmccpld.cpp libobmcjtag.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = jtag-tool

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all install clean