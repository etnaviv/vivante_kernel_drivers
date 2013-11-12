$(eval $(call add-module, galcore))

galcore:
	cd $(TOPDIR)vendor/marvell/generic/graphics/driver && KERNEL_DIR=$(KBUILD_OUTPUT) make -f makefile && cp hal/driver/*.ko $(KBUILD_OUTPUT)/modules
