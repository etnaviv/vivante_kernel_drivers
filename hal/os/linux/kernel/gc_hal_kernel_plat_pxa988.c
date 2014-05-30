/*
 * gc_hal_kernel_plat_pxa988.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gc_hal_kernel_plat_common.h"
#include <linux/kallsyms.h>

#define __PLAT_APINAME(apiname)     pxa988_##apiname

#if defined(CONFIG_ARM64)
#define PARAM_TYPE_PWR unsigned int
#else
#define PARAM_TYPE_PWR int
#endif

typedef int (*PFUNC_GC_PWR)(PARAM_TYPE_PWR);

static PFUNC_GC_PWR GC3D_PWR = gcvNULL;
static PFUNC_GC_PWR GC2D_PWR = gcvNULL;

/**
 * gc3d shader definition
 */
static struct gc_ops gc3dsh_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = gpu_clk_setrate_dft,
    .getclkrate = gpu_clk_getrate_dft,
};

static struct gc_iface gc3dsh_iface = {
    .name               = "gc3dsh",
    .con_id             = "GC_SHADER_CLK",
    .ops                = &gc3dsh_ops,
};

/**
 * gc3d definition
 */

static void __PLAT_APINAME(gc3d_pwr_ops)(struct gc_iface *iface, PARAM_TYPE_PWR enabled)
{
    PR_DEBUG("[%6s] %s %d\n", iface->name, __func__, enabled);

    if(GC3D_PWR == gcvNULL)
    {
#ifdef CONFIG_KALLSYMS
        GC3D_PWR = (PFUNC_GC_PWR)kallsyms_lookup_name("gc_pwr");

#elif (defined CONFIG_ARM64) || (defined CONFIG_CPU_EDEN) || (defined CONFIG_CPU_PXA1928)

        extern void gc3d_pwr(unsigned int);

        GC3D_PWR = gc3d_pwr;
#elif (defined CONFIG_CPU_PXA988)

        extern void gc_pwr(PARAM_TYPE_PWR);

        GC3D_PWR = gc_pwr;
#else
        gcmkPRINT("GC3D_PWR not implemented!");
        return;
#endif
    }

    GC3D_PWR(enabled);
}

static struct gc_ops gc3d_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = gpu_clk_setrate_dft,
    .getclkrate = gpu_clk_getrate_dft,
    .pwrops     = __PLAT_APINAME(gc3d_pwr_ops),
};

static struct gc_iface *gc3d_iface_chains[] = {
    &gc3dsh_iface,
};

static struct gc_iface gc3d_iface = {
    .name               = "gc3d",
    .con_id             = "GCCLK",
    .ops                = &gc3d_ops,
    .chains_count       = 1,
    .chains_clk         = gc3d_iface_chains,
};

/**
 * gc2d definition
 */
static void __PLAT_APINAME(gc2d_pwr_ops)(struct gc_iface *iface, PARAM_TYPE_PWR enabled)
{
    PR_DEBUG("[%6s] %s %d\n", iface->name, __func__, enabled);

    if(GC2D_PWR == gcvNULL)
    {
#ifdef CONFIG_KALLSYMS
        GC2D_PWR = (PFUNC_GC_PWR)kallsyms_lookup_name("gc2d_pwr");

#elif (defined CONFIG_ARM64) || (defined CONFIG_CPU_EDEN) || (defined CONFIG_CPU_PXA1928)

        extern void gc2d_pwr(unsigned int);

        GC2D_PWR = gc2d_pwr;
#elif (defined CONFIG_CPU_PXA988)

        extern void gc2d_pwr(PARAM_TYPE_PWR);

        GC2D_PWR = gc2d_pwr;
#else
        gcmkPRINT("GC2D_PWR not implemented!");
        return;
#endif
    }

    GC2D_PWR(enabled);
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

struct gc_iface *pxa988_gc_ifaces[] = {
    [gcvCORE_MAJOR] = &gc3d_iface,
    [gcvCORE_2D]    = &gc2d_iface,
    [gcvCORE_SH]    = &gc3dsh_iface,
    gcvNULL,
};

