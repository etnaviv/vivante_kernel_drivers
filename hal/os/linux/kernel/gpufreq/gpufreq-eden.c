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
#include <linux/kallsyms.h>

#if MRVL_CONFIG_ENABLE_GPUFREQ
#include <linux/clk.h>
#include <linux/err.h>
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
#include <linux/pm_qos.h>
#endif

#define GPUFREQ_FREQ_TABLE_MAX_NUM  10

#define GPUFREQ_SET_FREQ_TABLE(_table, _index, _freq) \
{ \
    _table[_index].index     = _index; \
    _table[_index].frequency = _freq;  \
}

typedef int (*PFUNC_GET_FREQS_TBL)(unsigned long *, unsigned int *, unsigned int);

struct gpufreq_eden {
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

#if defined(CONFIG_ARM64)
/* No need to restrict max freq to 600MHz for pxa1928 A0. */
#define RESTRICT_MAX_FREQ_600M        0
#else
#define RESTRICT_MAX_FREQ_600M        1
#endif

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

/* [RO] attr: scaling_available_frequencies */
static ssize_t show_scaling_available_frequencies(struct gpufreq_policy *policy, char *buf)
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

gpufreq_freq_attr_ro(scaling_available_frequencies);

static struct gpufreq_freq_attr *driver_attrs[] = {
    &scaling_available_frequencies,
    NULL
};

static int eden_gpufreq_init (struct gpufreq_policy *policy);
static int eden_gpufreq_exit (struct gpufreq_policy *policy);
static int eden_gpufreq_verify (struct gpufreq_policy *policy);
static int eden_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation);
static int eden_gpufreq_set(unsigned int gpu, struct gpufreq_freqs *freq, struct gpufreq_freqs *freq_sh);
static unsigned int eden_gpufreq_get (unsigned int chip);

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
static unsigned int is_qos_inited = 0;

DECLARE_META_REQUEST(3d, min);
IMPLEMENT_META_NOTIFIER(0, 3d, min, GPUFREQ_RELATION_L);
DECLARE_META_REQUEST(3d, max);
IMPLEMENT_META_NOTIFIER(0, 3d, max, GPUFREQ_RELATION_H);

DECLARE_META_REQUEST(2d, min);
IMPLEMENT_META_NOTIFIER(1, 2d, min, GPUFREQ_RELATION_L);
DECLARE_META_REQUEST(2d, max);
IMPLEMENT_META_NOTIFIER(1, 2d, max, GPUFREQ_RELATION_H);

static struct _gc_qos gc_qos[] = {
    DECLARE_META_GC_QOS_3D,
    DECLARE_META_GC_QOS_2D,
};

void eden_gpufreq_register_qos(unsigned int gpu)
{
    pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
    pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
}

void eden_gpufreq_unregister_qos(unsigned int gpu)
{
    pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
    pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
}

#endif /* MRVL_CONFIG_ENABLE_QOS_SUPPORT */

struct gpufreq_driver eden_gpufreq_driver = {
    .init   = eden_gpufreq_init,
    .verify = eden_gpufreq_verify,
    .get    = eden_gpufreq_get,
    .target = eden_gpufreq_target,
    .name   = "eden-gpufreq",
    .exit   = eden_gpufreq_exit,
    .attr   = driver_attrs,
};

static void eden_init_freqs_table_func(unsigned int gpu)
{
#ifdef CONFIG_KALLSYMS
    switch (gpu) {
    case gcvCORE_MAJOR:
        gpu_eden[gpu].pf_get_freqs_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gc3d_freqs_table");
        gpu_eden[gpu].pf_get_sh_freqs_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gc3d_sh_freqs_table");
        break;

    case gcvCORE_2D:
        gpu_eden[gpu].pf_get_freqs_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gc2d_freqs_table");
        break;

    default:
        debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %d\n", gpu);
    }

#elif (defined CONFIG_ARM64) || (defined CONFIG_CPU_EDEN) || (defined CONFIG_CPU_PXA1928)
extern int get_gc3d_freqs_table(unsigned long *gcu3d_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

extern int get_gc3d_sh_freqs_table(unsigned long *gcu3d_sh_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

extern int get_gc2d_freqs_table(unsigned long *gcu2d_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);
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
#else
    debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %d\n", gpu);
#endif
}

static int eden_gpufreq_init (struct gpufreq_policy *policy)
{
    unsigned gpu = policy->gpu;

    /* get func pointer from kernel functions. */
    eden_init_freqs_table_func(gpu);

    /* build freqs table */
    gpufreq_frequency_table_get(gpu,
                                gpu_eden[gpu].freq_table,
                                sizeof(gpu_eden[gpu].freq_table) / sizeof(struct gpufreq_frequency_table),
                                gpu_eden[gpu].freq_table_sh,
                                sizeof(gpu_eden[gpu].freq_table_sh) / sizeof(struct gpufreq_frequency_table));

    gpufreq_frequency_table_gpuinfo(policy, gpu_eden[gpu].freq_table);

    policy->cur = eden_gpufreq_get(policy->gpu);

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(unlikely(!(is_qos_inited & (1 << gpu))))
    {
        eden_gpufreq_register_qos(gpu);
        pm_qos_add_request(gc_qos[gpu].pm_qos_req_min,
                           gc_qos[gpu].pm_qos_class_min,
                           policy->gpuinfo.min_freq);
        pm_qos_add_request(gc_qos[gpu].pm_qos_req_max,
                           gc_qos[gpu].pm_qos_class_max,
                           policy->gpuinfo.max_freq);
        is_qos_inited |= (1 << gpu);
    }
#endif

    debug_log(GPUFREQ_LOG_INFO, "GPUFreq for Eden gpu %d initialized, cur_freq %u\n", gpu, policy->cur);

    return 0;
}

static int eden_gpufreq_exit (struct gpufreq_policy *policy)
{
    unsigned gpu = policy->gpu;
    eden_gpufreq_unregister_qos(gpu);

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

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    target_freq = max((unsigned int)pm_qos_request(gc_qos[gpu].pm_qos_class_min),
                     target_freq);
    target_freq = min((unsigned int)pm_qos_request(gc_qos[gpu].pm_qos_class_max),
                     target_freq);
#endif

    /* find a nearest freq in freq_table for target_freq */
    ret = gpufreq_frequency_table_target(policy, freq_table, target_freq, relation, &index);
    if(ret)
    {
        debug_log(GPUFREQ_LOG_ERROR, "[%d] error: invalid target frequency: %u\n", gpu, target_freq);
        return ret;
    }

    freq.gpu      = policy->gpu;

    if(has_feat_freq_change_indirect())
    {
        /* Freq may be changed by galcore daemon, so policy->cur may not represent the actual freq. */
        freq.old_freq = policy->cur = eden_gpufreq_get(gpu);
    }
    else
    {
        freq.old_freq = policy->cur;
    }

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

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    debug_log(GPUFREQ_LOG_DEBUG, "[%d] Qos_min: %d, Qos_max: %d, Target: %d (KHZ)\n",
            gpu,
            pm_qos_request(gc_qos[gpu].pm_qos_class_min),
            pm_qos_request(gc_qos[gpu].pm_qos_class_max),
            freq.new_freq);
#endif

    if(((gpu == gcvCORE_2D) && (freq.old_freq == freq.new_freq))
        || ((gpu == gcvCORE_MAJOR) && (freq.old_freq == freq.new_freq) && (freq_sh.old_freq == freq_sh.new_freq)))
        return ret;

    gpufreq_notify_transition(&freq, GPUFREQ_PRECHANGE);

    eden_gpufreq_set(gpu, &freq, &freq_sh);

    gpufreq_notify_transition(&freq, GPUFREQ_POSTCHANGE);

    return ret;
}

static int eden_gpufreq_set(unsigned int gpu, struct gpufreq_freqs *freq, struct gpufreq_freqs *freq_sh)
{
    int ret = 0;
    unsigned int rate = 0;
    gceSTATUS status;

    if (has_feat_dfc_protect_clk_op())
        gpufreq_acquire_clock_mutex(gpu);

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

    if (has_feat_dfc_protect_clk_op())
        gpufreq_release_clock_mutex(gpu);

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

#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */

