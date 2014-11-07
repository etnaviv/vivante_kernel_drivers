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


#ifndef _gc_hal_kernel_platform_h_
#define _gc_hal_kernel_platform_h_
#include <linux/mm.h>

typedef struct _gcsMODULE_PARAMETERS
{
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    gctINT  irqLine3D0;
    gctUINT registerMemBase3D0;
    gctUINT registerMemSize3D0;
    gctINT  irqLine3D1;
    gctUINT registerMemBase3D1;
    gctUINT registerMemSize3D1;
#else
    gctINT  irqLine;
    gctUINT registerMemBase;
    gctUINT registerMemSize;
#endif
    gctINT  irqLine2D;
    gctUINT registerMemBase2D;
    gctUINT registerMemSize2D;
    gctINT  irqLineVG;
    gctUINT registerMemBaseVG;
    gctUINT registerMemSizeVG;
    gctUINT contiguousSize;
    gctUINT contiguousBase;
    gctUINT contiguousRequested;
    gctUINT bankSize;
    gctINT  fastClear;
    gctINT  compression;
    gctINT  powerManagement;
    gctINT  gpuProfiler;
    gctINT  signal;
    gctUINT baseAddress;
    gctUINT physSize;
    gctUINT logFileSize;
    gctUINT recovery;
    gctUINT stuckDump;
    gctUINT showArgs;
    gctUINT gpu3DMinClock;
}
gcsMODULE_PARAMETERS;

typedef struct _gcsPLATFORM * gckPLATFORM;

typedef struct _gcsPLATFORM_OPERATIONS
{
    /*******************************************************************************
    **
    **  needAddDevice
    **
    **  Determine whether platform_device is created by initialization code.
    **  If platform_device is created by BSP, return gcvFLASE here.
    */
    gctBOOL
    (*needAddDevice)(
        IN gckPLATFORM Platform
        );

    /*******************************************************************************
    **
    **  adjustParam
    **
    **  Override content of arguments, if a argument is not changed here, it will
    **  keep as default value or value set by insmod command line.
    */
    gceSTATUS
    (*adjustParam)(
        IN gckPLATFORM Platform,
        OUT gcsMODULE_PARAMETERS *Args
        );

    /*******************************************************************************
    **
    **  adjustDriver
    **
    **  Override content of platform_driver which will be registered.
    */
    gceSTATUS
    (*adjustDriver)(
        IN gckPLATFORM Platform
        );

    /*******************************************************************************
    **
    **  getPower
    **
    **  Prepare power and clock operation.
    */
    gceSTATUS
    (*getPower)(
        IN gckPLATFORM Platform
        );

    /*******************************************************************************
    **
    **  putPower
    **
    **  Finish power and clock operation.
    */
    gceSTATUS
    (*putPower)(
        IN gckPLATFORM Platform
        );

    /*******************************************************************************
    **
    **  allocPriv
    **
    **  Construct platform private data.
    */
    gceSTATUS
    (*allocPriv)(
        IN gckPLATFORM Platform
        );

    /*******************************************************************************
    **
    **  freePriv
    **
    **  free platform private data.
    */
    gceSTATUS
    (*freePriv)(
        IN gckPLATFORM Platform
        );

    /*******************************************************************************
    **
    **  setPower
    **
    **  Set power state of specified GPU.
    **
    **  INPUT:
    **
    **      gceCORE GPU
    **          GPU neeed to config.
    **
    **      gceBOOL Enable
    **          Enable or disable power.
    */
    gceSTATUS
    (*setPower)(
        IN gckPLATFORM Platform,
        IN gceCORE GPU,
        IN gctBOOL Enable
        );

    /*******************************************************************************
    **
    **  setClock
    **
    **  Set clock state of specified GPU.
    **
    **  INPUT:
    **
    **      gceCORE GPU
    **          GPU neeed to config.
    **
    **      gceBOOL Enable
    **          Enable or disable clock.
    */
    gceSTATUS
    (*setClock)(
        IN gckPLATFORM Platform,
        IN gceCORE GPU,
        IN gctBOOL Enable
        );

    /*******************************************************************************
    **
    **  reset
    **
    **  Reset GPU outside.
    **
    **  INPUT:
    **
    **      gceCORE GPU
    **          GPU neeed to reset.
    */
    gceSTATUS
    (*reset)(
        IN gckPLATFORM Platform,
        IN gceCORE GPU
        );

    /*******************************************************************************
    **
    **  getGPUPhysical
    **
    **  Convert CPU physical address to GPU physical address if they are
    **  different.
    */
    gceSTATUS
    (*getGPUPhysical)(
        IN gckPLATFORM Platform,
        IN gctUINT32 CPUPhysical,
        OUT gctUINT32_PTR GPUPhysical
        );

    /*******************************************************************************
    **
    **  adjustProt
    **
    **  Override Prot flag when mapping paged memory to userspace.
    */
    gceSTATUS
    (*adjustProt)(
        IN struct vm_area_struct * vma
        );

    /*******************************************************************************
    **
    **  shrinkMemory
    **
    **  Do something to collect memory, eg, act as oom killer.
    */
    gceSTATUS
    (*shrinkMemory)(
        IN gckPLATFORM Platform
        );

    /*******************************************************************************
    **
    **  cache
    **
    **  Cache operation.
    */
    gceSTATUS
    (*cache)(
        IN gckPLATFORM Platform,
        IN gctUINT32 ProcessID,
        IN gctPHYS_ADDR Handle,
        IN gctUINT32 Physical,
        IN gctPOINTER Logical,
        IN gctSIZE_T Bytes,
        IN gceCACHEOPERATION Operation
        );
}
gcsPLATFORM_OPERATIONS;

typedef struct _gcsPLATFORM
{
    struct platform_device* device;
    struct platform_driver* driver;

    gcsPLATFORM_OPERATIONS* ops;

    void*                   priv;
}
gcsPLATFORM;

void
gckPLATFORM_QueryOperations(
    IN gcsPLATFORM_OPERATIONS ** Operations
    );

#endif
