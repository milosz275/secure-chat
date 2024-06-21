CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
INCLUDES = -Iinclude
CLIBS = 
CSRCS = $(wildcard src/*.c)
COBJS = $(CSRCS:.c=.o)
COBJS := $(addprefix build/, $(COBJS))
MAIN = main
LIBNAME = libcommon.a

.PHONY: default all debug clean depend lib

default: all

all: $(MAIN)
	@echo "Project has been compiled"

debug: CFLAGS += -g -DDEBUG -D_DEBUG
debug: $(MAIN)
	@echo "Project has been compiled in debug mode"

$(MAIN): $(COBJS)
	@mkdir -p build/bin
	$(CC) $(CFLAGS) $(INCLUDES) -o build/bin/$(MAIN) $(COBJS) $(CLIBS)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

depend: $(CSRCS)
	makedepend $(INCLUDES) $^

lib: $(LIBNAME)

$(LIBNAME): $(COBJS)
	@mkdir -p build/lib
	ar rcs build/lib/$(LIBNAME) $(COBJS)

clean:
	$(RM) build/src/*.o *~ $(MAIN)
	$(RM) build/bin/$(MAIN)
	$(RM) build/lib/$(LIBNAME)

-include $(SRCS:.c=.d)
