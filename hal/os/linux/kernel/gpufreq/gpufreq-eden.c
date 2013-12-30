/*
 * gpufreq-eden.c
 *
 * Author: Jie Zhang <jiezhang@marvell.com>
 * Copyright (C) 2012 - 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gpufreq.h"

#if MRVL_CONFIG_ENABLE_GPUFREQ
#if MRVL_PLATFORM_TTD2
#include <linux/clk.h>
#include <linux/err.h>

#define GPUFREQ_FREQ_TABLE_MAX_NUM  10

#define GPUFREQ_SET_FREQ_TABLE(_table, _index, _freq) \
{ \
    _table[_index].index     = _index; \
    _table[_index].frequency = _freq;  \
}

extern int get_gc3d_freqs_table(unsigned long *gcu3d_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

extern int get_gc3d_sh_freqs_table(unsigned long *gcu3d_sh_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

extern int get_gc2d_freqs_table(unsigned long *gcu2d_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

typedef int (*PFUNC_GET_FREQS_TBL)(unsigned long *, unsigned int *, unsigned int);

struct gpufreq_eden {
#if !MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK
    /* 2d/3d core clk*/
    struct clk *gc_clk;

    /* 3d shader clk*/
    struct clk *gc_clk_sh;
#endif

    /* predefined freqs_table */
    struct gpufreq_frequency_table freq_table[GPUFREQ_FREQ_TABLE_MAX_NUM+2];

    /* shader freqency table*/
    struct gpufreq_frequency_table freq_table_sh[GPUFREQ_FREQ_TABLE_MAX_NUM+2];

    /* function pointer to get freqs table */
    PFUNC_GET_FREQS_TBL pf_get_freqs_table;
    PFUNC_GET_FREQS_TBL pf_get_sh_freqs_table;
};

static gckOS gpu_os;
struct gpufreq_eden gpu_eden[GPUFREQ_GPU_NUMS];
static unsigned int major_freq_table_size;

#if !MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK
static unsigned int gpufreq_3d_shader_freq_get(void)
{
    unsigned int rate = ~0;
    struct clk *gc_clk = gpu_eden[gcvCORE_MAJOR].gc_clk_sh;

    if(!gc_clk)
    {
        debug_log(GPUFREQ_LOG_ERROR, "gc shader clk of gpu 0 is invalid\n");
        return -EINVAL;
    }

    rate = clk_get_rate(gc_clk);

    if(rate == (unsigned int)-1)
        return -EINVAL;

    return HZ_TO_KHZ(rate);
}
#else
static unsigned int gpufreq_3d_shader_freq_get(void)
{
    gceSTATUS status;
    unsigned int rate = ~0;

    gcmkONERROR(gckOS_QueryClkRate(gpu_os, gcvCORE_SH, &rate));
    return rate;

OnError:
    debug_log(GPUFREQ_LOG_ERROR, "failed to get clk rate of gpu %u\n", gcvCORE_SH);
    return -EINVAL;
}
#endif

#define RESTRICT_MAX_FREQ_600M        1

static int gpufreq_frequency_table_get(unsigned int gpu,
    struct gpufreq_frequency_table *core_table,
    unsigned int core_table_size,
    struct gpufreq_frequency_table *sh_table,
    unsigned int sh_table_size)
{
    int ret, i, j;
    unsigned long gc_freqs_table[GPUFREQ_FREQ_TABLE_MAX_NUM];
    unsigned int freq_table_item_count = 0;
    /*Shader frequency table*/
    unsigned long gc_sh_freqs_table[GPUFREQ_FREQ_TABLE_MAX_NUM];
    unsigned int sh_freq_table_item_count = 0;

    WARN_ON(gpu >= GPUFREQ_GPU_NUMS);
    WARN_ON(gpu_eden[gpu].pf_get_freqs_table == NULL);

    ret = (* gpu_eden[gpu].pf_get_freqs_table)(&gc_freqs_table[0],
                                           &freq_table_item_count,
                                           GPUFREQ_FREQ_TABLE_MAX_NUM);
    if(unlikely(ret != 0))
    {
        /* failed in getting table, make a default one */
        gc_freqs_table[0] = 156000000;
        gc_freqs_table[1] = 312000000;
        gc_freqs_table[2] = 416000000;
        gc_freqs_table[3] = 624000000;
        freq_table_item_count = 4;
    }

#if RESTRICT_MAX_FREQ_600M
    for(i = 0; i < freq_table_item_count; i++)
    {
        if(gc_freqs_table[i] > 600000000)
        {
            freq_table_item_count = i;
            break;
        }
    }
#endif

    if(gpu == gcvCORE_MAJOR)
    {
        ret = (* gpu_eden[gpu].pf_get_sh_freqs_table)(&gc_sh_freqs_table[0],
                                           &sh_freq_table_item_count,
                                           GPUFREQ_FREQ_TABLE_MAX_NUM);
        if(unlikely(ret != 0))
        {
            /* failed in getting table, make a default one */
            gc_sh_freqs_table[0] = 156000000;
            gc_sh_freqs_table[1] = 312000000;
            gc_sh_freqs_table[2] = 416000000;
            gc_sh_freqs_table[3] = 624000000;
            sh_freq_table_item_count = 4;
        }

#if RESTRICT_MAX_FREQ_600M
        for(i = 0; i < sh_freq_table_item_count; i++)
        {
            if(gc_sh_freqs_table[i] > 600000000)
            {
                sh_freq_table_item_count = i;
                break;
            }
        }
#endif
    }

    if(gpu == gcvCORE_MAJOR)
    {
        int outIndex = 0;
        /* Core: Idle, Shader: Idle */
        GPUFREQ_SET_FREQ_TABLE(core_table, 0, HZ_TO_KHZ(gc_freqs_table[0]));
        GPUFREQ_SET_FREQ_TABLE(sh_table, 0, HZ_TO_KHZ(gc_sh_freqs_table[0]));

        /* Core: Busy, Shader: Idle */
        i = gcmMIN(1, (freq_table_item_count - 1));
        GPUFREQ_SET_FREQ_TABLE(core_table, 1, HZ_TO_KHZ(gc_freqs_table[i]));
        GPUFREQ_SET_FREQ_TABLE(sh_table, 1, HZ_TO_KHZ(gc_sh_freqs_table[0]));

        outIndex = 2;
        for(i = 1, j = 1; (i < freq_table_item_count - 1) || (j < sh_freq_table_item_count - 1); i++, j++, outIndex++)
        {
            if(i >= (freq_table_item_count - 1))
            {
                i = (int)freq_table_item_count - 2;

                if(i < 0)
                {
                    i = 0;
                }
            }

            if(j >= sh_freq_table_item_count- 1)
            {
                j = (int)sh_freq_table_item_count - 2;

                if(j < 0)
                {
                    j = 0;
                }
            }

            GPUFREQ_SET_FREQ_TABLE(core_table, outIndex, HZ_TO_KHZ(gc_freqs_table[i]));
            GPUFREQ_SET_FREQ_TABLE(sh_table, outIndex, HZ_TO_KHZ(gc_sh_freqs_table[j]));
        }

#if 0
        /* Core: Idle, Shader: Busy */
        i = ((int)freq_table_item_count - 2 < 0)? 0 : ((int)freq_table_item_count - 2);
        j = sh_freq_table_item_count - 1;
        GPUFREQ_SET_FREQ_TABLE(core_table, outIndex,HZ_TO_KHZ(gc_freqs_table[i]));
        GPUFREQ_SET_FREQ_TABLE(sh_table, outIndex, HZ_TO_KHZ(gc_sh_freqs_table[j]));
        outIndex++;
#endif

        /* Core: Busy, Shader: Busy */
        i = freq_table_item_count - 1;
        j = sh_freq_table_item_count - 1;
        GPUFREQ_SET_FREQ_TABLE(core_table, outIndex, HZ_TO_KHZ(gc_freqs_table[i]));
        GPUFREQ_SET_FREQ_TABLE(sh_table, outIndex, HZ_TO_KHZ(gc_sh_freqs_table[j]));
        outIndex++;

        /* End of Table */
        GPUFREQ_SET_FREQ_TABLE(core_table, outIndex, GPUFREQ_TABLE_END);
        GPUFREQ_SET_FREQ_TABLE(sh_table, outIndex, GPUFREQ_TABLE_END);

        major_freq_table_size = outIndex;

        for(i = 0; i < major_freq_table_size; i++)
        {
            debug_log(GPUFREQ_LOG_DEBUG, "[%d] entry %d is (%d, %d) KHZ\n",
                gpu, i, core_table[i].frequency, sh_table[i].frequency);
        }
    }
    else if(gpu == gcvCORE_2D)
    {
        /* initialize frequency table */
        for(i = 0; i < freq_table_item_count; i++)
        {
            debug_log(GPUFREQ_LOG_DEBUG, "[%d] entry %d is %ld KHZ\n", gpu, i, HZ_TO_KHZ(gc_freqs_table[i]));
            GPUFREQ_SET_FREQ_TABLE(core_table, i, HZ_TO_KHZ(gc_freqs_table[i]));
        }
        GPUFREQ_SET_FREQ_TABLE(core_table, i, GPUFREQ_TABLE_END);

        /* Init shader frequency table to zero, because 2D core has no shader. */
        gckOS_ZeroMemory(sh_table, sh_table_size * sizeof(struct gpufreq_frequency_table));
        GPUFREQ_SET_FREQ_TABLE(sh_table, 0, GPUFREQ_TABLE_END);
    }

    return 0;
}

/* [RO] attr: scaling_available_freqs */
static ssize_t show_scaling_available_freqs(struct gpufreq_policy *policy, char *buf)
{
    unsigned int i;
    unsigned int gpu = policy->gpu;
    struct gpufreq_frequency_table *freq_table = gcvNULL;
    ssize_t count = 0;
    unsigned int prevFreq = 0;

    freq_table = gpu_eden[gpu].freq_table;

    for (i = 0; (freq_table[i].frequency != GPUFREQ_TABLE_END); ++i)
    {
        if (freq_table[i].frequency == GPUFREQ_ENTRY_INVALID)
            continue;

        if (prevFreq >= freq_table[i].frequency)
            continue;

        prevFreq = freq_table[i].frequency;

        count += sprintf(&buf[count], "%d ", freq_table[i].frequency);
    }

    count += sprintf(&buf[count], "\n");

    return count;
}

gpufreq_freq_attr_ro(scaling_available_freqs);

static struct gpufreq_freq_attr *driver_attrs[] = {
    &scaling_available_freqs,
    NULL
};

static int eden_gpufreq_init (struct gpufreq_policy *policy);
static int eden_gpufreq_verify (struct gpufreq_policy *policy);
static int eden_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation);
static int eden_gpufreq_set(unsigned int gpu, struct gpufreq_freqs *freq, struct gpufreq_freqs *freq_sh);
static unsigned int eden_gpufreq_get (unsigned int chip);

static struct gpufreq_driver eden_gpufreq_driver = {
    .init   = eden_gpufreq_init,
    .verify = eden_gpufreq_verify,
    .get    = eden_gpufreq_get,
    .target = eden_gpufreq_target,
    .name   = "eden-gpufreq",
    .attr   = driver_attrs,
};

static int eden_gpufreq_init (struct gpufreq_policy *policy)
{
    unsigned gpu = policy->gpu;
#if !MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK
    struct clk *gc_clk = gpu_eden[gpu].gc_clk;
    struct clk *gc_clk_sh = gcvNULL;
#endif

    /* get func pointer from kernel functions. */
    switch (gpu) {
    case gcvCORE_MAJOR:
        gpu_eden[gpu].pf_get_freqs_table = get_gc3d_freqs_table;
        gpu_eden[gpu].pf_get_sh_freqs_table = get_gc3d_sh_freqs_table;
        break;

    case gcvCORE_2D:
        gpu_eden[gpu].pf_get_freqs_table = get_gc2d_freqs_table;
        break;

    default:
        debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %d\n", gpu);
    }

    debug_log(GPUFREQ_LOG_DEBUG, "gpu %d, pfunc %p\n", gpu, gpu_eden[gpu].pf_get_freqs_table);

    /* build freqs table */
    gpufreq_frequency_table_get(gpu,
                                gpu_eden[gpu].freq_table,
                                sizeof(gpu_eden[gpu].freq_table) / sizeof(struct gpufreq_frequency_table),
                                gpu_eden[gpu].freq_table_sh,
                                sizeof(gpu_eden[gpu].freq_table_sh) / sizeof(struct gpufreq_frequency_table));

    gpufreq_frequency_table_gpuinfo(policy, gpu_eden[gpu].freq_table);

#if !MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK
    if(!gc_clk)
    {
        switch (gpu) {
        case gcvCORE_MAJOR:
            gc_clk = clk_get(NULL, "GC3D_CLK1X");
            gc_clk_sh = clk_get(NULL, "GC3D_CLK2X");
            break;

        case gcvCORE_2D:
            gc_clk = clk_get(NULL, "GC2D_CLK");
            break;

        default:
            debug_log(GPUFREQ_LOG_ERROR, "cannot get clk for gpu %d\n", gpu);
        }

        if(IS_ERR(gc_clk))
        {
            debug_log(GPUFREQ_LOG_ERROR, "get clk of gpu %d, ret %ld\n", gpu, PTR_ERR(gc_clk));
            return PTR_ERR(gc_clk);
        }

        if((gpu == gcvCORE_MAJOR) && IS_ERR(gc_clk_sh))
        {
            debug_log(GPUFREQ_LOG_ERROR, "get shader clk of gpu %d, ret %ld\n", gpu, PTR_ERR(gc_clk_sh));
            return PTR_ERR(gc_clk_sh);
        }

        /* store in global variable. */
        gpu_eden[gpu].gc_clk    = gc_clk;
        gpu_eden[gpu].gc_clk_sh = gc_clk_sh;
    }
#endif

    policy->cur = eden_gpufreq_get(policy->gpu);

    debug_log(GPUFREQ_LOG_INFO, "GPUFreq for Eden gpu %d initialized, cur_freq %u\n", gpu, policy->cur);

    return 0;
}

static int eden_gpufreq_verify (struct gpufreq_policy *policy)
{
    gpufreq_verify_within_limits(policy, policy->gpuinfo.min_freq,
                     policy->gpuinfo.max_freq);
    return 0;
}

static int eden_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation)
{
    int index;
    int ret = 0;
    struct gpufreq_freqs freq;
    struct gpufreq_freqs freq_sh = {0};
    unsigned int gpu = policy->gpu;
    struct gpufreq_frequency_table *freq_table = gpu_eden[gpu].freq_table;
    static int old_major_index = -1;
    static struct gpufreq_policy * old_major_policy = gcvNULL;

    /* find a nearest freq in freq_table for target_freq */
    ret = gpufreq_frequency_table_target(policy, freq_table, target_freq, relation, &index);
    if(ret)
    {
        debug_log(GPUFREQ_LOG_ERROR, "[%d] error: invalid target frequency: %u\n", gpu, target_freq);
        return ret;
    }

    freq.gpu      = policy->gpu;

#if MRVL_DFC_JUMP_HI_INDIRECT
    /* Freq may be changed by galcore daemon, so policy->cur may not represent the actual freq. */
    freq.old_freq = policy->cur = eden_gpufreq_get(gpu);
#else
    freq.old_freq = policy->cur;
#endif

    freq.new_freq = freq_table[index].frequency;

    if(gpu == gcvCORE_MAJOR)
    {
        if(old_major_policy != policy)
        {
            old_major_index = index;
            old_major_policy = policy;
        }
        freq_sh.gpu = gcvCORE_MAJOR;
        freq_sh.old_freq = gpufreq_3d_shader_freq_get();
        freq_sh.new_freq = gpu_eden[gcvCORE_MAJOR].freq_table_sh[index].frequency;

        /* FIXME: Workaround current gpufreq_frequency_table_target() function can only
           handle monotone increasing data.*/
        if(freq.old_freq == freq.new_freq)
        {
            if(target_freq > freq.old_freq)
            {
                if((old_major_index + 1) < major_freq_table_size)
                {
                    index = old_major_index + 1;
                }
            }
            else if(target_freq < freq.old_freq)
            {
                if((old_major_index - 1) >= 0)
                {
                    index = old_major_index - 1;
                }
            }

            freq.new_freq = freq_table[index].frequency;
            freq_sh.new_freq = gpu_eden[gcvCORE_MAJOR].freq_table_sh[index].frequency;
        }

        old_major_index = index;
    }

    if(((gpu == gcvCORE_2D) && (freq.old_freq == freq.new_freq))
        || ((gpu == gcvCORE_MAJOR) && (freq.old_freq == freq.new_freq) && (freq_sh.old_freq == freq_sh.new_freq)))
        return ret;

    gpufreq_notify_transition(&freq, GPUFREQ_PRECHANGE);

    eden_gpufreq_set(gpu, &freq, &freq_sh);

    gpufreq_notify_transition(&freq, GPUFREQ_POSTCHANGE);

    return ret;
}

#if MRVL_ENABLE_COMMON_PWRCLK_FRAMEWORK
static int eden_gpufreq_set(unsigned int gpu, struct gpufreq_freqs *freq, struct gpufreq_freqs *freq_sh)
{
    int ret = 0;
    unsigned int rate = 0;
    gceSTATUS status;

#if MRVL_DFC_PROTECT_CLK_OPERATION
    gpufreq_acquire_clock_mutex(gpu);
#endif

    if(gpu == gcvCORE_MAJOR)
    {
        debug_log(GPUFREQ_LOG_DEBUG, "[3D]gpufreq change: (%d, %d) --> (%d, %d)\n",
                        freq->old_freq, freq_sh->old_freq, freq->new_freq, freq_sh->new_freq);
    }
    else if(gpu == gcvCORE_2D)
    {
        debug_log(GPUFREQ_LOG_DEBUG, "[2D]gpufreq change: %d --> %d\n",
                        freq->old_freq, freq->new_freq);
    }

    status = gckOS_SetClkRate(gpu_os, gpu, freq->new_freq);
    if(gcmIS_ERROR(status))
    {
        debug_log(GPUFREQ_LOG_WARNING, "[%d] failed to set target rate %u KHZ\n",
                        gpu, freq->new_freq);
    }

    if(gpu == gcvCORE_MAJOR)
    {
        status = gckOS_SetClkRate(gpu_os, gcvCORE_SH, freq_sh->new_freq);
        if(gcmIS_ERROR(status))
        {
            debug_log(GPUFREQ_LOG_WARNING, "[%d] failed to set target rate %u KHZ\n",
                            gpu, freq_sh->new_freq);
        }
    }

#if MRVL_DFC_PROTECT_CLK_OPERATION
    gpufreq_release_clock_mutex(gpu);
#endif

    /* update current frequency after adjustment */
    rate = eden_gpufreq_get(gpu);
    if(rate == -EINVAL)
    {
        debug_log(GPUFREQ_LOG_WARNING, "failed get rate for gpu %d\n", gpu);
        freq->new_freq = freq->old_freq;
        ret = -EINVAL;
    }
    else
    {
        freq->new_freq = rate;
    }

    return ret;
}

static unsigned int eden_gpufreq_get (unsigned int gpu)
{
    gceSTATUS status;
    unsigned int rate = ~0;

    gcmkONERROR(gckOS_QueryClkRate(gpu_os, gpu, &rate));
    return rate;

OnError:
    debug_log(GPUFREQ_LOG_ERROR, "failed to get clk rate of gpu %u\n", gpu);
    return -EINVAL;
}
#else
static int eden_gpufreq_set(unsigned int gpu, struct gpufreq_freqs *freq, struct gpufreq_freqs *freq_sh)
{
    int ret = 0;
    unsigned int rate = 0;
    struct clk *gc_clk = gpu_eden[gpu].gc_clk;
    struct clk *gc_clk_sh = gpu_eden[gpu].gc_clk_sh;

#if MRVL_DFC_PROTECT_CLK_OPERATION
    gpufreq_acquire_clock_mutex(gpu);
#endif

    if(gpu == gcvCORE_MAJOR)
    {
        debug_log(GPUFREQ_LOG_DEBUG, "[3D]gpufreq change: (%d, %d) --> (%d, %d)\n",
                        freq->old_freq, freq_sh->old_freq, freq->new_freq, freq_sh->new_freq);
    }
    else if(gpu == gcvCORE_2D)
    {
        debug_log(GPUFREQ_LOG_DEBUG, "[2D]gpufreq change: %d --> %d\n",
                        freq->old_freq, freq->new_freq);
    }

#if MRVL_DFC_JUMP_HI_INDIRECT
    if((freq->new_freq > 312000)
    && (freq->old_freq < 312000)
    )
    {
        clk_set_rate(gc_clk, KHZ_TO_HZ(312000));
    }

    if((gpu == gcvCORE_MAJOR)
    && (freq_sh->new_freq > 312000)
    && (freq_sh->old_freq < 312000)
    )
    {
        clk_set_rate(gc_clk_sh, KHZ_TO_HZ(312000));
    }
#endif

    ret = clk_set_rate(gc_clk, KHZ_TO_HZ(freq->new_freq));
    if(ret)
    {
        debug_log(GPUFREQ_LOG_WARNING, "[%d] failed to set target rate %u to clk %p\n",
                        gpu, freq->new_freq, gc_clk);
    }

    if(gpu == gcvCORE_MAJOR)
    {
        ret = clk_set_rate(gc_clk_sh, KHZ_TO_HZ(freq_sh->new_freq));
        if(ret)
        {
            debug_log(GPUFREQ_LOG_WARNING, "[%d] failed to set target rate %u to clk %p\n",
                            gpu, freq_sh->new_freq, gc_clk_sh);
        }
    }

#if MRVL_DFC_PROTECT_CLK_OPERATION
    gpufreq_release_clock_mutex(gpu);
#endif

    /* update current frequency after adjustment */
    rate = eden_gpufreq_get(gpu);
    if(rate == -EINVAL)
    {
        debug_log(GPUFREQ_LOG_WARNING, "failed get rate for gpu %d\n", gpu);
        freq->new_freq = freq->old_freq;
        ret = -EINVAL;
    }
    else
    {
        freq->new_freq = rate;
    }

    return ret;
}

static unsigned int eden_gpufreq_get (unsigned int gpu)
{
    unsigned int rate = ~0;
    struct clk *gc_clk = gpu_eden[gpu].gc_clk;

    if(!gc_clk)
    {
        debug_log(GPUFREQ_LOG_ERROR, "gc clk of gpu %d is invalid\n", gpu);
        return -EINVAL;
    }

    rate = clk_get_rate(gc_clk);

    if(rate == (unsigned int)-1)
        return -EINVAL;

    return HZ_TO_KHZ(rate);
}
#endif

/***************************************************
**  interfaces exported to GC driver
****************************************************/
int __GPUFREQ_EXPORT_TO_GC gpufreq_init(gckOS Os)
{
    if(!gpu_os)
        gpu_os = Os;

    WARN_ON(!gpu_os);

    gpufreq_early_init();
    gpufreq_register_driver(Os, &eden_gpufreq_driver);
    return 0;
}

void __GPUFREQ_EXPORT_TO_GC gpufreq_exit(gckOS Os)
{
    gpufreq_unregister_driver(Os, &eden_gpufreq_driver);
    gpufreq_late_exit();
}

#endif /* End of MRVL_PLATFORM_TTD2 */
#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */

