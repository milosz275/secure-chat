# Compiler
CC = gcc
CXX = g++

# Compiler flags
CFLAGS = -std=c11 -Wall -Wextra
CXXFLAGS = -std=c++20 -Wall -Wextra

# Include directories
INCLUDES = -Iinclude

# Libraries
CLIBS = 
CXXLIBS = 

# Source files
CSRCS = $(wildcard src/*.c)
CXXSRCS = $(wildcard src/*.cpp)

# Object files
COBJS = $(CSRCS:.c=.o)
COBJS := $(addprefix build/, $(COBJS))

CXXOBJS = $(CXXSRCS:.cpp=.o)
CXXOBJS := $(addprefix build/, $(CXXOBJS))

# Executable name
MAIN = main

.PHONY: default all debug clean depend

default: all

all: $(MAIN)
	@echo "Project has been compiled"

debug: CFLAGS += -g -DDEBUG -D_DEBUG
debug: CXXFLAGS += -g -DDEBUG -D_DEBUG
debug: $(MAIN)
	@echo "Project has been compiled in debug mode"

### Choose one of the following for C or C++ compilation -------------------

# $(MAIN): $(COBJS)
# 	@mkdir -p build/bin
# 	$(CC) $(CFLAGS) $(INCLUDES) -o build/bin/$(MAIN) $(COBJS) $(CLIBS)

$(MAIN): $(CXXOBJS)
	@mkdir -p build/bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o build/bin/$(MAIN) $(CXXOBJS) $(CXXLIBS)

# build/%.o: %.c
# 	@mkdir -p $(@D)
# 	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

build/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<  -o $@

# depend: $(CSRCS)
# 	makedepend $(INCLUDES) $^

depend: $(CXXSRCS)
	makedepend $(INCLUDES) $^

### -----------------------------------------------------------------------

clean:
	$(RM) build/src/*.o *~ $(MAIN)
	$(RM) build/bin/$(MAIN)

# Include dependency files
-include $(SRCS:.c=.d)
-include $(SRCS:.cpp=.d)
