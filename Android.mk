.PHONY: build-galcore

$(PRODUCT_OUT)/ramdisk.img: galcore.ko

include $(CLEAR_VARS)
GALCORE_SRC_PATH := $(ANDROID_BUILD_TOP)/vendor/marvell/generic/graphics/driver
LOCAL_PATH := $(GALCORE_SRC_PATH)/hal/driver
LOCAL_SRC_FILES := galcore.ko
LOCAL_MODULE := $(LOCAL_SRC_FILES)
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/lib/modules
$(LOCAL_PATH)/$(LOCAL_SRC_FILES): build-galcore
include $(BUILD_PREBUILT)

build-galcore: build-kernel
	cd $(GALCORE_SRC_PATH) &&\
	$(MAKE) -j$(MAKE_JOBS)
ifeq (,$(wildcard $(PRODUCT_OUT)/system/lib/modules))
	mkdir -p $(PRODUCT_OUT)/system/lib/modules
endif
	cp $(GALCORE_SRC_PATH)/hal/driver/galcore.ko $(PRODUCT_OUT)/system/lib/modules
