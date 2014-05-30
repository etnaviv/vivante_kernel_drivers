/*
 * gpufreq-pxa988.c
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

struct gpufreq_pxa988 {
    /* predefined freqs_table */
    struct gpufreq_frequency_table freq_table[GPUFREQ_FREQ_TABLE_MAX_NUM+1];

    /* function pointer to get freqs table */
    PFUNC_GET_FREQS_TBL pf_get_freqs_table;
};

struct gpufreq_pxa988 gh[GPUFREQ_GPU_NUMS];
static gckOS gpu_os;

static int gpufreq_frequency_table_get(unsigned int gpu, struct gpufreq_frequency_table *table_freqs)
{
    int ret, i;
    unsigned long gcu_freqs_table[GPUFREQ_FREQ_TABLE_MAX_NUM];
    unsigned int freq_table_item_count = 0;

    WARN_ON(gpu >= GPUFREQ_GPU_NUMS);
    WARN_ON(gh[gpu].pf_get_freqs_table == NULL);

    ret = (* gh[gpu].pf_get_freqs_table)(&gcu_freqs_table[0],
                                           &freq_table_item_count,
                                           GPUFREQ_FREQ_TABLE_MAX_NUM);
    if(unlikely(ret != 0))
    {
        /* failed in getting table, make a default one */
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 0, HZ_TO_KHZ(156000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 1, HZ_TO_KHZ(312000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 2, HZ_TO_KHZ(416000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 3, HZ_TO_KHZ(624000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 4, GPUFREQ_TABLE_END);
        goto out;
    }

    /* initialize frequency table */
    for(i = 0; i < freq_table_item_count; i++)
    {
        debug_log(GPUFREQ_LOG_DEBUG, "[%d] entry %d is %lu KHZ\n", gpu, i, HZ_TO_KHZ(gcu_freqs_table[i]));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, i, HZ_TO_KHZ(gcu_freqs_table[i]));
    }
    GPUFREQ_SET_FREQ_TABLE(table_freqs, i, GPUFREQ_TABLE_END);

out:
    debug_log(GPUFREQ_LOG_DEBUG, "[%d] tbl[0] = %lu, %u\n", gpu, gcu_freqs_table[0], freq_table_item_count);
    return 0;
}

/* [RO] attr: scaling_available_frequencies */
static ssize_t show_scaling_available_frequencies(struct gpufreq_policy *policy, char *buf)
{
    unsigned int i;
    unsigned int gpu = policy->gpu;
    struct gpufreq_frequency_table *freq_table = gh[gpu].freq_table;
    ssize_t count = 0;

    for (i = 0; (freq_table[i].frequency != GPUFREQ_TABLE_END); ++i)
    {
        if (freq_table[i].frequency == GPUFREQ_ENTRY_INVALID)
            continue;

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

static int pxa988_gpufreq_init (struct gpufreq_policy *policy);
static int pxa988_gpufreq_exit (struct gpufreq_policy *policy);
static int pxa988_gpufreq_verify (struct gpufreq_policy *policy);
static int pxa988_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation);
static int pxa988_gpufreq_set(unsigned int gpu, struct gpufreq_freqs* freqs);
static unsigned int pxa988_gpufreq_get (unsigned int chip);

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

DECLARE_META_REQUEST(sh, min);
IMPLEMENT_META_NOTIFIER(2, sh, min, GPUFREQ_RELATION_L);
DECLARE_META_REQUEST(sh, max);
IMPLEMENT_META_NOTIFIER(2, sh, max, GPUFREQ_RELATION_H);

static struct _gc_qos gc_qos[] = {
    DECLARE_META_GC_QOS_3D,
    DECLARE_META_GC_QOS_2D,
    DECLARE_META_GC_QOS_SH,
};

void pxa988_gpufreq_register_qos(unsigned int gpu)
{
    pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
    pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
}

void pxa988_gpufreq_unregister_qos(unsigned int gpu)
{
    pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
    pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
}
#endif /* MRVL_CONFIG_ENABLE_QOS_SUPPORT */

struct gpufreq_driver pxa988_gpufreq_driver = {
    .init   = pxa988_gpufreq_init,
    .verify = pxa988_gpufreq_verify,
    .get    = pxa988_gpufreq_get,
    .target = pxa988_gpufreq_target,
    .name   = "pxa988-gpufreq",
    .exit   = pxa988_gpufreq_exit,
    .attr   = driver_attrs,
};

static void pxa988_init_freqs_table_func(unsigned int gpu)
{
#ifdef CONFIG_KALLSYMS
    switch (gpu) {
    case 0:
        gh[gpu].pf_get_freqs_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gcu_freqs_table");
        break;
    case 1:
        gh[gpu].pf_get_freqs_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gcu2d_freqs_table");
        break;
    default:
        debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %u\n", gpu);
    }

#elif (defined CONFIG_CPU_PXA988)
extern int get_gcu_freqs_table(unsigned long *gcu_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

extern int get_gcu2d_freqs_table(unsigned long *gcu2d_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);
    switch (gpu) {
    case 0:
        gh[gpu].pf_get_freqs_table = get_gcu_freqs_table;
        break;
    case 1:
        gh[gpu].pf_get_freqs_table = get_gcu2d_freqs_table;
        break;
    default:
        debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %u\n", gpu);
    }
#else
    debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %u\n", gpu);
#endif
}

static int pxa988_gpufreq_init (struct gpufreq_policy *policy)
{
    unsigned int gpu = policy->gpu;

    /* get func pointer from kernel functions. */
    pxa988_init_freqs_table_func(gpu);

    /* build freqs table */
    gpufreq_frequency_table_get(gpu, gh[gpu].freq_table);
    gpufreq_frequency_table_gpuinfo(policy, gh[gpu].freq_table);

    policy->cur = pxa988_gpufreq_get(policy->gpu);

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(unlikely(!(is_qos_inited & (1 << gpu))))
    {
        pxa988_gpufreq_register_qos(gpu);
        pm_qos_add_request(gc_qos[gpu].pm_qos_req_min,
                           gc_qos[gpu].pm_qos_class_min,
                           policy->gpuinfo.min_freq);
        pm_qos_add_request(gc_qos[gpu].pm_qos_req_max,
                           gc_qos[gpu].pm_qos_class_max,
                           policy->gpuinfo.max_freq);
        is_qos_inited |= (1 << gpu);
    }
#endif

    debug_log(GPUFREQ_LOG_INFO, "GPUFreq for HelanLTE gpu %u initialized, cur_freq %u\n", gpu, policy->cur);

    return 0;
}

static int pxa988_gpufreq_exit (struct gpufreq_policy *policy)
{
    unsigned gpu = policy->gpu;
    pxa988_gpufreq_unregister_qos(gpu);

    return 0;
}

static int pxa988_gpufreq_verify (struct gpufreq_policy *policy)
{
    gpufreq_verify_within_limits(policy, policy->gpuinfo.min_freq,
                     policy->gpuinfo.max_freq);
    return 0;
}

static int pxa988_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation)
{
    int index;
    int ret = 0;
    struct gpufreq_freqs freqs;
    unsigned int gpu = policy->gpu;
    struct gpufreq_frequency_table *freq_table = gh[gpu].freq_table;

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

    freqs.gpu      = policy->gpu;
    freqs.old_freq = policy->cur;
    freqs.new_freq = freq_table[index].frequency;

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    pr_debug("[%d] Qos_min: %d, Qos_max: %d, Target: %d (KHZ)\n",
            gpu,
            pm_qos_request(gc_qos[gpu].pm_qos_class_min),
            pm_qos_request(gc_qos[gpu].pm_qos_class_max),
            freqs.new_freq);
#endif

    if(freqs.old_freq == freqs.new_freq)
        return ret;

    gpufreq_notify_transition(&freqs, GPUFREQ_PRECHANGE);

    ret = pxa988_gpufreq_set(gpu, &freqs);

    gpufreq_notify_transition(&freqs, GPUFREQ_POSTCHANGE);

    return ret;
}

static int pxa988_gpufreq_set(unsigned int gpu, struct gpufreq_freqs* freqs)
{
    int ret = 0;
    unsigned int rate = 0;
    gceSTATUS status;

    /* update new frequency to hw */
    status = gckOS_SetClkRate(gpu_os, gpu, freqs->new_freq);
    if(gcmIS_ERROR(status))
    {
        debug_log(GPUFREQ_LOG_WARNING, "[%d] failed to set target rate %u KHZ\n",
                        gpu, freqs->new_freq);
    }

    /* update current frequency after adjustment */
    rate = pxa988_gpufreq_get(gpu);
    if(rate == -EINVAL)
    {
        debug_log(GPUFREQ_LOG_WARNING, "failed get rate for gpu %u\n", gpu);
        freqs->new_freq = freqs->old_freq;
        ret = -EINVAL;
    }
    else
    {
        freqs->new_freq = rate;
    }

    return ret;
}

static unsigned int pxa988_gpufreq_get (unsigned int gpu)
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


