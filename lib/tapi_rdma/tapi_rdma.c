/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2024 Advanced Micro Devices, Inc. */
/** @file
 * @brief Generic Test API to interact with RDMA links
 *
 * Generic API to interact with RDMA links.
 */
#define TE_LGR_USER     "TAPI RDMA"

#include "tapi_rdma.h"
#include "log_bufs.h"
#include "tapi_job.h"
#include "tapi_job_factory_rpc.h"
#include "te_alloc.h"
#include "te_string.h"
#include "te_vector.h"

#define RDMA_TOOL "rdma"

/* See description in tapi_rdma.h */
te_errno
tapi_rdma_link_get_stats(rcf_rpc_server *rpcs, const char *link,
                         tapi_rdma_link_stats_t **stats)
{
    te_errno            rc;
    tapi_job_factory_t *factory = NULL;
    te_vec              stat_vec = TE_VEC_INIT(tapi_rdma_link_stat_t);
    tapi_job_t         *job = NULL;
    tapi_job_channel_t *chan_out;
    tapi_job_channel_t *chan_err;
    tapi_job_channel_t *filter_out;
    tapi_job_buffer_t   buf_out = TAPI_JOB_BUFFER_INIT;
    tapi_job_status_t   status;

    int   ret;
    char *output;
    char *link_name = NULL;
    int   consumed;

    const char *argv[] = {
        RDMA_TOOL,
        "statistic",
        "show",
        "link",
        link,
        NULL,
    };

    tapi_job_simple_desc_t job_desc = {
        .program = RDMA_TOOL,
        .argv = argv,
        .stdout_loc = &chan_out,
        .stderr_loc = &chan_err,
        .job_loc = &job,
        .filters = TAPI_JOB_SIMPLE_FILTERS(
            {
                .use_stdout = true,
                .filter_name = "Stdout",
                .readable = true,
                .filter_var = &filter_out,
            },
            {
                .use_stderr = true,
                .filter_name = "Stderror",
                .log_level = TE_LL_ERROR,
            }
        ),
    };

    rc = tapi_job_factory_rpc_create(rpcs, &factory);
    if (rc != 0)
        goto out;

    rc = tapi_job_simple_create(factory, &job_desc);
    if (rc != 0)
        goto out;

    rc = tapi_job_start(job);
    if (rc != 0)
        goto out;

    do {
        rc = tapi_job_receive(TAPI_JOB_CHANNEL_SET(filter_out), -1, &buf_out);

        if (rc != 0)
            goto out;
    } while (!buf_out.eos);

    rc = tapi_job_wait(job, -1, &status);
    if (rc != 0)
        goto out;

    if (status.type != TAPI_JOB_STATUS_EXITED || status.value != 0)
    {
        ERROR("RDMA utility finished abnormally");
        rc = TE_RC(TE_TAPI, TE_EFAULT);
        goto out;
    }

    output = buf_out.data.ptr;
    ret = sscanf(output, "link %ms%n", &link_name, &consumed);
    free(link_name);
    if (ret != 1)
    {
        ERROR("Unexpected number of matched items: %d", ret);
        rc = TE_RC(TE_TAPI, TE_EFAULT);
        goto out;
    }
    output += consumed;

    do {
        tapi_rdma_link_stat_t stat;

        ret = sscanf(output, "%ms %"PRIi64"%n", &stat.name, &stat.value,
                     &consumed);
        if (ret == EOF)
            break;

        if (ret != 2)
        {
            if (ret == 1)
                free(stat.name);
            ERROR("Unexpected number of matched items: %d", ret);
            rc = TE_RC(TE_TAPI, TE_EFAULT);
            goto out;
        }

        TE_VEC_APPEND(&stat_vec, stat);
        output += consumed;
    } while(true);

    *stats = TE_ALLOC(sizeof(tapi_rdma_link_stats_t));
    (*stats)->stats = (tapi_rdma_link_stat_t *)stat_vec.data.ptr;
    (*stats)->size = te_vec_size(&stat_vec);

out:
    te_string_free(&buf_out.data);
    tapi_job_destroy(job, -1);
    tapi_job_factory_destroy(factory);
    return rc;
}

/* See description in tapi_rdma.h */
te_errno
tapi_rdma_link_diff_stats(const tapi_rdma_link_stats_t *old_stats,
                          const tapi_rdma_link_stats_t *new_stats,
                          tapi_rdma_link_stats_t **diff_stats)
{
    te_vec diff = TE_VEC_INIT(tapi_rdma_link_stat_t);

    unsigned int i;
    unsigned int j;
    unsigned int matches = 0;

    /*
     * The set of statistics reported by the rdma utility is unlikely to change
     * between calls, but leave some warnings in case it actually happens at
     * some point in the future.
     */
    if (old_stats->size != new_stats->size)
    {
        WARN("%s: input arrays have different sizes: %u and %u",
             __FUNCTION__, old_stats->size, new_stats->size);
    }

    for (i = 0; i < old_stats->size; i++)
    {
        for (j = 0; j < new_stats->size; j++)
        {
            if (strcmp(new_stats->stats[j].name, old_stats->stats[i].name) == 0)
            {
                matches++;
                if (new_stats->stats[j].value != old_stats->stats[i].value)
                {
                    tapi_rdma_link_stat_t stat;

                    stat.name = strdup(new_stats->stats[j].name);
                    stat.value = new_stats->stats[j].value - old_stats->stats[i].value;

                    TE_VEC_APPEND(&diff, stat);
                }
            }
        }
    }

    *diff_stats = TE_ALLOC(sizeof(tapi_rdma_link_stats_t));
    (*diff_stats)->stats = (tapi_rdma_link_stat_t *)diff.data.ptr;
    (*diff_stats)->size = te_vec_size(&diff);

    if (matches != old_stats->size)
    {
        WARN("%s: only %u old stats have been found among %u new ones",
             __FUNCTION__, matches, old_stats->size);
    }

    return 0;
}

/* See description in tapi_rdma.h */
void
tapi_rdma_link_log_stats(tapi_rdma_link_stats_t *stats, const char *description,
                         const char *pattern, bool non_empty,
                         te_log_level log_level)
{
    unsigned int  i;
    te_log_buf   *buf;
    bool          stats_printed = false;

    if (stats == NULL)
    {
        ERROR("Tried to log NULL RDMA statistics");
        return;
    }

    buf = te_log_buf_alloc();

    te_log_buf_append(buf, "%s:\n", description);

    for (i = 0; i < stats->size; i++)
    {
        if (pattern == NULL || strstr(stats->stats[i].name, pattern) != NULL)
        {
            te_log_buf_append(buf, "  %s: %"PRIi64"\n", stats->stats[i].name,
                stats->stats[i].value);
            stats_printed = true;
        }
    }

    if (!stats_printed)
        te_log_buf_append(buf, "<none>");

    if (!non_empty || stats_printed)
        LOG_MSG(log_level, "%s", te_log_buf_get(buf));

    te_log_buf_free(buf);
}
