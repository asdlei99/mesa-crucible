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

#include <string.h>

#include "framework/test/test.h"
#include "qonos/qonos.h"

void
qoEnumeratePhysicalDevices(VkInstance instance, uint32_t *count,
                           VkPhysicalDevice *physical_devices)
{
    VkResult result;

    result = vkEnumeratePhysicalDevices(instance, count, physical_devices);
    t_assert(result == VK_SUCCESS);
}

void
qoGetPhysicalDeviceProperties(VkPhysicalDevice physical_dev,
                              VkPhysicalDeviceProperties *properties)
{
    vkGetPhysicalDeviceProperties(physical_dev, properties);
}

void
qoGetPhysicalDeviceMemoryProperties(
        VkPhysicalDevice physical_dev,
        VkPhysicalDeviceMemoryProperties *mem_props)
{
    vkGetPhysicalDeviceMemoryProperties(physical_dev, mem_props);
}

VkMemoryRequirements
qoGetBufferMemoryRequirements(VkDevice dev, VkBuffer buffer)
{
    VkMemoryRequirements mem_reqs = {0};

    vkGetBufferMemoryRequirements(dev, buffer, &mem_reqs);

    return mem_reqs;
}

VkMemoryRequirements
qoGetImageMemoryRequirements(VkDevice dev, VkImage image)
{
    VkMemoryRequirements mem_reqs = {0};

    vkGetImageMemoryRequirements(dev, image, &mem_reqs);

    return mem_reqs;
}

VkResult
qoBindBufferMemory(VkDevice device, VkBuffer buffer,
                   VkDeviceMemory mem, VkDeviceSize offset)
{
    VkResult result;

    result = vkBindBufferMemory(device, buffer, mem, offset);
    t_assert(result == VK_SUCCESS);

    return result;
}

VkResult
qoBindImageMemory(VkDevice device, VkImage image,
                  VkDeviceMemory mem, VkDeviceSize offset)
{
    VkResult result;

    result = vkBindImageMemory(device, image, mem, offset);
    t_assert(result == VK_SUCCESS);

    return result;
}

VkResult
qoQueueSubmit(VkQueue queue, uint32_t cmdBufferCount,
              const VkCommandBuffer *commandBuffers, VkFence fence)
{
    VkResult result;

    result = vkQueueSubmit(queue, 1,
        &(VkSubmitInfo) {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = cmdBufferCount,
            .pCommandBuffers = commandBuffers,
        }, fence);
    t_assert(result == VK_SUCCESS);

    return result;
}

VkResult
qoQueueWaitIdle(VkQueue queue)
{
    VkResult result;

    result = vkQueueWaitIdle(queue);
    t_assert(result == VK_SUCCESS);

    return result;
}

VkDeviceMemory
__qoAllocMemory(VkDevice dev, const VkMemoryAllocateInfo *info)
{
    VkDeviceMemory memory = {0};
    VkResult result;

    t_assert(info->memoryTypeIndex != QO_MEMORY_TYPE_INDEX_INVALID);

    result = vkAllocateMemory(dev, info, NULL, &memory);

    t_assert(result == VK_SUCCESS);
    t_assert(memory != VK_NULL_HANDLE);
    t_cleanup_push_vk_device_memory(dev, memory);

    return memory;
}

VkDeviceMemory
__qoAllocMemoryFromRequirements(VkDevice dev,
                                const VkMemoryRequirements *mem_reqs,
                                const VkMemoryAllocateInfo *override_info)
{
    VkMemoryAllocateInfo info = *override_info;

    if (info.allocationSize == 0)
        info.allocationSize = mem_reqs->size;

    t_assert(info.allocationSize >= mem_reqs->size);
    t_assert(info.memoryTypeIndex != QO_MEMORY_TYPE_INDEX_INVALID);
    t_assert((1 << info.memoryTypeIndex) & mem_reqs->memoryTypeBits);

    return __qoAllocMemory(dev, &info);
}

VkDeviceMemory
__qoAllocBufferMemory(VkDevice dev, VkBuffer buffer,
                      const VkMemoryAllocateInfo *override_info)
{
    VkMemoryRequirements mem_reqs =
        qoGetBufferMemoryRequirements(dev, buffer);

    return __qoAllocMemoryFromRequirements(dev, &mem_reqs, override_info);
}

VkDeviceMemory
__qoAllocImageMemory(VkDevice dev, VkImage image,
                     const VkMemoryAllocateInfo *override_info)
{
    VkMemoryRequirements mem_reqs =
        qoGetImageMemoryRequirements(dev, image);

    return __qoAllocMemoryFromRequirements(dev, &mem_reqs, override_info);
}

void *
qoMapMemory(VkDevice dev, VkDeviceMemory mem,
            VkDeviceSize offset, VkDeviceSize size,
            VkMemoryMapFlags flags)
{
    void *map;
    VkResult result;

    result = vkMapMemory(dev, mem, offset, size, flags, &map);

    t_assert(result == VK_SUCCESS);
    t_assert(map);
    t_cleanup_push_vk_device_memory_map(dev, mem);

    return map;
}

VkPipelineCache
__qoCreatePipelineCache(VkDevice dev, const VkPipelineCacheCreateInfo *info)
{
    VkPipelineCache pipeline_cache = {0};
    VkResult result;

    result = vkCreatePipelineCache(dev, info, NULL, &pipeline_cache);

    t_assert(result == VK_SUCCESS);
    t_assert(pipeline_cache != VK_NULL_HANDLE);
    t_cleanup_push_vk_pipeline_cache(dev, pipeline_cache);

    return pipeline_cache;
}

VkPipelineLayout
__qoCreatePipelineLayout(VkDevice dev, const VkPipelineLayoutCreateInfo *info)
{
    VkPipelineLayout pipeline_layout = {0};
    VkResult result;

    result = vkCreatePipelineLayout(dev, info, NULL, &pipeline_layout);

    t_assert(result == VK_SUCCESS);
    t_assert(pipeline_layout != VK_NULL_HANDLE);
    t_cleanup_push_vk_pipeline_layout(dev, pipeline_layout);

    return pipeline_layout;
}

VkSampler
__qoCreateSampler(VkDevice dev, const VkSamplerCreateInfo *info)
{
    VkSampler sampler = {0};
    VkResult result;

    result = vkCreateSampler(dev, info, NULL, &sampler);

    t_assert(result == VK_SUCCESS);
    t_assert(sampler != VK_NULL_HANDLE);
    t_cleanup_push_vk_sampler(dev, sampler);

    return sampler;
}

VkDescriptorSetLayout
__qoCreateDescriptorSetLayout(VkDevice dev,
                              const VkDescriptorSetLayoutCreateInfo *info)
{
    VkDescriptorSetLayout layout = {0};
    VkResult result;

    result = vkCreateDescriptorSetLayout(dev, info, NULL, &layout);

    t_assert(result == VK_SUCCESS);
    t_assert(layout != VK_NULL_HANDLE);
    t_cleanup_push_vk_descriptor_set_layout(dev, layout);

    return layout;
}

VkDescriptorSet
__qoAllocateDescriptorSet(VkDevice dev, const VkDescriptorSetAllocateInfo *info)
{
    VkDescriptorSet set;
    VkResult result;

    t_assert(info->descriptorSetCount == 1);
    t_assert(info->pSetLayouts != NULL);

    result = vkAllocateDescriptorSets(dev, info, &set);

    t_assert(result == VK_SUCCESS);
    t_assert(set != VK_NULL_HANDLE);
    t_cleanup_push_vk_descriptor_set(dev, info->descriptorPool, set);

    return set;
}

VkBuffer
__qoCreateBuffer(VkDevice dev, const VkBufferCreateInfo *info)
{
    VkBuffer buffer = {0};
    VkResult result;

    result = vkCreateBuffer(dev, info, NULL, &buffer);

    t_assert(result == VK_SUCCESS);
    t_assert(buffer != VK_NULL_HANDLE);
    t_cleanup_push_vk_buffer(dev, buffer);

    return buffer;
}

VkBufferView
__qoCreateBufferView(VkDevice dev, const VkBufferViewCreateInfo *info)
{
    VkBufferView view = {0};
    VkResult result;

    result = vkCreateBufferView(dev, info, NULL, &view);

    t_assert(result == VK_SUCCESS);
    t_assert(view != VK_NULL_HANDLE);
    t_cleanup_push_vk_buffer_view(dev, view);

    return view;
}

VkCommandBuffer
__qoAllocateCommandBuffer(VkDevice dev, VkCommandPool pool,
                          const VkCommandBufferAllocateInfo *info)
{
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkResult result;

    assert(info->commandPool == pool);
    assert(info->commandBufferCount == 1);

    result = vkAllocateCommandBuffers(dev, info, &cmd);

    t_assert(result == VK_SUCCESS);
    t_assert(cmd);
    t_cleanup_push_vk_cmd_buffer(dev, pool, cmd);

    return cmd;
}

VkResult
__qoBeginCommandBuffer(VkCommandBuffer cmd,
                       const VkCommandBufferBeginInfo *info)
{
    VkResult result;

    result = vkBeginCommandBuffer(cmd, info);
    t_assert(result == VK_SUCCESS);

    return result;
}

VkResult
__qoEndCommandBuffer(VkCommandBuffer cmd)
{
    VkResult result;

    result = vkEndCommandBuffer(cmd);
    t_assert(result == VK_SUCCESS);

    return result;
}

VkFramebuffer
__qoCreateFramebuffer(VkDevice dev, const VkFramebufferCreateInfo *info)
{
    VkFramebuffer fb = {0};
    VkResult result;

    result = vkCreateFramebuffer(dev, info, NULL, &fb);

    t_assert(result == VK_SUCCESS);
    t_assert(fb != VK_NULL_HANDLE);
    t_cleanup_push_vk_framebuffer(dev, fb);

    return fb;
}

VkRenderPass
__qoCreateRenderPass(VkDevice dev, const VkRenderPassCreateInfo *info)
{
    VkRenderPass pass = {0};
    VkResult result;

    result = vkCreateRenderPass(dev, info, NULL, &pass);

    t_assert(result == VK_SUCCESS);
    t_assert(pass != VK_NULL_HANDLE);
    t_cleanup_push_vk_render_pass(dev, pass);

    return pass;
}

VkResult __qoEndCommandBuffer(VkCommandBuffer cmd);

VkImage
__qoCreateImage(VkDevice dev, const VkImageCreateInfo *info)
{
    VkImage image = {0};
    VkResult result;

    result = vkCreateImage(dev, info, NULL, &image);

    t_assert(result == VK_SUCCESS);
    t_assert(image != VK_NULL_HANDLE);
    t_cleanup_push_vk_image(dev, image);

    return image;
}

VkImageView
__qoCreateImageView(VkDevice dev, const VkImageViewCreateInfo *info)
{
    VkImageView view = {0};
    VkResult result;

    result = vkCreateImageView(dev, info, NULL, &view);

    t_assert(result == VK_SUCCESS);
    t_assert(view != VK_NULL_HANDLE);
    t_cleanup_push_vk_image_view(dev, view);

    return view;
}

VkShaderModule
__qoCreateShaderModule(VkDevice dev, const QoShaderModuleCreateInfo *info)
{
    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result;

    VkShaderModuleCreateInfo module_info = {
        .sType =VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    };

    if (t_use_spir_v && info->spirvSize > 0) {
        assert(info->pSpirv != NULL);
        module_info.codeSize = info->spirvSize;
        module_info.pCode = info->pSpirv;
    } else if (info->glslSize > 0) {
        assert(info->pGlsl != NULL);
        module_info.codeSize = info->glslSize;
        module_info.pCode = (const uint32_t *)info->pGlsl;
    } else {
        assert(info->spirvSize > 0 && info->pSpirv != NULL);
        module_info.codeSize = info->spirvSize;
        module_info.pCode = info->pSpirv;
    }

    result = vkCreateShaderModule(dev, &module_info, NULL, &module);

    t_assert(result == VK_SUCCESS);
    t_assert(module != VK_NULL_HANDLE);
    t_cleanup_push_vk_shader_module(dev, module);

    return module;
}
