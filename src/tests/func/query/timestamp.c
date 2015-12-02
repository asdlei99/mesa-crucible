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

#include <poll.h>
#include <stdio.h>
#include "tapi/t.h"

static uint64_t
get_timestamp(void)
{
    size_t buffer_size = 1024;

    VkBuffer buffer = qoCreateBuffer(t_device, .size = buffer_size);

    VkDeviceMemory mem = qoAllocBufferMemory(t_device, buffer,
        .memoryTypeIndex = t_mem_type_index_for_mmap);

    void *map = qoMapMemory(t_device, mem, /*offset*/ 0,
                            buffer_size, /*flags*/ 0);
    memset(map, 0x11, buffer_size);

    qoBindBufferMemory(t_device, buffer, mem, 0);

    VkCommandBuffer cmdBuffer = qoAllocateCommandBuffer(t_device, t_cmd_pool);
    qoBeginCommandBuffer(cmdBuffer);
    vkCmdWriteTimestamp(cmdBuffer, VK_TIMESTAMP_TYPE_TOP, buffer, 0);
    vkCmdWriteTimestamp(cmdBuffer, VK_TIMESTAMP_TYPE_BOTTOM, buffer, 8);
    qoEndCommandBuffer(cmdBuffer);

    qoQueueSubmit(t_queue, 1, &cmdBuffer, VK_NULL_HANDLE);

    vkQueueWaitIdle(t_queue);

    uint64_t *results = map, retval;
    printf("top timestamp:       %20ld  (%016lx)\n", results[0], results[0]);
    printf("bottom timestamp:    %20ld  (%016lx)\n", results[1], results[1]);
    retval = results[0];

    return retval;
}

static void
test_timestamp(void)
{
    uint64_t a, b, freq, elapsed_ms;

    a = get_timestamp();
    t_assert(poll(NULL, 0, 100) == 0);
    b = get_timestamp();

    freq = t_physical_dev_props->limits.timestampFrequency / 1000;
    elapsed_ms = (b - a) / freq;
    printf("difference: %lu - %lu = %lu\n", b / freq, a / freq, elapsed_ms);
    if (elapsed_ms < 90 || elapsed_ms > 110)
        t_fail();
    else
        t_pass();
}

test_define {
    .name = "func.query.timestamp",
    .start = test_timestamp,
    .no_image = true,
};
