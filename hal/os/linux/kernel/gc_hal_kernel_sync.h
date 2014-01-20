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


#ifndef __gc_hal_kernel_sync_h_
#define __gc_hal_kernel_sync_h_

#include <linux/types.h>

#include <gc_hal.h>
#include <gc_hal_base.h>

#if gcdANDROID_NATIVE_FENCE_SYNC
#include <linux/sync.h>

struct viv_sync_timeline
{
    /* Parent object. */
    struct sync_timeline obj;

    /* Timestamp when sync_pt is created. */
    gctUINT stamp;

    /* Pointer to os struct. */
    gckOS os;
};


struct viv_sync_pt
{
    /* Parent object. */
    struct sync_pt pt;

    /* Reference sync point*/
    gctSYNC_POINT sync;

    /* Timestamp when sync_pt is created. */
    gctUINT stamp;
};

/* Create viv_sync_timeline object. */
struct viv_sync_timeline *
viv_sync_timeline_create(
    const char * Name,
    gckOS Os
    );

/* Create viv_sync_pt object. */
struct sync_pt *
viv_sync_pt_create(
    struct viv_sync_timeline * Obj,
    gctSYNC_POINT SyncPoint
    );

#endif

#endif /* __gc_hal_kernel_sync_h_ */
