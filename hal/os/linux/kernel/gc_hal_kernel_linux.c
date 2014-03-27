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



#include "gc_hal_kernel_linux.h"

#define _GC_OBJ_ZONE    gcvZONE_KERNEL

/******************************************************************************\
******************************* gckKERNEL API Code ******************************
\******************************************************************************/

/*******************************************************************************
**
**  gckKERNEL_QueryVideoMemory
**
**  Query the amount of video memory.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL object.
**
**  OUTPUT:
**
**      gcsHAL_INTERFACE * Interface
**          Pointer to an gcsHAL_INTERFACE structure that will be filled in with
**          the memory information.
*/
gceSTATUS
gckKERNEL_QueryVideoMemory(
    IN gckKERNEL Kernel,
    OUT gcsHAL_INTERFACE * Interface
    )
{
    gckGALDEVICE device;

    gcmkHEADER_ARG("Kernel=%p", Kernel);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Interface != NULL);

    /* Extract the pointer to the gckGALDEVICE class. */
    device = (gckGALDEVICE) Kernel->context;

    /* Get internal memory size and physical address. */
    Interface->u.QueryVideoMemory.internalSize = device->internalSize;
    Interface->u.QueryVideoMemory.internalPhysical = device->internalPhysicalName;

    /* Get external memory size and physical address. */
    Interface->u.QueryVideoMemory.externalSize = device->externalSize;
    Interface->u.QueryVideoMemory.externalPhysical = device->externalPhysicalName;

    /* Get contiguous memory size and physical address. */
    Interface->u.QueryVideoMemory.contiguousSize = device->contiguousSize;
    Interface->u.QueryVideoMemory.contiguousPhysical = device->contiguousPhysicalName;

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckKERNEL_GetVideoMemoryPool
**
**  Get the gckVIDMEM object belonging to the specified pool.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL object.
**
**      gcePOOL Pool
**          Pool to query gckVIDMEM object for.
**
**  OUTPUT:
**
**      gckVIDMEM * VideoMemory
**          Pointer to a variable that will hold the pointer to the gckVIDMEM
**          object belonging to the requested pool.
*/
gceSTATUS
gckKERNEL_GetVideoMemoryPool(
    IN gckKERNEL Kernel,
    IN gcePOOL Pool,
    OUT gckVIDMEM * VideoMemory
    )
{
    gckGALDEVICE device;
    gckVIDMEM videoMemory;

    gcmkHEADER_ARG("Kernel=%p Pool=%d", Kernel, Pool);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(VideoMemory != NULL);

    /* Extract the pointer to the gckGALDEVICE class. */
    device = (gckGALDEVICE) Kernel->context;

    /* Dispatch on pool. */
    switch (Pool)
    {
    case gcvPOOL_LOCAL_INTERNAL:
        /* Internal memory. */
        videoMemory = device->internalVidMem;
        break;

    case gcvPOOL_LOCAL_EXTERNAL:
        /* External memory. */
        videoMemory = device->externalVidMem;
        break;

    case gcvPOOL_SYSTEM:
        /* System memory. */
        videoMemory = device->contiguousVidMem;
        break;

    default:
        /* Unknown pool. */
        videoMemory = NULL;
    }

    /* Return pointer to the gckVIDMEM object. */
    *VideoMemory = videoMemory;

    /* Return status. */
    gcmkFOOTER_ARG("*VideoMemory=%p", *VideoMemory);
    return (videoMemory == NULL) ? gcvSTATUS_OUT_OF_MEMORY : gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckKERNEL_MapMemory
**
**  Map video memory into the current process space.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL object.
**
**      gctPHYS_ADDR Physical
**          Physical address of video memory to map.
**
**      gctSIZE_T Bytes
**          Number of bytes to map.
**
**  OUTPUT:
**
**      gctPOINTER * Logical
**          Pointer to a variable that will hold the base address of the mapped
**          memory region.
*/
gceSTATUS
gckKERNEL_MapMemory(
    IN gckKERNEL Kernel,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Logical
    )
{
    gckKERNEL kernel = Kernel;
    gctPHYS_ADDR physical = gcmNAME_TO_PTR(Physical);

    return gckOS_MapMemory(Kernel->os, physical, Bytes, Logical);
}

/*******************************************************************************
**
**  gckKERNEL_UnmapMemory
**
**  Unmap video memory from the current process space.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL object.
**
**      gctPHYS_ADDR Physical
**          Physical address of video memory to map.
**
**      gctSIZE_T Bytes
**          Number of bytes to map.
**
**      gctPOINTER Logical
**          Base address of the mapped memory region.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_UnmapMemory(
    IN gckKERNEL Kernel,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    IN gctPOINTER Logical
    )
{
    gckKERNEL kernel = Kernel;
    gctPHYS_ADDR physical = gcmNAME_TO_PTR(Physical);

    return gckOS_UnmapMemory(Kernel->os, physical, Bytes, Logical);
}

/*******************************************************************************
**
**  gckKERNEL_MapVideoMemory
**
**  Get the logical address for a hardware specific memory address for the
**  current process.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL object.
**
**      gctBOOL InUserSpace
**          gcvTRUE to map the memory into the user space.
**
**      gctUINT32 Address
**          Hardware specific memory address.
**
**  OUTPUT:
**
**      gctPOINTER * Logical
**          Pointer to a variable that will hold the logical address of the
**          specified memory address.
*/
gceSTATUS
gckKERNEL_MapVideoMemoryEx(
    IN gckKERNEL Kernel,
    IN gceCORE Core,
    IN gctBOOL InUserSpace,
    IN gctUINT32 Address,
    OUT gctPOINTER * Logical
    )
{
    gckGALDEVICE device   = gcvNULL;
    PLINUX_MDL mdl        = gcvNULL;
    PLINUX_MDL_MAP mdlMap = gcvNULL;
    gcePOOL pool          = gcvPOOL_UNKNOWN;
    gctUINT32 offset      = 0;
    gctUINT32 base        = 0;
    gceSTATUS status;
    gctPOINTER logical    = gcvNULL;

    gcmkHEADER_ARG("Kernel=%p InUserSpace=%d Address=%08x",
                   Kernel, InUserSpace, Address);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Logical != NULL);

    /* Extract the pointer to the gckGALDEVICE class. */
    device = (gckGALDEVICE) Kernel->context;

#if gcdENABLE_VG
    if (Core == gcvCORE_VG)
    {
        /* Split the memory address into a pool type and offset. */
        gcmkONERROR(
            gckVGHARDWARE_SplitMemory(Kernel->vg->hardware, Address, &pool, &offset));
    }
    else
#endif
    {
        /* Split the memory address into a pool type and offset. */
        gcmkONERROR(
            gckHARDWARE_SplitMemory(Kernel->hardware, Address, &pool, &offset));
    }

    /* Dispatch on pool. */
    switch (pool)
    {
    case gcvPOOL_LOCAL_INTERNAL:
        /* Internal memory. */
        logical = device->internalLogical;
        break;

    case gcvPOOL_LOCAL_EXTERNAL:
        /* External memory. */
        logical = device->externalLogical;
        break;

    case gcvPOOL_SYSTEM:
        /* System memory. */
        if (device->contiguousMapped)
        {
            logical = device->contiguousBase;
        }
        else
        {
            gctINT processID;
            gckOS_GetProcessID(&processID);

            mdl = (PLINUX_MDL) device->contiguousPhysical;

            mdlMap = FindMdlMap(mdl, processID);
            gcmkASSERT(mdlMap);

            logical = (gctPOINTER) mdlMap->vmaAddr;
        }
#if gcdENABLE_VG
        if (Core == gcvCORE_VG)
        {
            gcmkVERIFY_OK(
                gckVGHARDWARE_SplitMemory(Kernel->vg->hardware,
                                        device->contiguousVidMem->baseAddress,
                                        &pool,
                                        &base));
        }
        else
#endif
        {
            gcmkVERIFY_OK(
                gckHARDWARE_SplitMemory(Kernel->hardware,
                                        device->contiguousVidMem->baseAddress,
                                        &pool,
                                        &base));
        }
        offset -= base;
        break;

    default:
        /* Invalid memory pool. */
        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Build logical address of specified address. */
    *Logical = (gctPOINTER) ((gctUINT8_PTR) logical + offset);

    /* Success. */
    gcmkFOOTER_ARG("*Logical=%p", *Logical);
    return gcvSTATUS_OK;

OnError:
    /* Retunn the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckKERNEL_MapVideoMemory
**
**  Get the logical address for a hardware specific memory address for the
**  current process.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL object.
**
**      gctBOOL InUserSpace
**          gcvTRUE to map the memory into the user space.
**
**      gctUINT32 Address
**          Hardware specific memory address.
**
**  OUTPUT:
**
**      gctPOINTER * Logical
**          Pointer to a variable that will hold the logical address of the
**          specified memory address.
*/
gceSTATUS
gckKERNEL_MapVideoMemory(
    IN gckKERNEL Kernel,
    IN gctBOOL InUserSpace,
    IN gctUINT32 Address,
    OUT gctPOINTER * Logical
    )
{
    return gckKERNEL_MapVideoMemoryEx(Kernel, gcvCORE_MAJOR, InUserSpace, Address, Logical);
}
/*******************************************************************************
**
**  gckKERNEL_Notify
**
**  This function iscalled by clients to notify the gckKERNRL object of an event.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL object.
**
**      gceNOTIFY Notification
**          Notification event.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_Notify(
    IN gckKERNEL Kernel,
#if gcdMULTI_GPU
    IN gctUINT CoreId,
#endif
    IN gceNOTIFY Notification,
    IN gctBOOL Data
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Kernel=%p Notification=%d Data=%d",
                   Kernel, Notification, Data);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Dispatch on notifcation. */
    switch (Notification)
    {
    case gcvNOTIFY_INTERRUPT:
        /* Process the interrupt. */
#if COMMAND_PROCESSOR_VERSION > 1
        status = gckINTERRUPT_Notify(Kernel->interrupt, Data);
#else
        status = gckHARDWARE_Interrupt(Kernel->hardware,
#if gcdMULTI_GPU
                                       CoreId,
#endif
                                       Data);
#endif
        break;

    default:
        status = gcvSTATUS_OK;
        break;
    }

    /* Success. */
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckKERNEL_QuerySettings(
    IN gckKERNEL Kernel,
    OUT gcsKERNEL_SETTINGS * Settings
    )
{
    gckGALDEVICE device;

    gcmkHEADER_ARG("Kernel=%p", Kernel);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Settings != gcvNULL);

    /* Extract the pointer to the gckGALDEVICE class. */
    device = (gckGALDEVICE) Kernel->context;

    /* Fill in signal. */
    Settings->signal = device->signal;

    /* Success. */
    gcmkFOOTER_ARG("Settings->signal=%d", Settings->signal);
    return gcvSTATUS_OK;
}
