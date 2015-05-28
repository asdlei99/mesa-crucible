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

#include <crucible/cru.h>
#include <poll.h>
#include <stdio.h>

static uint64_t
get_timestamp(void)
{
    VkBuffer buffer;
    vkCreateBuffer(t_device,
                   &(VkBufferCreateInfo) {
                       .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                           .size = 1024,
                           .usage = VK_BUFFER_USAGE_GENERAL,
                           .flags = 0
                           },
                   &buffer);

    VkMemoryRequirements buffer_requirements;
    size_t size = sizeof(buffer_requirements);
    vkGetObjectInfo(t_device, VK_OBJECT_TYPE_BUFFER, buffer,
                    VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS,
                    &size, &buffer_requirements);

    VkDeviceMemory mem;
    vkAllocMemory(t_device,
                  &(VkMemoryAllocInfo) {
                      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO,
                          .allocationSize = buffer_requirements.size,
                          .memProps = VK_MEMORY_PROPERTY_HOST_DEVICE_COHERENT_BIT,
                          .memPriority = VK_MEMORY_PRIORITY_NORMAL
                          },
                  &mem);

    void *map;
    vkMapMemory(t_device, mem, 0, buffer_requirements.size, 0, &map);
    memset(map, 0x11, buffer_requirements.size);

    vkQueueBindObjectMemory(t_queue, VK_OBJECT_TYPE_BUFFER,
                            buffer,
                            0, /* allocation index; for objects which need to bind to multiple mems */
                            mem, 0);

    VkCmdBuffer cmdBuffer;
    vkCreateCommandBuffer(t_device,
                          &(VkCmdBufferCreateInfo) {
                              .sType = VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
                                  .queueNodeIndex = 0,
                                  .flags = 0
                                  },
                          &cmdBuffer);

    vkBeginCommandBuffer(cmdBuffer,
                         &(VkCmdBufferBeginInfo) {
                             .sType = VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
                                 .flags = 0
                                 });

    vkCmdWriteTimestamp(cmdBuffer, VK_TIMESTAMP_TYPE_TOP, buffer, 0);
    vkCmdWriteTimestamp(cmdBuffer, VK_TIMESTAMP_TYPE_BOTTOM, buffer, 8);

    vkEndCommandBuffer(cmdBuffer);

    vkQueueSubmit(t_queue, 1, &cmdBuffer, 0);

    vkQueueWaitIdle(t_queue);

    vkDestroyObject(t_device, VK_OBJECT_TYPE_BUFFER, buffer);
    vkDestroyObject(t_device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmdBuffer);

    uint64_t *results = map, retval;
    printf("top timestamp:       %20ld  (%016lx)\n", results[0], results[0]);
    printf("bottom timestamp:    %20ld  (%016lx)\n", results[1], results[1]);
    retval = results[0];

    vkUnmapMemory(t_device, mem);
    vkFreeMemory(t_device, mem);

    return retval;
}

static void
test_timestamp(void)
{
    uint64_t a, b, freq, elapsed_ms;

    a = get_timestamp();
    t_assert(poll(NULL, 0, 100) == 0);
    b = get_timestamp();

    VkResult result;
    VkPhysicalDeviceProperties properties;
    size_t size = sizeof(properties);

    result = vkGetPhysicalDeviceInfo(t_physical_device,
                                     VK_PHYSICAL_DEVICE_INFO_TYPE_PROPERTIES,
                                     &size, &properties);
    t_assert(result == VK_SUCCESS);

    freq = properties.timestampFrequency / 1000;
    elapsed_ms = (b - a) / freq;
    printf("difference: %lu - %lu = %lu\n", b / freq, a / freq, elapsed_ms);
    if (elapsed_ms < 90 || elapsed_ms > 110)
        t_fail();
    else
        t_pass();
}

cru_define_test {
    .name = "func.query.timestamp",
    .start = test_timestamp,
    .no_image = true,
};
