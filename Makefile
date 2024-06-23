COMMON = common
SERVER = server
CLIENT = client
LIBNAME = libcommon.a

SERVER_BIN = $(SERVER)/build/bin/$(SERVER)
COMMON_LIB = $(COMMON)/build/lib/$(LIBNAME)
CLIENT_BIN = $(CLIENT)/build/bin/$(CLIENT)

.PHONY: all common server client
all: common server client

common:
	$(MAKE) -C $(COMMON) lib

server: common
	$(MAKE) -C $(SERVER)

client: server
	$(MAKE) -C $(CLIENT)

clean:
	find $(SERVER)/build/src -name '*.o' -delete
	find $(SERVER)/build/src -name '*~' -delete
	$(RM) $(SERVER)/build/bin/$(SERVER)
	find $(COMMON)/build/src -name '*.o' -delete
	find $(COMMON)/build/src -name '*~' -delete
	find $(COMMON)/build/bin -name '$(COMMON)' -delete
	$(RM) $(COMMON)/build/lib/$(LIBNAME)
	find $(CLIENT)/build/src -name '*.o' -delete
	find $(CLIENT)/build/src -name '*~' -delete
	$(RM) $(CLIENT)/build/bin/$(CLIENT)
