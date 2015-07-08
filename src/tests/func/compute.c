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

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <libpng16/png.h>

#include <crucible/cru.h>

#include "compute-spirv.h"

static inline uint32_t
align_u32(uint32_t value, uint32_t alignment)
{
   return (value + alignment - 1) & ~(alignment - 1);
}

static void
test(void)
{
    VkDescriptorSetLayout set_layout;

    set_layout = qoCreateDescriptorSetLayout(t_device,
            .count = 2,
            .pBinding = (VkDescriptorSetLayoutBinding[]) {
                {
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .arraySize = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .pImmutableSamplers = NULL,
                },
                {
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .arraySize = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .pImmutableSamplers = NULL,
                },
            });

    VkPipelineLayout pipeline_layout = qoCreatePipelineLayout(t_device,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout);

    VkShader cs = qoCreateShaderGLSL(
        t_device, COMPUTE,

        layout (local_size_x = 8, local_size_y = 2, local_size_z = 2) in;

        void main()
        {
        }
    );

    VkPipeline pipeline;
    vkCreateComputePipeline(
        t_device,
        &(VkComputePipelineCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = NULL,
            .cs = {
                .stage = VK_SHADER_STAGE_COMPUTE,
                .shader = cs,
            },
            .flags = 0,
            .layout = pipeline_layout
        }, &pipeline);

    VkDescriptorSet set;
    qoAllocDescriptorSets(t_device, /*pool*/ 0,
                          VK_DESCRIPTOR_SET_USAGE_STATIC,
                          1, &set_layout, &set);

    VkBuffer buffer = qoCreateBuffer(t_device, .size = 1024,
                                     .usage = VK_BUFFER_USAGE_GENERAL);

    VkDeviceMemory mem = qoAllocMemory(t_device, .allocationSize = 4096);
    qoQueueBindBufferMemory(t_queue, buffer, /*index*/ 0, mem, 0);

    VkBufferView buffer_view;
    buffer_view = qoCreateBufferView(t_device,
        .buffer = buffer,
        .viewType = VK_BUFFER_VIEW_TYPE_RAW,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = 0, .range = 64);

    VkSampler sampler = qoCreateSampler(t_device,
        .magFilter = VK_TEX_FILTER_LINEAR,
        .minFilter = VK_TEX_FILTER_LINEAR,
        .mipMode = VK_TEX_MIPMAP_MODE_NEAREST,
        .addressU = VK_TEX_ADDRESS_CLAMP,
        .addressV = VK_TEX_ADDRESS_CLAMP,
        .addressW = VK_TEX_ADDRESS_CLAMP,
        .mipLodBias = 0,
        .maxAnisotropy = 0,
        .compareOp = VK_COMPARE_OP_GREATER,
        .minLod = 0,
        .maxLod = 5,
        .borderColor = VK_BORDER_COLOR_TRANSPARENT_BLACK);

   vkUpdateDescriptors(t_device,
        set, 2,
        (const void * []) {
            &(VkUpdateBuffers) {
                .sType = VK_STRUCTURE_TYPE_UPDATE_BUFFERS,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .arrayIndex = 0,
                .binding = 0,
                .count = 1,
                .pBufferViews = (VkBufferViewAttachInfo[]) {
                    {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_ATTACH_INFO,
                        .view = buffer_view,
                    },
                },
            },
            &(VkUpdateSamplers) {
                .sType = VK_STRUCTURE_TYPE_UPDATE_SAMPLERS,
                .binding = 1,
                .count = 1,
                .pSamplers = (const VkSampler[]) { sampler }
            },
        });

    vkCmdBindPipeline(t_cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    vkCmdBindDescriptorSets(t_cmd_buffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline_layout, 0, 1,
                            &set, 0, NULL);

    vkCmdDispatch(t_cmd_buffer, 8, 2, 2);

    qoEndCommandBuffer(t_cmd_buffer);
    qoQueueSubmit(t_queue, 1, &t_cmd_buffer, 0);
}

cru_define_test {
    .name = "func.compute",
    .start = test,
    .no_image = true,
};
