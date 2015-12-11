// Copyright 2015 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#pragma once

#include <fnmatch.h>

#include "util/misc.h"
#include "util/cru_vec.h"
#include "tapi/t_def.h"

typedef struct test_def_vec test_def_vec_t;

extern test_def_t __start_test_defs, __stop_test_defs;

#define cru_foreach_test_def(def) \
   for (def = &__start_test_defs; \
        def < &__stop_test_defs; ++def)


static pure inline uint64_t
test_def_get_id(const test_def_t *def)
{
    return def - &__start_test_defs;
}

static pure inline test_def_t *
test_def_from_id(uint64_t id)
{
    test_def_t *def = &__start_test_defs + id;

    if (def < &__stop_test_defs) {
        return def;
    } else {
        return NULL;
    }
}

/// Match the test name against the glob pattern.
///
/// Crucible's example tests and self tests are special. The user doesn't
/// want to run them during normal test runs. Therefore example tests match
/// only patterns that begin with a literal "example.", and self tests
/// a literal "self.".
static inline bool
test_def_match(const test_def_t *def, const char *glob)
{
    if (strncmp("example.", def->name, 8) == 0 &&
        strncmp("example.", glob, 8) != 0) {
        return false;
    }

    if (strncmp("self.", def->name, 5) == 0 &&
        strncmp("self.", glob, 5) != 0) {
        return false;
    }

    return fnmatch(glob, def->name, 0) == 0;
}

static inline uint32_t
cru_num_defs(void)
{
    return &__stop_test_defs - &__start_test_defs;
}

static inline const test_def_t *
cru_find_def(const char *name)
{
    const test_def_t *def;

    cru_foreach_test_def(def) {
        if (cru_streq(def->name, name)) {
            return def;
        }
    }

    return NULL;
}
