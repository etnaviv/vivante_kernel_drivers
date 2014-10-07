ifeq ($(TARGET_BOARD_PLATFORM),mrvl)

.PHONY: build-galcore

ARCH ?= arm64
ARCH_TYPE ?= arm64

ifneq ($(ARCH),)
       ARCH := $(ARCH)
       ARCH_TYPE := $(ARCH)
       BUILD_PARAMETERS := ARCH=${ARCH} ARCH_TYPE=${ARCH_TYPE} CROSS_COMPILE=${CROSS_COMPILE} -j$(MAKE_JOBS)
endif

ifeq ($(ARCH), arm64)
       CROSS_COMPILE := $(KERNEL_TOOLS_PREFIX)
       BUILD_PARAMETERS := -j$(MAKE_JOBS)
endif

$(PRODUCT_OUT)/ramdisk.img: build-galcore

include $(CLEAR_VARS)
GALCORE_SRC_PATH := $(ANDROID_BUILD_TOP)/vendor/marvell/generic/graphics/driver
LOCAL_PATH := $(GALCORE_SRC_PATH)/hal/driver
LOCAL_SRC_FILES := galcore.ko
LOCAL_MODULE := $(LOCAL_SRC_FILES)
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/root/lib/modules
$(LOCAL_PATH)/$(LOCAL_SRC_FILES): build-galcore
include $(BUILD_PREBUILT)

build-galcore: android_kernel
	make clean -C $(GALCORE_SRC_PATH)
	cd $(GALCORE_SRC_PATH) &&\
	$(MAKE) $(BUILD_PARAMETERS)
ifeq (,$(wildcard $(PRODUCT_OUT)/root/lib/modules))
	mkdir -p $(PRODUCT_OUT)/root/lib/modules
endif
	cp $(GALCORE_SRC_PATH)/hal/driver/galcore.ko $(PRODUCT_OUT)/root/lib/modules


.PHONY: build-debug-galcore
build-debug-galcore: $(PRODUCT_OUT)/$(KERNEL_IMAGE)_debug
	cd $(GALCORE_SRC_PATH) &&\
	$(MAKE) $(BUILD_PARAMETERS)
ifeq (,$(wildcard $(PRODUCT_OUT)/system/lib/modules))
	mkdir -p $(PRODUCT_OUT)/system/lib/modules
endif
	cp $(GALCORE_SRC_PATH)/hal/driver/galcore.ko $(PRODUCT_OUT)/system/lib/modules

endif
