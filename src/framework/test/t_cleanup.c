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

#include "test.h"

void
t_cleanup_push_command(enum cru_cleanup_cmd cmd, ...)
{
    ASSERT_TEST_IN_MAJOR_PHASE;

    va_list va;

    va_start(va, cmd);
    t_cleanup_push_commandv(cmd, va);
    va_end(va);
}

void
t_cleanup_push_commandv(enum cru_cleanup_cmd cmd, va_list va)
{
    ASSERT_TEST_IN_MAJOR_PHASE;

    cru_cleanup_push_commandv(current.cleanup, cmd, va);
}

void
t_cleanup_pop(void)
{
    ASSERT_TEST_IN_MAJOR_PHASE;

    cru_cleanup_pop(current.cleanup);
}

void
t_cleanup_pop_all(void)
{
    ASSERT_TEST_IN_MAJOR_PHASE;

    cru_cleanup_pop_all(current.cleanup);
}
