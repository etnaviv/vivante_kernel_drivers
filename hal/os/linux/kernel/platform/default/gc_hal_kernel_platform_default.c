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


#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"

gctBOOL
_NeedAddDevice(
    IN gckPLATFORM Platform
    )
{
#if MRVL_USE_GPU_RESERVE_MEM
    return gcvFALSE;
#else
    return gcvTRUE;
#endif
}

gcsPLATFORM_OPERATIONS platformOperations =
{
    .needAddDevice = _NeedAddDevice,
};

void
gckPLATFORM_QueryOperations(
    IN gcsPLATFORM_OPERATIONS ** Operations
    )
{
     *Operations = &platformOperations;
}
