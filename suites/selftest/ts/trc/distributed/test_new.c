/** @file
 * @brief
 * Check distributed TRC.
 *
 * Copyright (C) 2025 OKTET Labs Ltd., St. Petersburg, Russia
 */

/** @page test_new Trivial test for checking TRC.
 *
 * @objective Check distributed TRC when a test exists only in the
 *            merged database.
 *
 * @param a     The only argument.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <dmitry.izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "distributed/test_new"

#include "tapi_test.h"
#include "tapi_env.h"

int
main(int argc, char **argv)
{
    int a;

    TEST_START;
    TEST_GET_INT_PARAM(a);

    if (a != 1)
        TEST_VERDICT("a is equal to %d", a);

    TEST_SUCCESS;

cleanup:

    TEST_END;
}
