#ifndef _gc_hal_kernel_features_h_
#define _gc_hal_kernel_features_h_

#include <linux/cputype.h>

/* refer to MRVL_CONFIG_POWER_CLOCK_SEPARATED(removed) */
static inline int has_feat_separated_power_clock(void)
{
    return 1;
}

/* refer to MRVL_2D3D_CLOCK_SEPARATED(removed) */
static inline int has_feat_separated_gc_clock(void)
{
    return 1;
}

/* refer to MRVL_2D3D_POWER_SEPARATED(removed) */
static inline int has_feat_separated_gc_power(void)
{
    return 1;
}

/* refer to MRVL_2D_POWER_DYNAMIC_ONOFF(removed)
    On pxa1928 Zx, GC2D has no power switch.
*/
static inline int has_feat_2d_power_onoff(void)
{
    return !cpu_is_pxa1928_zx();
}

/* MRVL_3D_CORE_SH_CLOCK_SEPARATED */
static inline int has_feat_3d_shader_clock(void)
{
    return 1;
}

/* refer to MRVL_POLICY_CLKOFF_WHEN_IDLE(removed) */
static inline int has_feat_policy_clock_off_when_idle(void)
{
    return 1;
}

/* refer to MRVL_DFC_PROTECT_CLK_OPERATION(removed)
    To prevent clock on/off when DFC.
*/
static inline int has_feat_dfc_protect_clk_op(void)
{
    return cpu_is_pxa1928();
}

/*
    refer to MRVL_DFC_JUMP_HI_INDIRECT (removed)
    Change to intermediate frequency before to high frequency,
    and set freq to lowest when power off.
*/
static inline int has_feat_freq_change_indirect(void)
{
    return cpu_is_pxa1928();
}

#endif
