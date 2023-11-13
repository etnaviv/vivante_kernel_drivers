##############################################################################
#
#    Copyright (c) 2005 - 2015 by Vivante Corp.  All rights reserved.
#
#    The material in this file is confidential and contains trade secrets
#    of Vivante Corporation. This is proprietary information owned by
#    Vivante Corporation. No part of this work may be disclosed,
#    reproduced, copied, transmitted, or used in any way for any purpose,
#    without the express written permission of Vivante Corporation.
#
##############################################################################


LOCAL_PATH := $(call my-dir)
include $(LOCAL_PATH)/../../Android.mk.def


#
# galcore.ko
#
include $(CLEAR_VARS)
.PHONY: KBUILD

GALCORE := \
	$(LOCAL_PATH)/../../galcore.ko

$(GALCORE):   KBUILD
	@cd $(AQROOT)
	@$(MAKE) -f Kbuild -C $(AQROOT) \
		AQROOT=$(abspath $(AQROOT)) \
		AQARCH=$(abspath $(AQARCH)) \
		AQVGARCH=$(abspath $(AQVGARCH)) \
		ARCH_TYPE=$(ARCH_TYPE) \
		DEBUG=$(DEBUG) \
		VIVANTE_ENABLE_2D=$(VIVANTE_ENABLE_2D) \
		VIVANTE_ENABLE_3D=$(VIVANTE_ENABLE_3D) \
		VIVANTE_ENABLE_VG=$(VIVANTE_ENABLE_VG) \
		NO_DMA_COHERENT=$(NO_DMA_COHERENT) \
		ENABLE_GPU_CLOCK_BY_DRIVER=$(ENABLE_GPU_CLOCK_BY_DRIVER) \
		USE_PLATFORM_DRIVER=$(USE_PLATFORM_DRIVER) \
		FORCE_ALL_VIDEO_MEMORY_CACHED=$(FORCE_ALL_VIDEO_MEMORY_CACHED) \
		NONPAGED_MEMORY_CACHEABLE=$(NONPAGED_MEMORY_CACHEABLE) \
		NONPAGED_MEMORY_BUFFERABLE=$(NONPAGED_MEMORY_BUFFERABLE) \
		ENABLE_OUTER_CACHE_PATCH=$(ENABLE_OUTER_CACHE_PATCH) \
		USE_BANK_ALIGNMENT=$(USE_BANK_ALIGNMENT) \
		BANK_BIT_START=$(BANK_BIT_START) \
		BANK_BIT_END=$(BANK_BIT_END) \
		BANK_CHANNEL_BIT=$(BANK_CHANNEL_BIT) \
		SECURITY=$(SECURITY)


LOCAL_SRC_FILES := \
	../../galcore.ko

LOCAL_GENERATED_SOURCES := \
	$(GALCORE)

LOCAL_MODULE       := galcore.ko
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH  := $(TARGET_OUT_SHARED_LIBRARIES)/modules
include $(BUILD_PREBUILT)

