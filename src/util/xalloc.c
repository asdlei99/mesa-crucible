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

#include <stdlib.h>

#include "util/misc.h"
#include "util/xalloc.h"

void *
xmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p)
        cru_oom();
    return p;
}

void *
xmallocn(size_t n, size_t size)
{
    size_t total_size;

    if (!unlikely(cru_mul_size_checked(&total_size, n, size)))
        cru_oom();

    return xmalloc(total_size);
}

void *
xrealloc(void *mem, size_t size)
{
    void *p = realloc(mem, size);
    if (!p)
        cru_oom();
    return p;
}

void *
xreallocn(void *mem, size_t n, size_t size)
{
    size_t total_size;

    if (!unlikely(cru_mul_size_checked(&total_size, n, size)))
        cru_oom();

    return xrealloc(mem, total_size);
}

void *
xzalloc(size_t size)
{
    void *p = calloc(1, size);
    if (!p)
        cru_oom();
    return p;
}

void *
xzallocn(size_t n, size_t size)
{
    size_t total_size;

    if (!unlikely(cru_mul_size_checked(&total_size, n, size)))
        cru_oom();

    return xzalloc(total_size);
}

char *
xstrdup(const char *s)
{
    char *t = strdup(s);
    if (!t)
        cru_oom();
    return t;
}
