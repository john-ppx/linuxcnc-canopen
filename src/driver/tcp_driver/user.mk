include ../config.mk

MOD_NAME := canopen

TARGET := $(MOD_NAME).so

STACKDRV_SRC =  ./driver/tcp_driver
STACK_SRC =     ../CANopenNode/stack
CANOPEN_SRC =   ../CANopenNode
OBJ_DICT_SRC =  ./objDict
APP_SRC =       ./app

INCLUDE_DIRS = -I$(STACKDRV_SRC) \
               -I$(STACK_SRC)    \
               -I$(CANOPEN_SRC)  \
               -I$(OBJ_DICT_SRC) \
               -I$(APP_SRC)

SOURCES =       $(STACKDRV_SRC)/CO_driver.c         \
                $(STACKDRV_SRC)/CO_OD_storage.c     \
                $(STACKDRV_SRC)/CO_Linux_tasks.c    \
                $(STACK_SRC)/crc16-ccitt.c          \
                $(STACK_SRC)/CO_SDO.c               \
                $(STACK_SRC)/CO_Emergency.c         \
                $(STACK_SRC)/CO_NMT_Heartbeat.c     \
                $(STACK_SRC)/CO_SYNC.c              \
                $(STACK_SRC)/CO_PDO.c               \
                $(STACK_SRC)/CO_HBconsumer.c        \
                $(STACK_SRC)/CO_SDOmaster.c         \
                $(STACK_SRC)/CO_trace.c             \
                $(CANOPEN_SRC)/CANopen.c            \
                canopen.c                           \
                CO_master.c                         \
                CO_command.c                        \
                CO_comm_helpers.c                   \
                $(OBJ_DICT_SRC)/CO_OD.c             \
                $(APP_SRC)/application.c

CANOPEN_OBJS = $(SOURCES:%.c=%.o)

obj-m := $(MOD_NAME).o

include $(MODINC)
EXTRA_CFLAGS := $(filter-out -Wframe-larger-than=%,$(EXTRA_CFLAGS))
EXTRA_CFLAGS += $(INCLUDE_DIRS)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(CANOPEN_OBJS)

clean:
	rm -f $(CANOPEN_OBJS)

