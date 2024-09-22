CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address -g -O2
INCLUDES = -Iinclude -I../common/include
CLIBS = -lpthread -L../common/build/lib -lcommon -lsqlite3
CSRCS = $(wildcard src/*.c)
COBJS = $(CSRCS:.c=.o)
COBJS := $(addprefix build/, $(COBJS))
MAIN = server

.PHONY: default all debug clean depend

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

clean:
	$(RM) build/src/*.o *~ $(MAIN)
	$(RM) build/bin/$(MAIN)

-include $(SRCS:.c=.d)
