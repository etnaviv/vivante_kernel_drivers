/*
 * gc_hal_kernel_plat_helan.c
 *
 * Author: Watson Wang <zswang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gc_hal_kernel_plat_common.h"

#if MRVL_PLATFORM_PXA1088
#define __PLAT_APINAME(apiname)     helan_##apiname

extern void gc_pwr(PARAM_TYPE_PWR);
#define GC_PWR_SHARED    gc_pwr

/* used to protect gc3d/gc2d power sequence */
static DEFINE_SPINLOCK(_gc_pwr_lock_shared);

/* used to keep track of gc3d/gc2d power on/off */
static int _gc_pwr_refcnt_shared;

static inline void __gpu_pwr_shared(struct gc_iface *iface, PARAM_TYPE_PWR enabled)
{
    spin_lock(&_gc_pwr_lock_shared);

    if(enabled)
    {
        if(0 == _gc_pwr_refcnt_shared)
            GC_PWR_SHARED(1);
        _gc_pwr_refcnt_shared++;
    }
    else
    {
        if(WARN_ON(0 == _gc_pwr_refcnt_shared))
            goto out;

        if(1 == _gc_pwr_refcnt_shared)
        {
            GC_PWR_SHARED(0);
        }
        _gc_pwr_refcnt_shared--;
    }

out:
    spin_unlock(&_gc_pwr_lock_shared);
}

/**
 * gc3d definition
 */
static void __PLAT_APINAME(gc3d_pwr_ops)(struct gc_iface *iface, PARAM_TYPE_PWR enabled)
{
    PR_DEBUG("[%6s] %s %d\n", iface->name, __func__, enabled);
    __gpu_pwr_shared(iface, enabled);
}

static struct gc_ops gc3d_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = gpu_clk_setrate_dft,
    .getclkrate = gpu_clk_getrate_dft,
    .pwrops     = __PLAT_APINAME(gc3d_pwr_ops),
};

static struct gc_iface gc3d_iface = {
    .name               = "gc3d",
    .con_id             = "GCCLK",
    .ops                = &gc3d_ops,
};

/**
 * gc2d definition
 */
static void __PLAT_APINAME(gc2d_pwr_ops)(struct gc_iface *iface, PARAM_TYPE_PWR enabled)
{
    PR_DEBUG("[%6s] %s %d\n", iface->name, __func__, enabled);
    __gpu_pwr_shared(iface, enabled);
}

static struct gc_ops gc2d_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = gpu_clk_setrate_dft,
    .getclkrate = gpu_clk_getrate_dft,
    .pwrops     = __PLAT_APINAME(gc2d_pwr_ops),
};

static struct gc_iface gc2d_iface = {
    .name               = "gc2d",
    .con_id             = "GC2DCLK",
    .ops                = &gc2d_ops,
};

struct gc_iface *gc_ifaces[] = {
    [gcvCORE_MAJOR] = &gc3d_iface,
    [gcvCORE_2D]    = &gc2d_iface,
    gcvNULL,
};

#endif
