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

#include <stdio.h>
#include <stdlib.h>

#include <crucible/xalloc.h>

void cru_noreturn
xdie_oom(void)
{
    fprintf(stderr, "out of memory\n");
    abort();
}

void *
xmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p)
        xdie_oom();
    return p;
}

void *
xrealloc(void *mem, size_t size)
{
    void *p = realloc(mem, size);
    if (!p)
        xdie_oom();
    return p;
}

void *
xzalloc(size_t size)
{
    return xcalloc(1, size);
}

void *
xcalloc(size_t n, size_t size)
{
    void *p = calloc(n, size);
    if (!p)
        xdie_oom();
    return p;
}
