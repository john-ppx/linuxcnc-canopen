.PHONY: all install clean

all:
	@$(MAKE) -f user.mk all

install:
	@$(MAKE) -f user.mk install

clean:
	rm -f *.so *.ko *.o
	rm -f *.sym *.tmp *.ver
	rm -f *.mod.c .*.cmd
	rm -f modules.order Module.symvers
	rm -rf .tmp_versions

