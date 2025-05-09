/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief Testing Results Comparator
 *
 * Implementation of auxiliary routines to work with TRC database.
 *
 *
 * Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.
 */

#include "te_config.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_ASSERT_H
#include <assert.h>
#endif

#include "te_alloc.h"
#include "te_queue.h"
#include "logger_api.h"
#include "logic_expr.h"

#include "te_trc.h"
#include "trc_db.h"

#define CONST_CHAR2XML  (const xmlChar *)

/* See description in trc_db.h */
void
trc_free_test_iter_args_head(trc_test_iter_args_head *head)
{
    trc_test_iter_arg   *p;

    while ((p = TAILQ_FIRST(head)) != NULL)
    {
        TAILQ_REMOVE(head, p, links);
        free(p->name);
        free(p->value);
        free(p);
    }
}

/* See description in trc_db.h */
void
trc_free_test_iter_args(trc_test_iter_args *args)
{
    if (args == NULL)
        return;

    trc_free_test_iter_args_head(&args->head);
    tq_strings_free(&args->save_order, free);
}

/* See description in trc_db.h */
void
trc_test_iter_args_init(trc_test_iter_args *args)
{
    args->node = NULL;
    TAILQ_INIT(&args->head);
    TAILQ_INIT(&args->save_order);
}

/* See description in trc_db.h */
te_errno
trc_test_iter_args_copy(trc_test_iter_args *dst,
                        trc_test_iter_args *src)
{
    trc_test_iter_arg   *dup_arg;
    trc_test_iter_arg   *arg;

    TAILQ_FOREACH(arg, &src->head, links)
    {
        dup_arg = TE_ALLOC(sizeof(*dup_arg));

        dup_arg->name = strdup(arg->name);
        dup_arg->value = strdup(arg->value);
        if (dup_arg->name == NULL ||
            dup_arg->value == NULL)
        {
            free(dup_arg->name);
            free(dup_arg->value);
            free(dup_arg);
            return TE_ENOMEM;
        }

        TAILQ_INSERT_TAIL(&dst->head, dup_arg, links);
    }

    return tq_strings_copy(&dst->save_order, &src->save_order);
}

/* See description in trc_db.h */
trc_test_iter_args *
trc_test_iter_args_dup(trc_test_iter_args *args)
{
    trc_test_iter_args *dup;
    te_errno rc;

    dup = TE_ALLOC(sizeof(*dup));

    trc_test_iter_args_init(dup);

    rc = trc_test_iter_args_copy(dup, args);
    if (rc != 0)
    {
        trc_free_test_iter_args(dup);
        free(dup);
        return NULL;
    }

    return dup;
}

/* See description in trc_db.h */
void
trc_exp_result_entry_free(trc_exp_result_entry *rentry)
{
    if (rentry == NULL)
        return;

    te_test_result_clean(&rentry->result);
    free(rentry->key);
    free(rentry->notes);
}

/* See description in trc_db.h */
void
trc_exp_result_free(trc_exp_result *result)
{
    trc_exp_result_entry   *p;

    if (result == NULL)
        return;

    free(result->tags_str);
    free(result->tags_expr);
    tq_strings_free(result->tags, free);
    free(result->tags);

    while ((p = TAILQ_FIRST(&result->results)) != NULL)
    {
        TAILQ_REMOVE(&result->results, p, links);
        trc_exp_result_entry_free(p);
        free(p);
    }

    free(result->key);
    free(result->notes);
}

/* See description in trc_db.h */
trc_exp_result_entry *
trc_exp_result_entry_dup(trc_exp_result_entry *rentry)
{
    trc_exp_result_entry   *dup_entry;

    dup_entry = TE_ALLOC(sizeof(*dup_entry));
    if (rentry->notes != NULL)
        dup_entry->notes = strdup(rentry->notes);
    if (rentry->key != NULL)
        dup_entry->key = strdup(rentry->key);

    te_test_result_cpy(&dup_entry->result, &rentry->result);

    dup_entry->is_expected = rentry->is_expected;

    return dup_entry;
}

/* See description in trc_db.h */
trc_exp_result *
trc_exp_result_dup(const trc_exp_result *result)
{
    trc_exp_result         *dup;
    trc_exp_result_entry   *p;
    trc_exp_result_entry   *dup_entry;

    if (result == NULL)
        return NULL;

    dup = TE_ALLOC(sizeof(*dup));

    if (result->key != NULL)
        dup->key = strdup(result->key);
    if (result->notes != NULL)
        dup->notes = strdup(result->notes);

    if (result->tags_str != NULL)
        dup->tags_str = strdup(result->tags_str);
    dup->tags_expr = logic_expr_dup(result->tags_expr);

    TAILQ_INIT(&dup->results);

    TAILQ_FOREACH(p, &result->results, links)
    {
        dup_entry = trc_exp_result_entry_dup(p);
        TAILQ_INSERT_TAIL(&dup->results, dup_entry, links);
    }

    return dup;
}

/* See description in trc_db.h */
trc_exp_results *
trc_exp_results_dup(trc_exp_results *results)
{
    trc_exp_result  *result = NULL;
    trc_exp_result  *dup_result = NULL;
    trc_exp_results *dup = NULL;

    if (results == NULL)
        return NULL;

    dup = TE_ALLOC(sizeof(*dup));
    STAILQ_INIT(dup);

    STAILQ_FOREACH(result, results, links)
    {
        dup_result = trc_exp_result_dup(result);
        STAILQ_INSERT_TAIL(dup, dup_result, links);
    }

    return dup;
}

/* See description in trc_db.h */
void
trc_exp_results_free(trc_exp_results *results)
{
    trc_exp_result *p;

    if (results == NULL)
        return;

    while ((p = STAILQ_FIRST(results)) != NULL)
    {
        STAILQ_REMOVE(results, p, trc_exp_result, links);
        trc_exp_result_free(p);
        free(p);
    }
}

/* See description in trc_db.h */
void
trc_free_test_iter(trc_test_iter *iter)
{
    trc_free_test_iter_args(&iter->args);
    free(iter->notes);
    free(iter->filename);
    trc_exp_results_free(&iter->exp_results);
    trc_free_trc_tests(&iter->tests);
}

/* See description in trc_db.h */
void
trc_free_test_iters(trc_test_iters *iters)
{
    trc_test_iter  *p;

    while ((p = TAILQ_FIRST(&iters->head)) != NULL)
    {
        TAILQ_REMOVE(&iters->head, p, links);
        trc_free_test_iter(p);
        free(p);
    }
}

/* See description in trc_db.h */
void
trc_free_trc_test(trc_test *test)
{
    if (test == NULL)
        return;

    free(test->name);
    free(test->notes);
    free(test->objective);
    free(test->filename);
    trc_free_test_iters(&test->iters);
}

/* See description in trc_db.h */
void
trc_free_trc_tests(trc_tests *tests)
{
    trc_test   *p;

    while ((p = TAILQ_FIRST(&tests->head)) != NULL)
    {
        TAILQ_REMOVE(&tests->head, p, links);
        trc_free_trc_test(p);
        free(p);
    }
}

static void trc_remove_exp_results_test(trc_test *test);

/**
 * Remove all expected results from a given iteration,
 * unlink and free related XML nodes.
 *
 * @param iter      TRC DB test iteration structure pointer
 */
static void
trc_remove_exp_results_iter(trc_test_iter *iter)
{
    trc_test    *p;
    xmlNodePtr   child_node;
    xmlNodePtr   aux_node;

    trc_exp_results_free(&iter->exp_results);

    if (iter->node != NULL)
    {
        for (child_node = iter->node->children; child_node != NULL;
             child_node = aux_node)
        {
            aux_node = child_node->next;
            if (xmlStrcmp(child_node->name,
                          CONST_CHAR2XML("results")) == 0)
            {
                xmlUnlinkNode(child_node);
                xmlFreeNode(child_node);
            }
        }
    }

    for (p = TAILQ_FIRST(&iter->tests.head); p != NULL;
         p = TAILQ_NEXT(p, links))
        trc_remove_exp_results_test(p);
}

/**
 * Remove all expected results from iterations of
 * a given test, unlink and free related XML nodes.
 *
 * @param test      TRC DB test structure pointer
 */
static void
trc_remove_exp_results_test(trc_test *test)
{
    trc_test_iter  *p;
    for (p = TAILQ_FIRST(&test->iters.head); p != NULL;
         p = TAILQ_NEXT(p, links))
        trc_remove_exp_results_iter(p);
}

/* See description in trc_db.h */
void
trc_remove_exp_results(te_trc_db *db)
{
    trc_test   *p;

    for (p = TAILQ_FIRST(&db->tests.head); p != NULL;
         p = TAILQ_NEXT(p, links))
        trc_remove_exp_results_test(p);
}

/* See description in trc_db.h */
void
trc_db_close(te_trc_db *db)
{
    if (db == NULL)
        return;

    free(db->filename);
    xmlFreeDoc(db->xml_doc);
    free(db->version);
    trc_free_trc_tests(&db->tests);
    free(db);
}


/* See the description in te_trc.h */
te_errno
trc_db_init(te_trc_db **db)
{
    *db = TE_ALLOC(sizeof(**db));

    TAILQ_INIT(&(*db)->tests.head);
    TAILQ_INIT(&(*db)->globals.head);

    return 0;
}

void
trc_db_test_update_path(trc_test *test)
{
    free(test->path);

    test->path = te_sprintf("%s/%s",
                            ((test->parent != NULL) &&
                             (test->parent->parent->name != NULL)) ?
                            test->parent->parent->path : "", test->name);
}

/* See the description in trc_db.h */
trc_test *
trc_db_new_test(trc_tests *tests, trc_test_iter *parent, const char *name)
{
    trc_test   *p;

    p = TE_ALLOC(sizeof(*p));

    TAILQ_INIT(&p->iters.head);
    LIST_INIT(&p->users);
    p->parent = parent;
    p->path = NULL;
    p->filename = NULL;
    if (name != NULL)
    {
        p->name = strdup(name);
        trc_db_test_update_path(p);

        if (p->name == NULL)
        {
            ERROR("%s(): strdup(%s) failed", __FUNCTION__, name);
            free(p);
            return NULL;
        }
    }
    TAILQ_INSERT_TAIL(&tests->head, p, links);

    return p;
}

/**
 * Create list of arguments.
 *
 * @param args          Empty list of arguments
 * @param n_args        Number of arguments to add into the list
 * @param in_args       Added arguments
 *
 * @return Status code.
 */
static te_errno
trc_db_test_iter_args(trc_test_iter_args *args, unsigned int n_args,
                      trc_report_argument *add_args)
{
    unsigned int        i;

    assert(TAILQ_EMPTY(&args->head));

    for (i = 0; i < n_args; ++i)
    {
        trc_test_iter_arg  *arg;
        trc_test_iter_arg  *insert_after = NULL;

        arg = TE_ALLOC(sizeof(*arg));

        arg->name = strdup(add_args[i].name);
        arg->value = strdup(add_args[i].value);

        if (arg->name == NULL || arg->value == NULL)
        {
            while ((arg = TAILQ_FIRST(&args->head)) != NULL)
            {
                TAILQ_REMOVE(&args->head, arg, links);
                free(arg->name);
                free(arg->value);
                free(arg);
            }
            return TE_RC(TE_TRC, TE_ENOMEM);
        }

        TAILQ_FOREACH_REVERSE(insert_after, &args->head,
                              trc_test_iter_args_head,
                              links)
        {
            if (strcmp(insert_after->name, arg->name) < 0)
                break;
        }

        if (insert_after == NULL)
            TAILQ_INSERT_HEAD(&args->head, arg, links);
        else
            TAILQ_INSERT_AFTER(&args->head, insert_after, arg, links);
    }

    return 0;
}

/* See the description in trc_db.h */
void
trc_db_test_delete_wilds(trc_test *test)
{
    trc_test_iter       *p;
    trc_test_iter_arg   *arg;
    trc_test_iter       *tvar;

    TAILQ_FOREACH_SAFE(p, &test->iters.head, links, tvar)
    {
        TAILQ_FOREACH(arg, &p->args.head, links)
            if (strlen(arg->value) == 0)
                break;

        if (arg != NULL)
        {
            TAILQ_REMOVE(&test->iters.head, p, links);
            trc_free_test_iter(p);
        }
    }
}

/* See the description in trc_db.h */
trc_test_iter *
trc_db_new_test_iter(trc_test *test, unsigned int n_args,
                     trc_report_argument *args,
                     trc_test_iter *insert_before)
{
    trc_test_iter *p;
    te_errno       rc;

    p = TE_ALLOC(sizeof(*p));

    trc_test_iter_args_init(&p->args);

    STAILQ_INIT(&p->exp_results);
    TAILQ_INIT(&p->tests.head);
    LIST_INIT(&p->users);
    p->parent = test;
    p->filename = NULL;
    rc = trc_db_test_iter_args(&p->args, n_args, args);
    if (rc != 0)
    {
        free(p);
        return NULL;
    }

    if (insert_before == NULL)
        TAILQ_INSERT_TAIL(&test->iters.head, p, links);
    else
        TAILQ_INSERT_BEFORE(insert_before, p, links);

    return p;
}

/* See the description in trc_db.h */
void
trc_exp_results_cpy(trc_exp_results *dest, trc_exp_results *src)
{
    trc_exp_result  *exp_r;
    trc_exp_result  *exp_r_dup;

    if (dest == NULL || src == NULL)
        return;

    STAILQ_FOREACH(exp_r, src, links)
    {
        exp_r_dup = trc_exp_result_dup(exp_r);
        STAILQ_INSERT_TAIL(dest, exp_r_dup, links);
    }
}

/* See the description in trc_db.h */
void
trc_db_test_iter_res_cpy(trc_test_iter *dest, trc_test_iter *src)
{
    if (dest->notes != NULL)
        free(dest->notes);

    trc_exp_results_free(&dest->exp_results);

    if (src->notes != NULL)
        dest->notes = strdup(src->notes);

    /* No need to copy, do not free! */
    dest->exp_default = src->exp_default;

    trc_exp_results_cpy(&dest->exp_results, &src->exp_results);
}

/* See the description in trc_db.h */
void
trc_db_test_iter_res_split(trc_test_iter *itr)
{
    trc_exp_result   *exp_r;
    trc_exp_result   *tvar;
    trc_exp_result   *split_r;
    trc_exp_result   *exp_r_prev = NULL;
    trc_exp_result   *last_r = NULL;
    logic_expr      **le_arr;
    int               le_count = 0;
    int               i;
    int               rc;

    STAILQ_FOREACH(exp_r, &itr->exp_results, links)
        last_r = exp_r;

    if (last_r == NULL)
        return;

    exp_r_prev = last_r;

    STAILQ_FOREACH_SAFE(exp_r, &itr->exp_results, links, tvar)
    {
        logic_expr_dnf(&exp_r->tags_expr, NULL);
        rc =  logic_expr_dnf_split(exp_r->tags_expr, &le_arr,
                                   &le_count);

        if (rc != 0)
            return;

        free(exp_r->tags_str);
        free(exp_r->tags_expr);
        exp_r->tags_str = NULL;
        exp_r->tags_expr = NULL;

        for (i = 0; i < le_count; i++)
        {
            split_r = trc_exp_result_dup(exp_r);
            split_r->tags_expr = le_arr[i];
            split_r->tags_str = logic_expr_to_str(split_r->tags_expr);

            STAILQ_INSERT_AFTER(&itr->exp_results, exp_r_prev, split_r, links);

            exp_r_prev = split_r;
        }

        free(le_arr);

        STAILQ_REMOVE_HEAD(&itr->exp_results, links);
        trc_exp_result_free(exp_r);
        if (exp_r == last_r)
            break;
    }
}

/* See the description in te_trc.h */
unsigned int
trc_db_new_user(te_trc_db *db)
{
    assert(db != NULL);
    return db->user_id++;
}

/* See the description in te_trc.h */
void
trc_db_free_user(te_trc_db *db, unsigned int user_id)
{
    UNUSED(db);
    UNUSED(user_id);
}

/**
 * Find user data attached to the current element of the TRC database.
 *
 * @param users_data    Users data for the current element
 * @param user_id       User ID
 *
 * @return Pointer to internal list element or NULL.
 */
static trc_user_data *
trc_db_find_user_data(const trc_users_data *users_data,
                      unsigned int          user_id)
{
    const trc_user_data *p;

    assert(users_data != NULL);
    for (p = LIST_FIRST(users_data);
         p != NULL && p->user_id != user_id;
         p = LIST_NEXT(p, links));

    /*
     * Discard 'const' qualifier inherited from the list, since it
     * means that the function does not modify the list.
     */
    return (trc_user_data *)p;
}

/**
 * Find user data attached to the current element of the TRC database.
 *
 * @param walker        TRC database walker
 * @param user_id       User ID
 *
 * @return Pointer to internal list element or NULL.
 */
static trc_user_data *
trc_db_walker_find_user_data(const te_trc_db_walker *walker,
                             unsigned int            user_id)
{
    return trc_db_find_user_data(trc_db_walker_users_data(walker),
                                 user_id);
}

/**
 * Find user data attached to the parrent of current element of
 * the TRC database.
 *
 * @param walker        TRC database walker
 * @param user_id       User ID
 *
 * @return Pointer to internal list element or NULL.
 */
static trc_user_data *
trc_db_walker_find_parent_user_data(const te_trc_db_walker *walker,
                                    unsigned int            user_id)
{
    return trc_db_find_user_data(trc_db_walker_parent_users_data(walker),
                                 user_id);
}

/* See the description in te_trc.h */
void *
trc_db_walker_get_user_data(const te_trc_db_walker *walker,
                            unsigned int            user_id)
{
    trc_user_data *ud =
        trc_db_walker_find_user_data(walker, user_id);

    return (ud == NULL) ? NULL : ud->data;
}

/* See the description in te_trc.h */
void *
trc_db_walker_get_parent_user_data(const te_trc_db_walker *walker,
                                   unsigned int            user_id)
{
    trc_user_data *ud =
        trc_db_walker_find_parent_user_data(walker, user_id);

    return (ud == NULL) ? NULL : ud->data;
}

/* See the description in trc_db.h */
void *
trc_db_test_get_user_data(const trc_test *test, unsigned int user_id)
{
    trc_user_data *ud = trc_db_find_user_data(&test->users, user_id);

    return (ud == NULL) ? NULL : ud->data;
}

/* See the description in trc_db.h */
void *
trc_db_iter_get_user_data(const trc_test_iter *iter, unsigned int user_id)
{
    trc_user_data *ud = trc_db_find_user_data(&iter->users, user_id);

    return (ud == NULL) ? NULL : ud->data;
}

/* See the description in trc_db.h */
te_errno
trc_db_set_user_data(void *db_item, bool is_iter, unsigned int user_id,
                     void *user_data)
{
    trc_users_data  *users = is_iter ? &((trc_test_iter *)db_item)->users :
                                       &((trc_test *)db_item)->users;

    trc_user_data *ud = trc_db_find_user_data(users, user_id);

    if (ud == NULL)
    {
        ud = TE_ALLOC(sizeof(*ud));

        ud->user_id = user_id;
        LIST_INSERT_HEAD(users, ud, links);
    }

    ud->data = user_data;

    return 0;
}

/* See the description in trc_db.h */
te_errno
trc_db_iter_set_user_data(trc_test_iter *iter, unsigned int user_id,
                          void *user_data)
{
    return trc_db_set_user_data(iter, true, user_id, user_data);
}

/* See the description in trc_db.h */
te_errno
trc_db_test_set_user_data(trc_test *test, unsigned int user_id,
                          void *user_data)
{
    return trc_db_set_user_data(test, false, user_id, user_data);
}

/* See the description in te_trc.h */
te_errno
trc_db_walker_set_prop_ud(const te_trc_db_walker *walker,
                          unsigned int user_id, void *user_data,
                          void *(*data_gen)(void *, bool))
{
    bool is_iter = trc_db_walker_is_iter(walker);
    void       *p = is_iter ? (void *)trc_db_walker_get_iter(walker) :
                                (void *)trc_db_walker_get_test(walker);

    trc_users_data  *list;
    trc_user_data   *ud;

    while (p != NULL)
    {
        list = is_iter ? &((trc_test_iter *)p)->users :
                          &((trc_test *)p)->users;
        ud = trc_db_find_user_data(list, user_id);

        if (ud == NULL)
        {
            ud = TE_ALLOC(sizeof(*ud));

            ud->user_id = user_id;
            LIST_INSERT_HEAD(list, ud, links);
        }

        if (data_gen == NULL)
            ud->data = user_data;
        else
            ud->data = data_gen(user_data, is_iter);

        p = is_iter ? (void *)((trc_test_iter *)p)->parent :
                        (void *)((trc_test *)p)->parent;
        is_iter = !is_iter;
    }

    return 0;
}

/* See the description in te_trc.h */
te_errno
trc_db_walker_set_user_data(const te_trc_db_walker *walker,
                            unsigned int user_id, void *user_data)
{
    trc_user_data *ud = trc_db_walker_find_user_data(walker, user_id);

    if (ud == NULL)
    {
        trc_users_data *list = trc_db_walker_users_data(walker);

        ud = TE_ALLOC(sizeof(*ud));

        ud->user_id = user_id;
        LIST_INSERT_HEAD(list, ud, links);
    }

    ud->data = user_data;

    return 0;
}

/* See the description in te_trc.h */
void
trc_db_walker_free_user_data(te_trc_db_walker *walker,
                             unsigned int user_id,
                             void (*user_free)(void *))
{
    trc_user_data *ud = trc_db_walker_find_user_data(walker, user_id);

    if (ud != NULL)
    {
        LIST_REMOVE(ud, links);
        if (user_free != NULL)
            user_free(ud->data);
        free(ud);
    }
}

/* See the description in te_trc.h */
te_errno
trc_db_free_user_data(te_trc_db *db, unsigned int user_id,
                      void (*test_free)(void *),
                      void (*iter_free)(void *))
{
    te_trc_db_walker       *walker;
    trc_db_walker_motion    mv;

    walker = trc_db_new_walker(db);
    if (walker == NULL)
        return TE_ENOMEM;

    while ((mv = trc_db_walker_move(walker)) != TRC_DB_WALKER_ROOT)
    {
        if (mv != TRC_DB_WALKER_FATHER)
            trc_db_walker_free_user_data(walker, user_id,
                trc_db_walker_is_iter(walker) ? iter_free : test_free);
    }

    trc_db_free_walker(walker);

    return 0;
}

/* See the description in trc_db.h */
void *
trc_db_get_test_by_path(te_trc_db *db, char *path)
{
#define PATH_ITEM_LEN   20
    char            path_item[PATH_ITEM_LEN];
    int             i;
    int             j = 0;
    trc_test       *test = NULL;
    trc_tests      *tests;
    trc_test_iter  *iter;
    trc_test_iters *iters = NULL;
    bool is_iter;

    if (path == NULL)
        return NULL;

    is_iter = false;
    tests = &db->tests;

    for (i = 0; i <= (int)strlen(path); i++)
    {
        if (path[i] == '/' || path[i] == '\0')
        {
            if (j > 0)
            {
                path_item[j] = '\0';

                if (is_iter)
                {
                    TAILQ_FOREACH(iter, &iters->head, links)
                        TAILQ_FOREACH(test, &iter->tests.head, links)
                            if (strcmp(test->name, path_item) == 0)
                                break;

                    if (test == NULL)
                        return NULL;
                    else
                        iters = &test->iters;
                }
                else
                {
                    TAILQ_FOREACH(test, &tests->head, links)
                        if (strcmp(test->name, path_item) == 0)
                            break;

                    if (test == NULL)
                        return NULL;
                    else
                    {
                        is_iter = true;
                        iters = &test->iters;
                    }
                }

                j = 0;
            }
        }
        else
            path_item[j++] = path[i];

        if (j == PATH_ITEM_LEN)
            return NULL;
    }

    return test;

#undef PATH_ITEM_LEN
}

/* See the description in trc_db.h */
const trc_exp_result *
trc_db_iter_get_exp_result(const trc_test_iter    *iter,
                           const tqh_strings      *tags,
                           bool last_match)
{
    const trc_exp_result       *result;
    const trc_exp_result       *p;
    int                         res;
    const trc_exp_result_entry *q;

    if (iter == NULL)
        return NULL;

    /* Do we have a tag with expected SKIPPED result? */
    result = NULL;
    STAILQ_FOREACH(p, &iter->exp_results, links)
    {
        VERB("%s: matching start", __FUNCTION__);

        /*
         * If there is no tag expression, consider result as matching
         * any tags.
         */
        if (p->tags_expr == NULL)
            res = 1;
        else
            res = logic_expr_match(p->tags_expr, tags);

        if (res != -1)
        {
            INFO("Matching tag found");
            TAILQ_FOREACH(q, &p->results, links)
            {
                if (q->result.status == TE_TEST_SKIPPED)
                {
                    /* Skipped results have top priority in any case */
                    result = p;
                    break;
                }
            }
            if (q != NULL)
                break;

            if (result == NULL || last_match)
                result = p;
        }
    }

    /* We have not found matching tagged result */
    if (result == NULL)
    {
        /* May be default expected result exists? */
        result = iter->exp_default;
    }

    if (result == NULL)
    {
        INFO("Expected result is not known");
    }

    return result;
}

/* See the description in trc_db.h */
int
trc_test_result_cmp(const te_test_result *p, const te_test_result *q)
{
    int              rc;
    te_test_verdict *p_v;
    te_test_verdict *q_v;

    TRC_DUMMY_CMP(p, q);

    rc = p->status - q->status;
    if (rc != 0)
        return rc;

    p_v = TAILQ_FIRST(&p->verdicts);
    q_v = TAILQ_FIRST(&q->verdicts);

    while (p_v != NULL && q_v != NULL)
    {
        rc = strcmp(p_v->str, q_v->str);
        if (rc != 0)
            return rc;

        p_v = TAILQ_NEXT(p_v, links);
        q_v = TAILQ_NEXT(q_v, links);
    }

    if (p_v == NULL && q_v == NULL)
        return 0;
    else if (p_v != NULL)
        return 1;
    else
        return -1;
}

/* See the description in trc_db.h */
int
trc_exp_rentry_cmp(const trc_exp_result_entry *p,
                   const trc_exp_result_entry *q,
                   trc_results_cmp_flags flags)
{
    int rc = 0;

    TRC_DUMMY_CMP(p, q);

    if (!p->is_expected && q->is_expected)
        return -1;
    else if (p->is_expected && !q->is_expected)
        return 1;

    rc = trc_test_result_cmp(&p->result,
                             &q->result);
    if (rc != 0)
        return rc;

    rc = strcmp_null(p->key, q->key);
    if (rc != 0)
        return rc;

    if (flags & RESULTS_CMP_NO_NOTES)
        return 0;

    rc = strcmp_null(p->notes, q->notes);
    if (rc != 0)
        return rc;

    return 0;
}

/* See the description in trc_db.h */
int
trc_exp_result_cmp(const trc_exp_result *p, const trc_exp_result *q,
                   trc_results_cmp_flags flags)
{
    int rc;
    trc_exp_result_entry *p_r;
    trc_exp_result_entry *q_r;
    bool p_tags_str_void;
    bool q_tags_str_void;

    TRC_DUMMY_CMP(p, q);

    if (flags & RESULTS_CMP_NO_TAGS)
    {
        rc = 0;
    }
    else
    {
        p_tags_str_void = (p->tags_str == NULL ||
                           strlen(p->tags_str) == 0);
        q_tags_str_void = (q->tags_str == NULL ||
                           strlen(q->tags_str) == 0);

        if (p_tags_str_void && q_tags_str_void)
            rc = 0;
        else if (p_tags_str_void)
            rc = -1;
        else if (q_tags_str_void)
            rc = 1;
        else
            rc = strcmp(p->tags_str, q->tags_str);
    }

    if (rc != 0)
        return rc;

    p_r = TAILQ_FIRST(&p->results);
    q_r = TAILQ_FIRST(&q->results);

    while (p_r != NULL && q_r != NULL)
    {
        rc = trc_exp_rentry_cmp(p_r, q_r, flags);
        if (rc != 0)
            return rc;

        p_r = TAILQ_NEXT(p_r, links);
        q_r = TAILQ_NEXT(q_r, links);
    }

    if (p_r == NULL && q_r == NULL)
    {
        rc = strcmp_null(p->key, q->key);
        if (rc != 0)
            return rc;
        else
            return strcmp_null(p->notes, q->notes);
    }
    else if (p_r != NULL)
    {
        return 1;
    }
    else
    {
        return -1;
    }
}

/* See the description in trc_db.h */
int
trc_exp_results_cmp(const trc_exp_results *p, const trc_exp_results *q,
                    trc_results_cmp_flags flags)
{
    int rc;
    trc_exp_result *p_r;
    trc_exp_result *q_r;
    bool p_is_void = (p == NULL || STAILQ_EMPTY(p));
    bool q_is_void = (q == NULL || STAILQ_EMPTY(q));

    if (p_is_void && q_is_void)
        return 0;
    else if (!p_is_void && q_is_void)
        return 1;
    else if (p_is_void && !q_is_void)
        return -1;

    p_r = STAILQ_FIRST(p);
    q_r = STAILQ_FIRST(q);

    while (p_r != NULL && q_r != NULL)
    {
        rc = trc_exp_result_cmp(p_r, q_r, flags);
        if (rc != 0)
            return rc;

        p_r = STAILQ_NEXT(p_r, links);
        q_r = STAILQ_NEXT(q_r, links);
    }

    if (p_r == NULL && q_r == NULL)
        return 0;
    else if (p_r != NULL)
        return 1;
    else
        return -1;
}
