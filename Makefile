.PHONY: all clean

#------------------------------------------------------------------
# MASTER setting
#------------------------------------------------------------------
MASTER_EXE_NAME = trina-energy1master
PROTOCOL_NAME = trina-energy1
MASTER_CONFIG_PATH = /etc/trina-energy1master/
MASTER_VERSION = 0.0
TAG_VERSION = 1
MASTER_INCLUDE = -I./
MASTER_LIBRARY = 
MASTER_CONFIG_NAME = configuration.json

MASTER_ARGS = -DMASTER_NAME=\"$(MASTER_EXE_NAME)\" \
				-DVERSION=\"$(MASTER_VERSION)\" \
				-DCONFIG_PATH=\"$(MASTER_CONFIG_PATH)\" \
				-DCONFIG_FILE=\"$(MASTER_CONFIG_PATH)$(MASTER_CONFIG_NAME)\" \
				-DMASTER_PID_PATH=\"/var/run/$(MASTER_EXE_NAME).pid\" \
				-DFW_TAG_VERSION=$(TAG_VERSION)

MASTER_C_MAIN = ./master.c
MASTER_C_LIST = ./master_configuration.c \
				./equipment_hook.c \
				./tag.c \
				./trina_rs485.c

#------------------------------------------------------------------
CFLAGS =

OUTPUT_PATH = ./bin

CLIENT_SO_DEST = /usr/lib/

LIBRARY = -ljansson -lpthread -lzmq -lmxtagf

MXFB_LIBRARY = -lmxfb -lmxfb_equ -lmxfbi

LDFLAGS = -Wno-deprecated-declarations -Wno-unused-function

INCLUDE = -I/usr/include/

#------------------------------------------------------------------

all: MASTER

MASTER: 
	mkdir -p $(OUTPUT_PATH)
	gcc -o $(OUTPUT_PATH)/$(MASTER_EXE_NAME) $(CFLAGS) $(LIBRARY) $(MXFB_LIBRARY) $(LDFLAGS) $(INCLUDE) $(MASTER_INCLUDE) $(MASTER_LIBRARY) $(MASTER_C_MAIN) $(MASTER_C_LIST) $(MASTER_ARGS)

clean:
	rm -rf $(OUTPUT_PATH)/*

