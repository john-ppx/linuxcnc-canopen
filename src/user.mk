include ../config.mk

MOD_NAME := canopen

TARGET := $(MOD_NAME).so

EXTRA_CFLAGS := $(filter-out -Wframe-larger-than=%,$(EXTRA_CFLAGS))

CANOPEN_OBJS = \
	canopen.o \
	test.o \

obj-m := $(MOD_NAME).o

include $(MODINC)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(CANOPEN_OBJS)

