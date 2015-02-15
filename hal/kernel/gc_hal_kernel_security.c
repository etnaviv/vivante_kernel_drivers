/****************************************************************************
*
*    Copyright (c) 2005 - 2015 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#include "gc_hal_kernel_precomp.h"




#define _GC_OBJ_ZONE    gcvZONE_KERNEL

#if gcdSECURITY

/*
** Open a security service channel.
*/
gceSTATUS
gckKERNEL_SecurityOpen(
    IN gckKERNEL Kernel,
    IN gctUINT32 GPU,
    OUT gctUINT32 *Channel
    )
{
    gceSTATUS status;

    gcmkONERROR(gckOS_OpenSecurityChannel(Kernel->os, Kernel->core, Channel));
    gcmkONERROR(gckOS_InitSecurityChannel(*Channel));

    return gcvSTATUS_OK;

OnError:
    return status;
}

/*
** Close a security service channel
*/
gceSTATUS
gckKERNEL_SecurityClose(
    IN gctUINT32 Channel
    )
{
    return gcvSTATUS_OK;
}

/*
** Security service interface.
*/
gceSTATUS
gckKERNEL_SecurityCallService(
    IN gctUINT32 Channel,
    IN OUT gcsTA_INTERFACE * Interface
)
{
    gceSTATUS status;
    gcmkHEADER();

    gcmkVERIFY_ARGUMENT(Interface != gcvNULL);

    gckOS_CallSecurityService(Channel, Interface);

    status = Interface->result;

    gcmkONERROR(status);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckKERNEL_SecurityStartCommand(
    IN gckKERNEL Kernel
    )
{
    gceSTATUS status;
    gcsTA_INTERFACE iface;

    gcmkHEADER();

    iface.command = KERNEL_START_COMMAND;
    iface.u.StartCommand.gpu = Kernel->core;

    gcmkONERROR(gckKERNEL_SecurityCallService(Kernel->securityChannel, &iface));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckKERNEL_SecurityAllocateSecurityMemory(
    IN gckKERNEL Kernel,
    IN gctUINT32 Bytes,
    OUT gctUINT32 * Handle
    )
{
    gceSTATUS status;
    gcsTA_INTERFACE iface;

    gcmkHEADER();

    iface.command = KERNEL_ALLOCATE_SECRUE_MEMORY;
    iface.u.AllocateSecurityMemory.bytes = Bytes;

    gcmkONERROR(gckKERNEL_SecurityCallService(Kernel->securityChannel, &iface));

    *Handle = iface.u.AllocateSecurityMemory.memory_handle;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckKERNEL_SecurityExecute(
    IN gckKERNEL Kernel,
    IN gctPOINTER Buffer,
    IN gctUINT32 Bytes
    )
{
    gceSTATUS status;
#if defined(LINUX)
    gctPHYS_ADDR_T physical;
    gctUINT32 address;
#endif
    gcsTA_INTERFACE iface;

    gcmkHEADER();

    iface.command = KERNEL_EXECUTE;
    iface.u.Execute.command_buffer = (gctUINT32 *)Buffer;
    iface.u.Execute.gpu = Kernel->core;
    iface.u.Execute.command_buffer_length = Bytes;

#if defined(LINUX)
    gcmkONERROR(gckOS_GetPhysicalAddress(Kernel->os, Buffer, &physical));
    gcmkSAFECASTPHYSADDRT(address, physical);

    iface.u.Execute.command_buffer = (gctUINT32 *)address;
#endif

    gcmkONERROR(gckKERNEL_SecurityCallService(Kernel->securityChannel, &iface));

    /* Update queue tail pointer. */
    gcmkONERROR(gckHARDWARE_UpdateQueueTail(
        Kernel->hardware, 0, 0
        ));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckKERNEL_SecurityMapMemory(
    IN gckKERNEL Kernel,
    IN gctUINT32 *PhysicalArray,
    IN gctUINT32 PageCount,
    OUT gctUINT32 * GPUAddress
    )
{
    gceSTATUS status;
    gcsTA_INTERFACE iface;
#if defined(LINUX)
    gctPHYS_ADDR_T physical;
    gctUINT32 address;
#endif

    gcmkHEADER();

    iface.command = KERNEL_MAP_MEMORY;

#if defined(LINUX)
    gcmkONERROR(gckOS_GetPhysicalAddress(Kernel->os, PhysicalArray, &physical));
    gcmkSAFECASTPHYSADDRT(address, physical);
    iface.u.MapMemory.physicals = (gctUINT32 *)address;
#endif

    iface.u.MapMemory.pageCount = PageCount;

    gcmkONERROR(gckKERNEL_SecurityCallService(Kernel->securityChannel, &iface));

    *GPUAddress = iface.u.MapMemory.gpuAddress;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckKERNEL_SecurityUnmapMemory(
    IN gckKERNEL Kernel,
    IN gctUINT32 GPUAddress,
    IN gctUINT32 PageCount
    )
{
    gceSTATUS status;
    gcsTA_INTERFACE iface;

    gcmkHEADER();

    iface.command = KERNEL_UNMAP_MEMORY;

    iface.u.UnmapMemory.gpuAddress = GPUAddress;
    iface.u.UnmapMemory.pageCount  = PageCount;

    gcmkONERROR(gckKERNEL_SecurityCallService(Kernel->securityChannel, &iface));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

#endif
