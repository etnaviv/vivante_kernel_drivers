##############################################################################
#
#    Copyright (c) 2005 - 2014 by Vivante Corp.  All rights reserved.
#
#    The material in this file is confidential and contains trade secrets
#    of Vivante Corporation. This is proprietary information owned by
#    Vivante Corporation. No part of this work may be disclosed,
#    reproduced, copied, transmitted, or used in any way for any purpose,
#    without the express written permission of Vivante Corporation.
#
##############################################################################


#
# Linux build file for kernel HAL driver.
#

include $(AQROOT)/config

DRIVER_OUT_DIR = hal/driver
KERNEL_DIR ?= $(TOOL_DIR)/kernel

OS_KERNEL_DIR   := hal/os/linux/kernel
ifeq ($(USE_ARCH_REG), 1)
ARCH_KERNEL_DIR := hal/kernel/arch_reg
ARCH_VG_KERNEL_DIR := hal/kernel/archvg_reg
else
ARCH_KERNEL_DIR := hal/kernel/arch
ARCH_VG_KERNEL_DIR := hal/kernel/archvg
endif
HAL_KERNEL_DIR  := hal/kernel
GPUFREQ_DIR     := $(OS_KERNEL_DIR)/gpufreq

CUSTOMER_ALLOCATOR_OBJS :=

EXTRA_CFLAGS += -Werror
EXTRA_CFLAGS += -fno-pic

ifneq ($(USE_MULTI_GPU), )
    EXTRA_CFLAGS += -DgcdMULTI_GPU=$(USE_MULTI_GPU)
endif

OBJS := $(OS_KERNEL_DIR)/gc_hal_kernel_device.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_driver.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_linux.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_math.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_os.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_sysfs.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_sysfs_test.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_debugfs.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_allocator.o \

OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_plat.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_plat_common.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_plat_adir.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_plat_eden.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_plat_pxa988.o \

ifeq ($(USE_GPU_FREQ), 1)

OBJS += $(GPUFREQ_DIR)/gpufreq.o \
        $(GPUFREQ_DIR)/gpufreq-pxa988.o \
        $(GPUFREQ_DIR)/gpufreq-eden.o \
        $(GPUFREQ_DIR)/gpufreq_ondemand.o \
        $(GPUFREQ_DIR)/gpufreq_conservative.o \
        $(GPUFREQ_DIR)/gpufreq_userspace.o \
        $(GPUFREQ_DIR)/gpufreq_performance.o \
        $(GPUFREQ_DIR)/gpufreq_powersave.o

EXTRA_CFLAGS += -DUSE_GPU_FREQ=1

else

EXTRA_CFLAGS += -DUSE_GPU_FREQ=0

endif


OBJS += $(HAL_KERNEL_DIR)/gc_hal_kernel.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_command.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_db.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_debug.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_event.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_heap.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_mmu.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_video_memory.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_power.o

OBJS += $(ARCH_KERNEL_DIR)/gc_hal_kernel_context.o \
        $(ARCH_KERNEL_DIR)/gc_hal_kernel_hardware.o

ifeq ($(VIVANTE_ENABLE_VG), 1)
OBJS +=\
          $(HAL_KERNEL_DIR)/gc_hal_kernel_vg.o\
          $(HAL_KERNEL_DIR)/gc_hal_kernel_command_vg.o\
          $(HAL_KERNEL_DIR)/gc_hal_kernel_interrupt_vg.o\
          $(HAL_KERNEL_DIR)/gc_hal_kernel_mmu_vg.o\
          $(ARCH_VG_KERNEL_DIR)/gc_hal_kernel_hardware_command_vg.o\
          $(ARCH_VG_KERNEL_DIR)/gc_hal_kernel_hardware_vg.o
endif

ifneq ($(CONFIG_SYNC),)
OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_sync.o
endif


ifneq ($(CUSTOMER_ALLOCATOR_OBJS),)
OBJS += $(CUSTOMER_ALLOCATOR_OBJS)
endif

ifeq ($(KERNELRELEASE), )

.PHONY: all clean install

# Define targets.
all:
	@mkdir -p $(DRIVER_OUT_DIR)
	@$(MAKE) V=$(V) ARCH=$(ARCH_TYPE) -C $(KERNEL_DIR) SUBDIRS=`pwd` modules

clean:
	@rm -rf $(OBJS)
	@rm -rf $(DRIVER_OUT_DIR)
	@rm -rf modules.order Module.symvers
	@find $(AQROOT) -name ".gc_*.cmd" | xargs rm -f

install: all
	@mkdir -p $(SDK_DIR)/drivers
	@cp $(DRIVER_OUT_DIR)/galcore.ko $(SDK_DIR)/drivers

else


EXTRA_CFLAGS += -DLINUX -DDRIVER

ifeq ($(FLAREON),1)
EXTRA_CFLAGS += -DFLAREON
endif

ifeq ($(DEBUG), 1)
EXTRA_CFLAGS += -DDBG=1 -DDEBUG -D_DEBUG
else
EXTRA_CFLAGS += -DDBG=0
endif

ifeq ($(NO_DMA_COHERENT), 1)
EXTRA_CFLAGS += -DNO_DMA_COHERENT
endif

ifeq ($(CONFIG_DOVE_GPU), 1)
EXTRA_CFLAGS += -DCONFIG_DOVE_GPU=1
endif

ifneq ($(USE_PLATFORM_DRIVER), 0)
EXTRA_CFLAGS += -DUSE_PLATFORM_DRIVER=1
else
EXTRA_CFLAGS += -DUSE_PLATFORM_DRIVER=0
endif

ifeq ($(USE_PROFILER), 1)
EXTRA_CFLAGS += -DVIVANTE_PROFILER=1
EXTRA_CFLAGS += -DVIVANTE_PROFILER_CONTEXT=1
else
EXTRA_CFLAGS += -DVIVANTE_PROFILER=0
EXTRA_CFLAGS += -DVIVANTE_PROFILER_CONTEXT=0
endif

ifeq ($(ANDROID), 1)
EXTRA_CFLAGS += -DANDROID=1
endif

ifeq ($(ENABLE_GPU_CLOCK_BY_DRIVER), 1)
EXTRA_CFLAGS += -DENABLE_GPU_CLOCK_BY_DRIVER=1
else
EXTRA_CFLAGS += -DENABLE_GPU_CLOCK_BY_DRIVER=0
endif

ifeq ($(USE_NEW_LINUX_SIGNAL), 1)
EXTRA_CFLAGS += -DUSE_NEW_LINUX_SIGNAL=1
else
EXTRA_CFLAGS += -DUSE_NEW_LINUX_SIGNAL=0
endif

ifeq ($(FORCE_ALL_VIDEO_MEMORY_CACHED), 1)
EXTRA_CFLAGS += -DgcdPAGED_MEMORY_CACHEABLE=1
else
EXTRA_CFLAGS += -DgcdPAGED_MEMORY_CACHEABLE=0
endif

ifeq ($(NONPAGED_MEMORY_CACHEABLE), 1)
EXTRA_CFLAGS += -DgcdNONPAGED_MEMORY_CACHEABLE=1
else
EXTRA_CFLAGS += -DgcdNONPAGED_MEMORY_CACHEABLE=0
endif

ifeq ($(NONPAGED_MEMORY_BUFFERABLE), 1)
EXTRA_CFLAGS += -DgcdNONPAGED_MEMORY_BUFFERABLE=1
else
EXTRA_CFLAGS += -DgcdNONPAGED_MEMORY_BUFFERABLE=0
endif

ifeq ($(CACHE_FUNCTION_UNIMPLEMENTED), 1)
EXTRA_CFLAGS += -DgcdCACHE_FUNCTION_UNIMPLEMENTED=1
else
EXTRA_CFLAGS += -DgcdCACHE_FUNCTION_UNIMPLEMENTED=0
endif

ifeq ($(VIVANTE_ENABLE_VG), 1)
EXTRA_CFLAGS += -DgcdENABLE_VG=1
else
EXTRA_CFLAGS += -DgcdENABLE_VG=0
endif

ifeq ($(CONFIG_SMP), y)
EXTRA_CFLAGS += -DgcdSMP=1
else
EXTRA_CFLAGS += -DgcdSMP=0
endif

ifeq ($(VIVANTE_NO_3D),1)
EXTRA_CFLAGS += -DVIVANTE_NO_3D
endif

ifeq ($(ENABLE_OUTER_CACHE_PATCH), 1)
EXTRA_CFLAGS += -DgcdENABLE_OUTER_CACHE_PATCH=1
else
EXTRA_CFLAGS += -DgcdENABLE_OUTER_CACHE_PATCH=0
endif

ifeq ($(USE_BANK_ALIGNMENT), 1)
    EXTRA_CFLAGS += -DgcdENABLE_BANK_ALIGNMENT=1
    ifneq ($(BANK_BIT_START), 0)
	        ifneq ($(BANK_BIT_END), 0)
	            EXTRA_CFLAGS += -DgcdBANK_BIT_START=$(BANK_BIT_START)
	            EXTRA_CFLAGS += -DgcdBANK_BIT_END=$(BANK_BIT_END)
	        endif
    endif

    ifneq ($(BANK_CHANNEL_BIT), 0)
        EXTRA_CFLAGS += -DgcdBANK_CHANNEL_BIT=$(BANK_CHANNEL_BIT)
    endif
endif

ifeq ($(FPGA_BUILD), 1)
EXTRA_CFLAGS += -DgcdFPGA_BUILD=1
else
EXTRA_CFLAGS += -DgcdFPGA_BUILD=0
endif


EXTRA_CFLAGS += -I$(AQROOT)/hal/inc
EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel
ifeq ($(USE_ARCH_REG), 1)
    EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/arch_reg
else
    EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/arch
endif
EXTRA_CFLAGS += -I$(AQARCH)/cmodel/inc
EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/inc
EXTRA_CFLAGS += -I$(AQROOT)/hal/os/linux/kernel

ifeq ($(VIVANTE_ENABLE_VG), 1)
    ifeq ($(USE_ARCH_REG), 1)
        EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/archvg_reg
    else
        EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/archvg
    endif
endif

obj-m = $(DRIVER_OUT_DIR)/galcore.o

$(DRIVER_OUT_DIR)/galcore-objs  = $(OBJS)

endif
