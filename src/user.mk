include ../config.mk

TARGET := canopen.so

EXTRA_CFLAGS := $(filter-out -Wframe-larger-than=%,$(EXTRA_CFLAGS))

CANOPEN_OBJS = \
	canopen.o \
	test.o \

.PHONY: all clean install

all: $(TARGET)

install: $(TARGET)
	cp $(TARGET) $(RTLIBDIR)/

$(TARGET): $(CANOPEN_OBJS)
	$(CC) -o $@ $(CANOPEN_OBJS) -shared -Bsymbolic -Wl,-rpath,$(LIBDIR) -L$(LIBDIR) -llinuxcnchal -lexpat

%.o: %.c
	$(CC) -o $@ $(EXTRA_CFLAGS) -Os -c $<

