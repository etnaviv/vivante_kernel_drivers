/****************************************************************************
*
*    Copyright (c) 2005 - 2014 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/



#include <linux/device.h>
#include <linux/slab.h>

#include "gc_hal_kernel_linux.h"
#include "gc_hal_driver.h"
#include "gc_hal_kernel_sysfs.h"

#if MRVL_CONFIG_ENABLE_GPUFREQ
#include "gpufreq.h"
#endif

#if USE_PLATFORM_DRIVER
#   include <linux/platform_device.h>
#endif

#ifdef CONFIG_PXA_DVFM
#   include <mach/dvfm.h>
#   include <mach/pxa3xx_dvfm.h>
#endif

#if MRVL_GPU_RESOURCE_DT
#include <linux/of.h>
#include <linux/of_device.h>
#endif

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_DRIVER

MODULE_DESCRIPTION("Vivante Graphics Driver");
MODULE_LICENSE("GPL");

static struct class* gpuClass;

static gckGALDEVICE galDevice;

static uint major = 199;
module_param(major, uint, 0644);

#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
static int irqLine3D0 = -1;
module_param(irqLine3D0, int, 0644);

static ulong registerMemBase3D0 = 0;
module_param(registerMemBase3D0, ulong, 0644);

static ulong registerMemSize3D0 = 2 << 10;
module_param(registerMemSize3D0, ulong, 0644);

static int irqLine3D1 = -1;
module_param(irqLine3D1, int, 0644);

static ulong registerMemBase3D1 = 0;
module_param(registerMemBase3D1, ulong, 0644);

static ulong registerMemSize3D1 = 2 << 10;
module_param(registerMemSize3D1, ulong, 0644);
#else
static int irqLine = -1;
module_param(irqLine, int, 0644);

static ulong registerMemBase = 0x80000000;
module_param(registerMemBase, ulong, 0644);

static ulong registerMemSize = 2 << 10;
module_param(registerMemSize, ulong, 0644);
#endif

static int irqLine2D = -1;
module_param(irqLine2D, int, 0644);

static ulong registerMemBase2D = 0x00000000;
module_param(registerMemBase2D, ulong, 0644);

static ulong registerMemSize2D = 2 << 10;
module_param(registerMemSize2D, ulong, 0644);

static int irqLineVG = -1;
module_param(irqLineVG, int, 0644);

static ulong registerMemBaseVG = 0x00000000;
module_param(registerMemBaseVG, ulong, 0644);

static ulong registerMemSizeVG = 2 << 10;
module_param(registerMemSizeVG, ulong, 0644);

static ulong contiguousSize = 4 << 20;
module_param(contiguousSize, ulong, 0644);

static ulong contiguousBase = 0;
module_param(contiguousBase, ulong, 0644);

static long pmemSize = 0;
module_param(pmemSize, long, 0644);

static ulong bankSize = 0;
module_param(bankSize, ulong, 0644);

static int fastClear = -1;
module_param(fastClear, int, 0644);

static int compression = -1;
module_param(compression, int, 0644);

#if gcdFPGA_BUILD
static int powerManagement = 0;
#else
static int powerManagement = 1;
#endif
module_param(powerManagement, int, 0644);

static int gpuProfiler = 0;
module_param(gpuProfiler, int, 0644);

static int signal = 48;
module_param(signal, int, 0644);

static ulong baseAddress = 0;
module_param(baseAddress, ulong, 0644);

#if MRVL_PLATFORM_TTD2_FPGA
static ulong physSize = 0x20000000;
#else
static ulong physSize = 0x40000000;
#endif
module_param(physSize, ulong, 0644);

static uint logFileSize = 0;
module_param(logFileSize,uint, 0644);

static uint recovery = 1;
module_param(recovery, uint, 0644);
MODULE_PARM_DESC(recovery, "Recover GPU from stuck (1: Enable, 0: Disable)");

/* Middle needs about 40KB buffer, Maximal may need more than 200KB buffer. */
static uint stuckDump = 1;
module_param(stuckDump, uint, 0644);
MODULE_PARM_DESC(stuckDump, "Level of stuck dump content (1: Minimal, 2: Middle, 3: Maximal)");

static int showArgs = 1;
module_param(showArgs, int, 0644);

#if ENABLE_GPU_CLOCK_BY_DRIVER
#if MRVL_PLATFORM_TTD2_FPGA
    unsigned long coreClock = 50;
#elif MRVL_PLATFORM_ADIR
    unsigned long coreClock = 624;
#elif MRVL_PLATFORM_TTD2
    unsigned long coreClock = 624;
#else
    unsigned long coreClock = 533;
#endif
    module_param(coreClock, ulong, 0644);

static unsigned long coreClock2D = 312;
module_param(coreClock2D, ulong, 0644);
#endif

/******************************************************************************\
* Create a data entry system using proc for GC
\******************************************************************************/
#define MRVL_CONFIG_PROC    1

#if MRVL_CONFIG_PROC
#include <linux/proc_fs.h>

#define GC_PROC_FILE        "driver/gc"
#define _GC_OBJ_ZONE        gcvZONE_DRIVER

static struct proc_dir_entry * gc_proc_file;

/* cat /proc/driver/gc will print gc related msg */
static ssize_t gc_proc_read(
    struct file *file,
    char __user *buffer,
    size_t count,
    loff_t *offset);

/* echo xx > /proc/driver/gc set ... */
static ssize_t gc_proc_write(
    struct file *file,
    const char *buff,
    size_t len,
    loff_t *off);

static struct file_operations gc_proc_ops = {
    .read = gc_proc_read,
    .write = gc_proc_write,
};

static gceSTATUS _gc_gather_infomation(char *buf, ssize_t* length)
{
    gceSTATUS status;
    ssize_t len = 0;
    gctUINT32 pid = 0;

    /* #################### [START ==DO NOT CHANGE THE FIRST LINE== START] #################### */
    /* @Ziyi: This string is checked by skia-neon related code to identify Marvell silicon,
              please do not change it and always keep it at the first line of /proc/driver/gc ! */
    gckOS_GetProcessID(&pid);
    len += sprintf(buf+len, "[%3d] %s (%s)\n", pid, _VENDOR_STRING_, _GC_VERSION_STRING_);
    /* @Ziyi: If any change happened between these 2 comments please contact zyxu@marvell.com, Thanks. */
    /* #################### [END ====DO NOT CHANGE THE FIRST LINE==== END] #################### */

    if(1)
    {
        gctUINT32 tmpLen = 0;
        gcmkONERROR(gckOS_ShowVidMemUsage(galDevice->os, buf+len, &tmpLen));
        len += tmpLen;
    }

    *length = len;
    return gcvSTATUS_OK;

OnError:
    return status;
}

static ssize_t gc_proc_read(
    struct file *file,
    char __user *buffer,
    size_t count,
    loff_t *offset)
{
    gceSTATUS status;
    ssize_t len = 0;
    char buf[1000];

    gcmkONERROR(_gc_gather_infomation(buf, &len));

    return simple_read_from_buffer(buffer, count, offset, buf, len);

OnError:
    return 0;
}

static ssize_t gc_proc_write(
    struct file *file,
    const char *buff,
    size_t len,
    loff_t *off)
{
    char messages[256];

    if(len > 256)
        len = 256;

    if(copy_from_user(messages, buff, len))
        return -EFAULT;

    printk("\n");
    if(strncmp(messages, "reg", 3) == 0)
    {
        gctINT32 option;
        gctUINT32 idle, address, clockControl;

        sscanf(messages+3, "%d", &option);

        switch(option) {
        case 1:
            /* Read the current FE address. */
            gckOS_ReadRegisterEx(galDevice->os,
                                 galDevice->kernels[0]->hardware->core,
                                 0x00664,
                                 &address);
            gcmkPRINT("address: 0x%2x\n", address);
            break;
        case 2:
            /* Read idle register. */
            gckOS_ReadRegisterEx(galDevice->os,
                                 galDevice->kernels[0]->hardware->core,
                                 0x00004,
                                 &idle);
            gcmkPRINT("idle: 0x%2x\n", idle);
            break;
        case 3:
            gckOS_ReadRegisterEx(galDevice->os,
                                 gcvCORE_MAJOR,
                                 0x00000,
                                 &clockControl);
            gcmkPRINT("clockControl: 0x%2x\n", clockControl);
            break;
        default:
            printk(KERN_INFO "usage: echo reg [1|2|3] > /proc/driver/gc\n");
        }
    }
    else if(strncmp(messages, "help", 4) == 0)
    {
        printk("Supported options:\n"
                "dutycycle       measure dutycycle in a period\n"
                "reg             enable debugging for register reading\n"
                "help            show this help page\n"
                "\n");
    }
    else
    {
        gcmkPRINT("unknown echo\n");
    }
    printk("\n");

    return len;
}

static void create_gc_proc_file(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
    gc_proc_file = proc_create(GC_PROC_FILE, 0644, gcvNULL, &gc_proc_ops);

    if(!gc_proc_file)
#else
    gc_proc_file = create_proc_entry(GC_PROC_FILE, 0644, gcvNULL);
    if (gc_proc_file) {
        gc_proc_file->proc_fops = &gc_proc_ops;
    } else
#endif
    gcmkPRINT("[galcore] proc file create failed!\n");
}

static void remove_gc_proc_file(void)
{
    if(gc_proc_file)
        remove_proc_entry(GC_PROC_FILE, gcvNULL);
}

#endif

static int drv_open(
    struct inode* inode,
    struct file* filp
    );

static int drv_release(
    struct inode* inode,
    struct file* filp
    );

static long drv_ioctl(
    struct file* filp,
    unsigned int ioctlCode,
    unsigned long arg
    );

static int drv_mmap(
    struct file* filp,
    struct vm_area_struct* vma
    );

static struct file_operations driver_fops =
{
    .owner      = THIS_MODULE,
    .open       = drv_open,
    .release    = drv_release,
    .unlocked_ioctl = drv_ioctl,
#ifdef HAVE_COMPAT_IOCTL
    .compat_ioctl = drv_ioctl,
#endif
    .mmap       = drv_mmap,
};

void
gckOS_DumpParam(
    void
    )
{
    printk("Galcore options:\n");
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    printk("  irqLine3D0         = %d\n",      irqLine3D0);
    printk("  registerMemBase3D0 = 0x%08lX\n", registerMemBase3D0);
    printk("  registerMemSize3D0 = 0x%08lX\n", registerMemSize3D0);

    if (irqLine3D1 != -1)
    {
        printk("  irqLine3D1         = %d\n",      irqLine3D1);
        printk("  registerMemBase3D1 = 0x%08lX\n", registerMemBase3D1);
        printk("  registerMemSize3D1 = 0x%08lX\n", registerMemSize3D1);
    }
#else
    printk("  irqLine           = %d\n",      irqLine);
    printk("  registerMemBase   = 0x%08lX\n", registerMemBase);
    printk("  registerMemSize   = 0x%08lX\n", registerMemSize);
#endif

    if (irqLine2D != -1)
    {
        printk("  irqLine2D         = %d\n",      irqLine2D);
        printk("  registerMemBase2D = 0x%08lX\n", registerMemBase2D);
        printk("  registerMemSize2D = 0x%08lX\n", registerMemSize2D);
    }

    if (irqLineVG != -1)
    {
        printk("  irqLineVG         = %d\n",      irqLineVG);
        printk("  registerMemBaseVG = 0x%08lX\n", registerMemBaseVG);
        printk("  registerMemSizeVG = 0x%08lX\n", registerMemSizeVG);
    }

    printk("  contiguousSize    = %ld\n",     contiguousSize);
    printk("  contiguousBase    = 0x%08lX\n", contiguousBase);
    printk("  bankSize          = 0x%08lX\n", bankSize);
    printk("  fastClear         = %d\n",      fastClear);
    printk("  compression       = %d\n",      compression);
    printk("  signal            = %d\n",      signal);
    printk("  powerManagement   = %d\n",      powerManagement);
    printk("  baseAddress       = 0x%08lX\n", baseAddress);
    printk("  physSize          = 0x%08lX\n", physSize);
    printk("  logFileSize       = %d KB \n",  logFileSize);
    printk("  recovery          = %d\n",      recovery);
    printk("  stuckDump         = %d\n",      stuckDump);
#if ENABLE_GPU_CLOCK_BY_DRIVER
    printk("  coreClock         = %lu\n",     coreClock);
#endif
    printk("  gpuProfiler       = %d\n",      gpuProfiler);
}

int drv_open(
    struct inode* inode,
    struct file* filp
    )
{
    gceSTATUS status;
    gctBOOL attached = gcvFALSE;
    gcsHAL_PRIVATE_DATA_PTR data = gcvNULL;
    gctINT i;

    gcmkHEADER_ARG("inode=0x%08X filp=0x%08X", inode, filp);

    if (filp == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): filp is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    data = kmalloc(sizeof(gcsHAL_PRIVATE_DATA), GFP_KERNEL | __GFP_NOWARN);

    if (data == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): private_data is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    data->device             = galDevice;
    data->mappedMemory       = gcvNULL;
    data->contiguousLogical  = gcvNULL;
    gcmkONERROR(gckOS_GetProcessID(&data->pidOpen));

    /* Attached the process. */
    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gcmkONERROR(gckKERNEL_AttachProcess(galDevice->kernels[i], gcvTRUE));
        }
    }
    attached = gcvTRUE;

    if (!galDevice->contiguousMapped)
    {
        if (galDevice->contiguousPhysical != gcvNULL)
        {
            gcmkONERROR(gckOS_MapMemory(
                galDevice->os,
                galDevice->contiguousPhysical,
                galDevice->contiguousSize,
                &data->contiguousLogical
                ));
        }
    }

    filp->private_data = data;

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    if (data != gcvNULL)
    {
        if (data->contiguousLogical != gcvNULL)
        {
            gcmkVERIFY_OK(gckOS_UnmapMemory(
                galDevice->os,
                galDevice->contiguousPhysical,
                galDevice->contiguousSize,
                data->contiguousLogical
                ));
        }

        kfree(data);
    }

    if (attached)
    {
        for (i = 0; i < gcdMAX_GPU_COUNT; i++)
        {
            if (galDevice->kernels[i] != gcvNULL)
            {
                gcmkVERIFY_OK(gckKERNEL_AttachProcess(galDevice->kernels[i], gcvFALSE));
            }
        }
    }

    gcmkFOOTER();
    return -ENOTTY;
}

int drv_release(
    struct inode* inode,
    struct file* filp
    )
{
    gceSTATUS status;
    gcsHAL_PRIVATE_DATA_PTR data;
    gckGALDEVICE device;
    gctINT i;

    gcmkHEADER_ARG("inode=0x%08X filp=0x%08X", inode, filp);

    if (filp == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): filp is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    data = filp->private_data;

    if (data == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): private_data is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    device = data->device;

    if (device == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): device is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if (!device->contiguousMapped)
    {
        if (data->contiguousLogical != gcvNULL)
        {
            gcmkONERROR(gckOS_UnmapMemoryEx(
                galDevice->os,
                galDevice->contiguousPhysical,
                galDevice->contiguousSize,
                data->contiguousLogical,
                data->pidOpen
                ));

            data->contiguousLogical = gcvNULL;
        }
    }

    /* A process gets detached. */
    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gcmkONERROR(gckKERNEL_AttachProcessEx(galDevice->kernels[i], gcvFALSE, data->pidOpen));
        }
    }

    kfree(data);
    filp->private_data = NULL;

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    gcmkFOOTER();
    return -ENOTTY;
}

long drv_ioctl(
    struct file* filp,
    unsigned int ioctlCode,
    unsigned long arg
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;
    gctUINT32 copyLen;
    DRIVER_ARGS drvArgs;
    gckGALDEVICE device;
    gcsHAL_PRIVATE_DATA_PTR data;
    gctINT32 i, count;
    gckVIDMEM_NODE nodeObject;

    gcmkHEADER_ARG(
        "filp=0x%08X ioctlCode=0x%08X arg=0x%08X",
        filp, ioctlCode, arg
        );

    if (filp == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): filp is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    data = filp->private_data;

    if (data == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): private_data is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    device = data->device;

    if (device == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): device is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if ((ioctlCode != IOCTL_GCHAL_INTERFACE)
    &&  (ioctlCode != IOCTL_GCHAL_KERNEL_INTERFACE)
    )
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): unknown command %d\n",
            __FUNCTION__, __LINE__,
            ioctlCode
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Get the drvArgs. */
    copyLen = copy_from_user(
        &drvArgs, (void *) arg, sizeof(DRIVER_ARGS)
        );

    if (copyLen != 0)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): error copying of the input arguments.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Now bring in the gcsHAL_INTERFACE structure. */
    if ((drvArgs.InputBufferSize  != sizeof(gcsHAL_INTERFACE))
    ||  (drvArgs.OutputBufferSize != sizeof(gcsHAL_INTERFACE))
    )
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): input %lu or/and output %lu structures are invalid with %lu.\n",
            __FUNCTION__, __LINE__,drvArgs.InputBufferSize,drvArgs.OutputBufferSize,sizeof(gcsHAL_INTERFACE)
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    copyLen = copy_from_user(
        &iface, gcmUINT64_TO_PTR(drvArgs.InputBuffer), sizeof(gcsHAL_INTERFACE)
        );

    if (copyLen != 0)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): error copying of input HAL interface.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if (iface.command == gcvHAL_CHIP_INFO)
    {
        count = 0;
        for (i = 0; i < gcdMAX_GPU_COUNT; i++)
        {
            if (device->kernels[i] != gcvNULL)
            {
#if gcdENABLE_VG
                if (i == gcvCORE_VG)
                {
                    iface.u.ChipInfo.types[count] = gcvHARDWARE_VG;
                }
                else
#endif
                {
                    gcmkVERIFY_OK(gckHARDWARE_GetType(device->kernels[i]->hardware,
                                                      &iface.u.ChipInfo.types[count]));
                }
                count++;
            }
        }

        iface.u.ChipInfo.count = count;
        iface.status = status = gcvSTATUS_OK;
    }
    else
    {
        if (iface.hardwareType > 7)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_ERROR, gcvZONE_DRIVER,
                "%s(%d): unknown hardwareType %d\n",
                __FUNCTION__, __LINE__,
                iface.hardwareType
                );

            gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

#if gcdENABLE_VG
        if (device->coreMapping[iface.hardwareType] == gcvCORE_VG)
        {
            status = gckVGKERNEL_Dispatch(device->kernels[gcvCORE_VG],
                                        (ioctlCode == IOCTL_GCHAL_INTERFACE),
                                        &iface);
        }
        else
#endif
        {
            status = gckKERNEL_Dispatch(device->kernels[device->coreMapping[iface.hardwareType]],
                                        (ioctlCode == IOCTL_GCHAL_INTERFACE),
                                        &iface);
        }
    }

    /* Redo system call after pending signal is handled. */
    if (status == gcvSTATUS_INTERRUPTED)
    {
        gcmkFOOTER();
        return -ERESTARTSYS;
    }

    if (gcmIS_SUCCESS(status) && (iface.command == gcvHAL_LOCK_VIDEO_MEMORY))
    {
        gcuVIDMEM_NODE_PTR node;
        gctUINT32 processID;

        gckOS_GetProcessID(&processID);

        gcmkONERROR(gckVIDMEM_HANDLE_Lookup(device->kernels[device->coreMapping[iface.hardwareType]],
                                processID,
                                (gctUINT32)iface.u.LockVideoMemory.node,
                                &nodeObject));
        node = nodeObject->node;

        /* Special case for mapped memory. */
        if ((data->mappedMemory != gcvNULL)
        &&  (node->VidMem.memory->object.type == gcvOBJ_VIDMEM)
        )
        {
            /* Compute offset into mapped memory. */
            gctUINT32 offset
                = (gctUINT8 *) gcmUINT64_TO_PTR(iface.u.LockVideoMemory.memory)
                - (gctUINT8 *) device->contiguousBase;

            /* Compute offset into user-mapped region. */
            iface.u.LockVideoMemory.memory =
                gcmPTR_TO_UINT64((gctUINT8 *) data->mappedMemory + offset);
        }
    }

    /* Copy data back to the user. */
    copyLen = copy_to_user(
        gcmUINT64_TO_PTR(drvArgs.OutputBuffer), &iface, sizeof(gcsHAL_INTERFACE)
        );

    if (copyLen != 0)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): error copying of output HAL interface.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    gcmkFOOTER();
    return -ENOTTY;
}

static int drv_mmap(
    struct file* filp,
    struct vm_area_struct* vma
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHAL_PRIVATE_DATA_PTR data;
    gckGALDEVICE device;

    gcmkHEADER_ARG("filp=0x%08X vma=0x%08X", filp, vma);

    if (filp == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): filp is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    data = filp->private_data;

    if (data == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): private_data is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    device = data->device;

    if (device == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): device is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

#if !gcdPAGED_MEMORY_CACHEABLE
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    vma->vm_flags    |= gcdVM_FLAGS;
#endif
    vma->vm_pgoff     = 0;

    if (device->contiguousMapped)
    {
        unsigned long size = vma->vm_end - vma->vm_start;
        int ret = 0;

        if (size > device->contiguousSize)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_ERROR, gcvZONE_DRIVER,
                "%s(%d): Invalid mapping size.\n",
                __FUNCTION__, __LINE__
                );

            gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        ret = io_remap_pfn_range(
            vma,
            vma->vm_start,
            device->requestedContiguousBase >> PAGE_SHIFT,
            size,
            vma->vm_page_prot
            );

        if (ret != 0)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_ERROR, gcvZONE_DRIVER,
                "%s(%d): io_remap_pfn_range failed %d\n",
                __FUNCTION__, __LINE__,
                ret
                );

            data->mappedMemory = gcvNULL;

            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
        }

        data->mappedMemory = (gctPOINTER) vma->vm_start;

        /* Success. */
        gcmkFOOTER_NO();
        return 0;
    }

OnError:
    gcmkFOOTER();
    return -ENOTTY;
}


#if !USE_PLATFORM_DRIVER
static int __init drv_init(void)
#else
static int drv_init(void)
#endif
{
    int ret, i;
    int result = -EINVAL;
    gceSTATUS status;
    gckGALDEVICE device = gcvNULL;
    struct class* device_class = gcvNULL;

    gcsDEVICE_CONSTRUCT_ARGS args = {
        .recovery  = recovery,
        .stuckDump = stuckDump,
    };

    gcmkHEADER();

# if MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK /* == 1 */

#if ENABLE_GPU_CLOCK_BY_DRIVER && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    {
        gcmkONERROR(gckOS_SetGPUPowerOnBeforeInit(gcvCORE_MAJOR, gcvTRUE, gcvTRUE, coreClock * 1000));

#   if MRVL_2D3D_CLOCK_SEPARATED
        gcmkONERROR(gckOS_SetGPUPowerOnBeforeInit(gcvCORE_2D, gcvTRUE, gcvTRUE, coreClock2D * 1000));
#   endif
    }
#endif

# else /* MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK == 0 */

#if ENABLE_GPU_CLOCK_BY_DRIVER && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
    gcmkONERROR(gckOS_GpuPowerEnable(gcvNULL, gcvCORE_MAJOR, gcvTRUE, gcvTRUE, coreClock*1000*1000));

#  if MRVL_2D3D_CLOCK_SEPARATED
#    if MRVL_2D3D_POWER_SEPARATED
        /* PXA1L88/TTD2: enable 2D power, too */
        gcmkONERROR(gckOS_GpuPowerEnable(gcvNULL, gcvCORE_2D, gcvTRUE, gcvTRUE, coreClock*1000*1000));
#    else
        /* PXA1088: 2D power is shared with 3D */
        gcmkONERROR(gckOS_GpuPowerEnable(gcvNULL, gcvCORE_2D, gcvTRUE, gcvFALSE, coreClock*1000*1000));
#    endif
#  endif

#endif

# endif /* MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK */


    printk(KERN_INFO "Galcore version %d.%d.%d.%d\n",
        gcvVERSION_MAJOR, gcvVERSION_MINOR, gcvVERSION_PATCH, gcvVERSION_BUILD);
    /* when enable gpu profiler, we need to turn off gpu powerMangement */
    if(gpuProfiler)
        powerManagement = 0;
    if (showArgs)
    {
        gckOS_DumpParam();
    }

#if MRVL_MMU_FLATMAP_2G_ADDRESS
    physSize = 0x80000000;
    printk("  physSize modified to          = 0x%08lX\n", physSize);
#endif

    if(logFileSize != 0)
    {
        gckDEBUGFS_Initialize();
    }

    /* Create the GAL device. */
    status = gckGALDEVICE_Construct(
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
        irqLine3D0,
        registerMemBase3D0, registerMemSize3D0,
        irqLine3D1,
        registerMemBase3D1, registerMemSize3D1,
#else
        irqLine,
        registerMemBase, registerMemSize,
#endif
        irqLine2D,
        registerMemBase2D, registerMemSize2D,
        irqLineVG,
        registerMemBaseVG, registerMemSizeVG,
        contiguousBase, contiguousSize,
        bankSize, fastClear, compression, baseAddress, physSize, signal,
        logFileSize,
        powerManagement,
        gpuProfiler,
        &args,
        &device
    );

    if (gcmIS_ERROR(status))
    {
        gcmkTRACE_ZONE(gcvLEVEL_ERROR, gcvZONE_DRIVER,
                       "%s(%d): Failed to create the GAL device: status=%d\n",
                       __FUNCTION__, __LINE__, status);

        goto OnError;
    }

    /* Start the GAL device. */
    gcmkONERROR(gckGALDEVICE_Start(device));

    if ((physSize != 0)
       && (device->kernels[gcvCORE_MAJOR] != gcvNULL)
       && (device->kernels[gcvCORE_MAJOR]->hardware->mmuVersion != 0))
    {
        status = gckMMU_Enable(device->kernels[gcvCORE_MAJOR]->mmu, baseAddress, physSize);
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DRIVER,
            "Enable new MMU: status=%d\n", status);

#if gcdMULTI_GPU_AFFINITY
        status = gckMMU_Enable(device->kernels[gcvCORE_OCL]->mmu, baseAddress, physSize);
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DRIVER,
            "Enable new MMU: status=%d\n", status);
#endif

        if ((device->kernels[gcvCORE_2D] != gcvNULL)
            && (device->kernels[gcvCORE_2D]->hardware->mmuVersion != 0))
        {
            status = gckMMU_Enable(device->kernels[gcvCORE_2D]->mmu, baseAddress, physSize);
            gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DRIVER,
                "Enable new MMU for 2D: status=%d\n", status);
        }

        /* Reset the base address */
        device->baseAddress = 0;
    }

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(device->kernels[i] != gcvNULL)
        {
            device->kernels[i]->bTryIdleGPUEnable = gcvTRUE;
        }
    }

    /* Register the character device. */
    ret = register_chrdev(major, DEVICE_NAME, &driver_fops);

    if (ret < 0)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): Could not allocate major number for mmap.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    if (major == 0)
    {
        major = ret;
    }

    /* Create the device class. */
    device_class = class_create(THIS_MODULE, "graphics_class");

    if (IS_ERR(device_class))
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): Failed to create the class.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
    device_create(device_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
#else
    device_create(device_class, NULL, MKDEV(major, 0), DEVICE_NAME);
#endif

    galDevice = device;
    gpuClass  = device_class;

#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    gcmkTRACE_ZONE(
        gcvLEVEL_INFO, gcvZONE_DRIVER,
        "%s(%d): irqLine3D0=%d, contiguousSize=%lu, memBase3D0=0x%lX\n",
        __FUNCTION__, __LINE__,
        irqLine3D0, contiguousSize, registerMemBase3D0
        );
#else
    gcmkTRACE_ZONE(
        gcvLEVEL_INFO, gcvZONE_DRIVER,
        "%s(%d): irqLine=%d, contiguousSize=%lu, memBase=0x%lX\n",
        __FUNCTION__, __LINE__,
        irqLine, contiguousSize, registerMemBase
        );
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    /* Roll back. */
    if (device_class != gcvNULL)
    {
        device_destroy(device_class, MKDEV(major, 0));
        class_destroy(device_class);
    }

    if (device != gcvNULL)
    {
        gcmkVERIFY_OK(gckGALDEVICE_Stop(device));
        gcmkVERIFY_OK(gckGALDEVICE_Destroy(device));
    }

    gcmkFOOTER();
    return result;
}

#if !USE_PLATFORM_DRIVER
static void __exit drv_exit(void)
#else
static void drv_exit(void)
#endif
{
    gcmkHEADER();

    gcmkASSERT(gpuClass != gcvNULL);
    device_destroy(gpuClass, MKDEV(major, 0));
    class_destroy(gpuClass);

    unregister_chrdev(major, DEVICE_NAME);

    gcmkVERIFY_OK(gckGALDEVICE_Stop(galDevice));
    gcmkVERIFY_OK(gckGALDEVICE_Destroy(galDevice));

    if(gckDEBUGFS_IsEnabled())
    {
        gckDEBUGFS_Terminate();
    }

# if MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK  /* == 1 */

#if ENABLE_GPU_CLOCK_BY_DRIVER && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    {
#   if MRVL_2D3D_CLOCK_SEPARATED
        gcmkVERIFY_OK(gckOS_SetGPUPowerOffMRVL(galDevice->os, gcvCORE_2D, gcvTRUE, gcvTRUE));
#   endif
        gcmkVERIFY_OK(gckOS_SetGPUPowerOffMRVL(galDevice->os, gcvCORE_MAJOR, gcvTRUE, gcvTRUE));
    }
#endif

# else /* MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK == 0 */

#if ENABLE_GPU_CLOCK_BY_DRIVER && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    {

#  if MRVL_2D3D_CLOCK_SEPARATED
#    if MRVL_2D3D_POWER_SEPARATED
        /* PXA1L88/TTD2 */
        gcmkVERIFY_OK(gckOS_GpuPowerDisable(galDevice->os, gcvCORE_2D, gcvTRUE, gcvTRUE));
#    else
        /* PXA1088 */
        gcmkVERIFY_OK(gckOS_GpuPowerDisable(galDevice->os, gcvCORE_2D, gcvTRUE, gcvFALSE));
#    endif
#  endif

        gcmkVERIFY_OK(gckOS_GpuPowerDisable(galDevice->os, gcvCORE_MAJOR, gcvTRUE, gcvTRUE));
    }
#endif

# endif /* MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK */

    gcmkFOOTER_NO();
}

#if !USE_PLATFORM_DRIVER
    module_init(drv_init);
    module_exit(drv_exit);
#else

#if MRVL_CONFIG_ENABLE_GPUFREQ
static int __enable_gpufreq(gckGALDEVICE device)
{
    gctUINT32 clockRate = 0;
    gceSTATUS status;
    int i;

    status = gckOS_QueryClkRate(device->os, gcvCORE_MAJOR, &clockRate);

    if(gcmIS_SUCCESS(status) && clockRate != 0)
    {
        gpufreq_init(device->os);
        printk("[galcore] gpufreq inited\n");
    }

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(device->kernels[i] != gcvNULL)
        {
            gckHARDWARE hardware = device->kernels[i]->hardware;

            if(!hardware->devObj.kobj)
                continue;

            gckOS_GPUFreqNotifierCallChain(
                            device->os,
                            GPUFREQ_GPU_EVENT_INIT,
                            (gctPOINTER) &hardware->devObj);
        }
    }

    return 0;
}

static int __disable_gpufreq(gckGALDEVICE device)
{
    gctUINT32 clockRate = 0;
    gceSTATUS status;
    int i;

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(device->kernels[i] != gcvNULL)
        {
            gckHARDWARE hardware = device->kernels[i]->hardware;

            if(!hardware->devObj.kobj)
                continue;

            gckOS_GPUFreqNotifierCallChain(
                            device->os,
                            GPUFREQ_GPU_EVENT_DESTORY,
                            (gctPOINTER) &hardware->devObj);
        }
    }

    status = gckOS_QueryClkRate(device->os, gcvCORE_MAJOR, &clockRate);

    if(gcmIS_SUCCESS(status) && clockRate != 0)
    {
        gpufreq_exit(device->os);
        printk("[galcore] gpufreq exited\n");
    }

    return 0;
}
#endif

#if MRVL_GPU_RESOURCE_DT
static int gpu_dt_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    gctUINT32 memBase = 0;
    gctUINT32 memSize = 0;

    if (of_property_read_u32(np, "gpu-mem-base", &memBase))
    {
        gcmkPRINT("continuous memory base address missing");
        return -EINVAL;
    }
    contiguousBase = memBase;

    if (of_property_read_u32(np, "gpu-mem-size", &memSize))
    {
        gcmkPRINT("continuous memory size missing");
        return -EINVAL;
    }
    contiguousSize = memSize;

    return 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
static int gpu_probe(struct platform_device *pdev)
#else
static int __devinit gpu_probe(struct platform_device *pdev)
#endif
{
    int ret = -ENODEV;
#if !MRVL_GPU_RESOURCE_DT
    struct resource* res;
#endif

    gcmkHEADER();

#if MRVL_GPU_RESOURCE_DT
    gcmkPRINT(KERN_INFO "[galcore] info: GC use DT to reserve video memory.\n");
    if (gpu_dt_probe(pdev))
    {
        gcmkPRINT(KERN_ERR "%s: No continuous memory supplied in Device Tree.\n",__FUNCTION__);
        goto gpu_probe_fail;
    }
#else
    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "gpu_irq");

    if (!res)
    {
        printk(KERN_ERR "%s: No irq line supplied.\n",__FUNCTION__);
        goto gpu_probe_fail;
    }

#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    irqLine3D0 = res->start;
#else
    irqLine = res->start;
#endif

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_base");

    if (!res)
    {
        printk(KERN_ERR "%s: No register base supplied.\n",__FUNCTION__);
        goto gpu_probe_fail;
    }

#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    registerMemBase3D0 = res->start;
    registerMemSize3D0 = res->end - res->start + 1;
#else
    registerMemBase = res->start;
    registerMemSize = res->end - res->start + 1;
#endif

#if MRVL_USE_GPU_RESERVE_MEM
    gcmkPRINT(KERN_INFO "[galcore] info: GC use memblock to reserve video memory.\n");
    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_mem");

    if (!res)
    {
        printk(KERN_ERR "%s: No memory base supplied.\n",__FUNCTION__);
        goto gpu_probe_fail;
    }

    contiguousBase = res->start;
    contiguousSize = res->end - res->start + 1;
#endif
#endif

    printk("\n[galcore] GC Version: %s\n", _GC_VERSION_STRING_);
    printk("\ncontiguousBase: 0x%08x, contiguousSize: 0x%08x\n",
        (gctUINT32)contiguousBase, (gctUINT32)contiguousSize);

    ret = drv_init();

    if (!ret)
    {
        platform_set_drvdata(pdev, galDevice);

#if MRVL_CONFIG_PROC
        create_gc_proc_file();
#endif

        create_gc_sysfs_file(pdev);

#if MRVL_CONFIG_ENABLE_GPUFREQ
        __enable_gpufreq(galDevice);
#endif

        gcmkFOOTER_NO();
        return ret;
    }

gpu_probe_fail:
    gcmkFOOTER_ARG(KERN_INFO "Failed to register gpu driver: %d\n", ret);
    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
static int gpu_remove(struct platform_device *pdev)
#else
static int __devexit gpu_remove(struct platform_device *pdev)
#endif
{
    gcmkHEADER();

#if MRVL_CONFIG_ENABLE_GPUFREQ
        __disable_gpufreq(galDevice);
#endif

    remove_gc_sysfs_file(pdev);

    drv_exit();

#if MRVL_CONFIG_PROC
    remove_gc_proc_file();
#endif

    gcmkFOOTER_NO();
    return 0;
}

static int gpu_suspend(struct platform_device *dev, pm_message_t state)
{
    gceSTATUS status;
    gckGALDEVICE device;
    gctINT i;

    device = platform_get_drvdata(dev);

    if (!device)
    {
        return -1;
    }

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (device->kernels[i] != gcvNULL)
        {
            /* Store states. */
#if gcdENABLE_VG
            if (i == gcvCORE_VG)
            {
                status = gckVGHARDWARE_QueryPowerManagementState(device->kernels[i]->vg->hardware, &device->statesStored[i]);
            }
            else
#endif
            {
                status = gckHARDWARE_QueryPowerManagementState(device->kernels[i]->hardware, &device->statesStored[i]);
            }

            if (gcmIS_ERROR(status))
            {
                return -1;
            }

#if gcdENABLE_VG
            if (i == gcvCORE_VG)
            {
                status = gckVGHARDWARE_SetPowerManagementState(device->kernels[i]->vg->hardware, gcvPOWER_OFF);
            }
            else
#endif
            {
                status = gckHARDWARE_SetPowerManagementState(device->kernels[i]->hardware, gcvPOWER_OFF);
            }

            if (gcmIS_ERROR(status))
            {
                return -1;
            }

        }
    }

    return 0;
}

static int gpu_resume(struct platform_device *dev)
{
    gceSTATUS status;
    gckGALDEVICE device;
    gctINT i;
    gceCHIPPOWERSTATE   statesStored;

    device = platform_get_drvdata(dev);

    if (!device)
    {
        return -1;
    }

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (device->kernels[i] != gcvNULL)
        {
#if gcdENABLE_VG
            if (i == gcvCORE_VG)
            {
                status = gckVGHARDWARE_SetPowerManagementState(device->kernels[i]->vg->hardware, gcvPOWER_ON);
            }
            else
#endif
            {
                status = gckHARDWARE_SetPowerManagementState(device->kernels[i]->hardware, gcvPOWER_ON);
            }

            if (gcmIS_ERROR(status))
            {
                return -1;
            }

            /* Convert global state to crossponding internal state. */
            switch(device->statesStored[i])
            {
            case gcvPOWER_OFF:
                statesStored = gcvPOWER_OFF_BROADCAST;
                break;
            case gcvPOWER_IDLE:
                statesStored = gcvPOWER_IDLE_BROADCAST;
                break;
            case gcvPOWER_SUSPEND:
                statesStored = gcvPOWER_SUSPEND_BROADCAST;
                break;
            case gcvPOWER_ON:
                statesStored = gcvPOWER_ON_AUTO;
                break;
            default:
                statesStored = device->statesStored[i];
                break;
            }

            /* Restore states. */
#if gcdENABLE_VG
            if (i == gcvCORE_VG)
            {
                status = gckVGHARDWARE_SetPowerManagementState(device->kernels[i]->vg->hardware, statesStored);
            }
            else
#endif
            {
                status = gckHARDWARE_SetPowerManagementState(device->kernels[i]->hardware, statesStored);
            }

            if (gcmIS_ERROR(status))
            {
                return -1;
            }
        }
    }

    return 0;
}

#if MRVL_GPU_RESOURCE_DT
static struct of_device_id gpu_dt_ids[] = {
    { .compatible = "marvell,gpu", },
    {}
};
#endif

static struct platform_driver gpu_driver = {
    .probe      = gpu_probe,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
    .remove     = gpu_remove,
#else
    .remove     = __devexit_p(gpu_remove),
#endif

    .suspend    = gpu_suspend,
    .resume     = gpu_resume,

    .driver     = {
        .name   = DEVICE_NAME,
#if MRVL_GPU_RESOURCE_DT
        .of_match_table = of_match_ptr(gpu_dt_ids),
#endif
    }
};

#ifndef CONFIG_DOVE_GPU
#if !MRVL_USE_GPU_RESERVE_MEM
/* gpu_resources are put in kernel when using memblock mechanism */
static struct resource gpu_resources[] = {
    {
        .name   = "gpu_irq",
        .flags  = IORESOURCE_IRQ,
    },
    {
        .name   = "gpu_base",
        .flags  = IORESOURCE_MEM,
    },
};

static struct platform_device * gpu_device;
#endif
#endif

static int __init gpu_init(void)
{
    int ret = 0;

#ifndef CONFIG_DOVE_GPU
# if !MRVL_USE_GPU_RESERVE_MEM
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    gpu_resources[0].start = gpu_resources[0].end = irqLine3D0;

    gpu_resources[1].start = registerMemBase3D0;
    gpu_resources[1].end   = registerMemBase3D0 + registerMemSize3D0 - 1;
#else
    gpu_resources[0].start = gpu_resources[0].end = irqLine;

    gpu_resources[1].start = registerMemBase;
    gpu_resources[1].end   = registerMemBase + registerMemSize - 1;
#endif

    /* Allocate device */
    gpu_device = platform_device_alloc(DEVICE_NAME, -1);
    if (!gpu_device)
    {
        printk(KERN_ERR "galcore: platform_device_alloc failed.\n");
        ret = -ENOMEM;
        goto out;
    }

    /* Insert resource */
    ret = platform_device_add_resources(gpu_device, gpu_resources, 2);
    if (ret)
    {
        printk(KERN_ERR "galcore: platform_device_add_resources failed.\n");
        goto put_dev;
    }

    /* Add device */
    ret = platform_device_add(gpu_device);
    if (ret)
    {
        printk(KERN_ERR "galcore: platform_device_add failed.\n");
        goto put_dev;
    }
# endif
#endif

    ret = platform_driver_register(&gpu_driver);
    if (!ret)
    {
        goto out;
    }

#ifndef CONFIG_DOVE_GPU
# if !MRVL_USE_GPU_RESERVE_MEM
    platform_device_del(gpu_device);
put_dev:
    platform_device_put(gpu_device);
# endif
#endif

out:
    return ret;
}

static void __exit gpu_exit(void)
{
    platform_driver_unregister(&gpu_driver);
#ifndef CONFIG_DOVE_GPU
# if !MRVL_USE_GPU_RESERVE_MEM
    platform_device_unregister(gpu_device);
# endif
#endif
}

module_init(gpu_init);
module_exit(gpu_exit);

#endif
