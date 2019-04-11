
#DRIVER := ./driver/canalystII
#DRIVER := ./driver/tcp_driver
DRIVER := ./driver/udp_driver

.PHONY: all install clean

all:
	@$(MAKE) -f $(DRIVER)/user.mk all

install:
	@$(MAKE) -f $(DRIVER)/user.mk install

clean:
	rm -f *.so *.ko *.o
	rm -f *.sym *.tmp *.ver
	rm -f *.mod.c .*.cmd
	rm -f modules.order Module.symvers
	rm -rf .tmp_versions
	@$(MAKE) -f $(DRIVER)/user.mk clean

