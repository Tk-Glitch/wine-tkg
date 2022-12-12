/* Automatically generated from Vulkan vk.xml; DO NOT EDIT!
 *
 * This file is generated from Vulkan vk.xml file covered
 * by the following copyright and permission notice:
 *
 * Copyright 2015-2022 The Khronos Group Inc.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "vulkan_loader.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

VkResult WINAPI vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR *pAcquireInfo, uint32_t *pImageIndex)
{
    struct vkAcquireNextImage2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pAcquireInfo = pAcquireInfo;
    params.pImageIndex = pImageIndex;
    status = UNIX_CALL(vkAcquireNextImage2KHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex)
{
    struct vkAcquireNextImageKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.timeout = timeout;
    params.semaphore = semaphore;
    params.fence = fence;
    params.pImageIndex = pImageIndex;
    status = UNIX_CALL(vkAcquireNextImageKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkAcquirePerformanceConfigurationINTEL(VkDevice device, const VkPerformanceConfigurationAcquireInfoINTEL *pAcquireInfo, VkPerformanceConfigurationINTEL *pConfiguration)
{
    struct vkAcquirePerformanceConfigurationINTEL_params params;
    NTSTATUS status;
    params.device = device;
    params.pAcquireInfo = pAcquireInfo;
    params.pConfiguration = pConfiguration;
    status = UNIX_CALL(vkAcquirePerformanceConfigurationINTEL, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkAcquireProfilingLockKHR(VkDevice device, const VkAcquireProfilingLockInfoKHR *pInfo)
{
    struct vkAcquireProfilingLockKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkAcquireProfilingLockKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo *pAllocateInfo, VkDescriptorSet *pDescriptorSets)
{
    struct vkAllocateDescriptorSets_params params;
    NTSTATUS status;
    params.device = device;
    params.pAllocateInfo = pAllocateInfo;
    params.pDescriptorSets = pDescriptorSets;
    status = UNIX_CALL(vkAllocateDescriptorSets, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo, const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory)
{
    struct vkAllocateMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.pAllocateInfo = pAllocateInfo;
    params.pAllocator = pAllocator;
    params.pMemory = pMemory;
    status = UNIX_CALL(vkAllocateMemory, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo)
{
    struct vkBeginCommandBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pBeginInfo = pBeginInfo;
    status = UNIX_CALL(vkBeginCommandBuffer, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBindAccelerationStructureMemoryNV(VkDevice device, uint32_t bindInfoCount, const VkBindAccelerationStructureMemoryInfoNV *pBindInfos)
{
    struct vkBindAccelerationStructureMemoryNV_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindAccelerationStructureMemoryNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    struct vkBindBufferMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.buffer = buffer;
    params.memory = memory;
    params.memoryOffset = memoryOffset;
    status = UNIX_CALL(vkBindBufferMemory, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos)
{
    struct vkBindBufferMemory2_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindBufferMemory2, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos)
{
    struct vkBindBufferMemory2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindBufferMemory2KHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    struct vkBindImageMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.memory = memory;
    params.memoryOffset = memoryOffset;
    status = UNIX_CALL(vkBindImageMemory, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos)
{
    struct vkBindImageMemory2_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindImageMemory2, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos)
{
    struct vkBindImageMemory2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindImageMemory2KHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBindOpticalFlowSessionImageNV(VkDevice device, VkOpticalFlowSessionNV session, VkOpticalFlowSessionBindingPointNV bindingPoint, VkImageView view, VkImageLayout layout)
{
    struct vkBindOpticalFlowSessionImageNV_params params;
    NTSTATUS status;
    params.device = device;
    params.session = session;
    params.bindingPoint = bindingPoint;
    params.view = view;
    params.layout = layout;
    status = UNIX_CALL(vkBindOpticalFlowSessionImageNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBuildAccelerationStructuresKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkAccelerationStructureBuildRangeInfoKHR * const*ppBuildRangeInfos)
{
    struct vkBuildAccelerationStructuresKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    params.ppBuildRangeInfos = ppBuildRangeInfos;
    status = UNIX_CALL(vkBuildAccelerationStructuresKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkBuildMicromapsEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkMicromapBuildInfoEXT *pInfos)
{
    struct vkBuildMicromapsEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    status = UNIX_CALL(vkBuildMicromapsEXT, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkCmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin)
{
    struct vkCmdBeginConditionalRenderingEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pConditionalRenderingBegin = pConditionalRenderingBegin;
    status = UNIX_CALL(vkCmdBeginConditionalRenderingEXT, &params);
    assert(!status);
}

void WINAPI vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT *pLabelInfo)
{
    struct vkCmdBeginDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pLabelInfo = pLabelInfo;
    status = UNIX_CALL(vkCmdBeginDebugUtilsLabelEXT, &params);
    assert(!status);
}

void WINAPI vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
{
    struct vkCmdBeginQuery_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.query = query;
    params.flags = flags;
    status = UNIX_CALL(vkCmdBeginQuery, &params);
    assert(!status);
}

void WINAPI vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index)
{
    struct vkCmdBeginQueryIndexedEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.query = query;
    params.flags = flags;
    params.index = index;
    status = UNIX_CALL(vkCmdBeginQueryIndexedEXT, &params);
    assert(!status);
}

void WINAPI vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents)
{
    struct vkCmdBeginRenderPass_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pRenderPassBegin = pRenderPassBegin;
    params.contents = contents;
    status = UNIX_CALL(vkCmdBeginRenderPass, &params);
    assert(!status);
}

void WINAPI vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, const VkSubpassBeginInfo *pSubpassBeginInfo)
{
    struct vkCmdBeginRenderPass2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pRenderPassBegin = pRenderPassBegin;
    params.pSubpassBeginInfo = pSubpassBeginInfo;
    status = UNIX_CALL(vkCmdBeginRenderPass2, &params);
    assert(!status);
}

void WINAPI vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, const VkSubpassBeginInfo *pSubpassBeginInfo)
{
    struct vkCmdBeginRenderPass2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pRenderPassBegin = pRenderPassBegin;
    params.pSubpassBeginInfo = pSubpassBeginInfo;
    status = UNIX_CALL(vkCmdBeginRenderPass2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo *pRenderingInfo)
{
    struct vkCmdBeginRendering_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pRenderingInfo = pRenderingInfo;
    status = UNIX_CALL(vkCmdBeginRendering, &params);
    assert(!status);
}

void WINAPI vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo *pRenderingInfo)
{
    struct vkCmdBeginRenderingKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pRenderingInfo = pRenderingInfo;
    status = UNIX_CALL(vkCmdBeginRenderingKHR, &params);
    assert(!status);
}

void WINAPI vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer *pCounterBuffers, const VkDeviceSize *pCounterBufferOffsets)
{
    struct vkCmdBeginTransformFeedbackEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstCounterBuffer = firstCounterBuffer;
    params.counterBufferCount = counterBufferCount;
    params.pCounterBuffers = pCounterBuffers;
    params.pCounterBufferOffsets = pCounterBufferOffsets;
    status = UNIX_CALL(vkCmdBeginTransformFeedbackEXT, &params);
    assert(!status);
}

void WINAPI vkCmdBindDescriptorBufferEmbeddedSamplersEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set)
{
    struct vkCmdBindDescriptorBufferEmbeddedSamplersEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.set = set;
    status = UNIX_CALL(vkCmdBindDescriptorBufferEmbeddedSamplersEXT, &params);
    assert(!status);
}

void WINAPI vkCmdBindDescriptorBuffersEXT(VkCommandBuffer commandBuffer, uint32_t bufferCount, const VkDescriptorBufferBindingInfoEXT *pBindingInfos)
{
    struct vkCmdBindDescriptorBuffersEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.bufferCount = bufferCount;
    params.pBindingInfos = pBindingInfos;
    status = UNIX_CALL(vkCmdBindDescriptorBuffersEXT, &params);
    assert(!status);
}

void WINAPI vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets)
{
    struct vkCmdBindDescriptorSets_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.firstSet = firstSet;
    params.descriptorSetCount = descriptorSetCount;
    params.pDescriptorSets = pDescriptorSets;
    params.dynamicOffsetCount = dynamicOffsetCount;
    params.pDynamicOffsets = pDynamicOffsets;
    status = UNIX_CALL(vkCmdBindDescriptorSets, &params);
    assert(!status);
}

void WINAPI vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
    struct vkCmdBindIndexBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.indexType = indexType;
    status = UNIX_CALL(vkCmdBindIndexBuffer, &params);
    assert(!status);
}

void WINAPI vkCmdBindInvocationMaskHUAWEI(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout)
{
    struct vkCmdBindInvocationMaskHUAWEI_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.imageView = imageView;
    params.imageLayout = imageLayout;
    status = UNIX_CALL(vkCmdBindInvocationMaskHUAWEI, &params);
    assert(!status);
}

void WINAPI vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
    struct vkCmdBindPipeline_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.pipeline = pipeline;
    status = UNIX_CALL(vkCmdBindPipeline, &params);
    assert(!status);
}

void WINAPI vkCmdBindPipelineShaderGroupNV(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline, uint32_t groupIndex)
{
    struct vkCmdBindPipelineShaderGroupNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.pipeline = pipeline;
    params.groupIndex = groupIndex;
    status = UNIX_CALL(vkCmdBindPipelineShaderGroupNV, &params);
    assert(!status);
}

void WINAPI vkCmdBindShadingRateImageNV(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout)
{
    struct vkCmdBindShadingRateImageNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.imageView = imageView;
    params.imageLayout = imageLayout;
    status = UNIX_CALL(vkCmdBindShadingRateImageNV, &params);
    assert(!status);
}

void WINAPI vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes)
{
    struct vkCmdBindTransformFeedbackBuffersEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstBinding = firstBinding;
    params.bindingCount = bindingCount;
    params.pBuffers = pBuffers;
    params.pOffsets = pOffsets;
    params.pSizes = pSizes;
    status = UNIX_CALL(vkCmdBindTransformFeedbackBuffersEXT, &params);
    assert(!status);
}

void WINAPI vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
{
    struct vkCmdBindVertexBuffers_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstBinding = firstBinding;
    params.bindingCount = bindingCount;
    params.pBuffers = pBuffers;
    params.pOffsets = pOffsets;
    status = UNIX_CALL(vkCmdBindVertexBuffers, &params);
    assert(!status);
}

void WINAPI vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes, const VkDeviceSize *pStrides)
{
    struct vkCmdBindVertexBuffers2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstBinding = firstBinding;
    params.bindingCount = bindingCount;
    params.pBuffers = pBuffers;
    params.pOffsets = pOffsets;
    params.pSizes = pSizes;
    params.pStrides = pStrides;
    status = UNIX_CALL(vkCmdBindVertexBuffers2, &params);
    assert(!status);
}

void WINAPI vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes, const VkDeviceSize *pStrides)
{
    struct vkCmdBindVertexBuffers2EXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstBinding = firstBinding;
    params.bindingCount = bindingCount;
    params.pBuffers = pBuffers;
    params.pOffsets = pOffsets;
    params.pSizes = pSizes;
    params.pStrides = pStrides;
    status = UNIX_CALL(vkCmdBindVertexBuffers2EXT, &params);
    assert(!status);
}

void WINAPI vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit *pRegions, VkFilter filter)
{
    struct vkCmdBlitImage_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.srcImage = srcImage;
    params.srcImageLayout = srcImageLayout;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    params.filter = filter;
    status = UNIX_CALL(vkCmdBlitImage, &params);
    assert(!status);
}

void WINAPI vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2 *pBlitImageInfo)
{
    struct vkCmdBlitImage2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pBlitImageInfo = pBlitImageInfo;
    status = UNIX_CALL(vkCmdBlitImage2, &params);
    assert(!status);
}

void WINAPI vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer, const VkBlitImageInfo2 *pBlitImageInfo)
{
    struct vkCmdBlitImage2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pBlitImageInfo = pBlitImageInfo;
    status = UNIX_CALL(vkCmdBlitImage2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdBuildAccelerationStructureNV(VkCommandBuffer commandBuffer, const VkAccelerationStructureInfoNV *pInfo, VkBuffer instanceData, VkDeviceSize instanceOffset, VkBool32 update, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkBuffer scratch, VkDeviceSize scratchOffset)
{
    struct vkCmdBuildAccelerationStructureNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    params.instanceData = instanceData;
    params.instanceOffset = instanceOffset;
    params.update = update;
    params.dst = dst;
    params.src = src;
    params.scratch = scratch;
    params.scratchOffset = scratchOffset;
    status = UNIX_CALL(vkCmdBuildAccelerationStructureNV, &params);
    assert(!status);
}

void WINAPI vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkDeviceAddress *pIndirectDeviceAddresses, const uint32_t *pIndirectStrides, const uint32_t * const*ppMaxPrimitiveCounts)
{
    struct vkCmdBuildAccelerationStructuresIndirectKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    params.pIndirectDeviceAddresses = pIndirectDeviceAddresses;
    params.pIndirectStrides = pIndirectStrides;
    params.ppMaxPrimitiveCounts = ppMaxPrimitiveCounts;
    status = UNIX_CALL(vkCmdBuildAccelerationStructuresIndirectKHR, &params);
    assert(!status);
}

void WINAPI vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkAccelerationStructureBuildRangeInfoKHR * const*ppBuildRangeInfos)
{
    struct vkCmdBuildAccelerationStructuresKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    params.ppBuildRangeInfos = ppBuildRangeInfos;
    status = UNIX_CALL(vkCmdBuildAccelerationStructuresKHR, &params);
    assert(!status);
}

void WINAPI vkCmdBuildMicromapsEXT(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkMicromapBuildInfoEXT *pInfos)
{
    struct vkCmdBuildMicromapsEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    status = UNIX_CALL(vkCmdBuildMicromapsEXT, &params);
    assert(!status);
}

void WINAPI vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment *pAttachments, uint32_t rectCount, const VkClearRect *pRects)
{
    struct vkCmdClearAttachments_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.attachmentCount = attachmentCount;
    params.pAttachments = pAttachments;
    params.rectCount = rectCount;
    params.pRects = pRects;
    status = UNIX_CALL(vkCmdClearAttachments, &params);
    assert(!status);
}

void WINAPI vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue *pColor, uint32_t rangeCount, const VkImageSubresourceRange *pRanges)
{
    struct vkCmdClearColorImage_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.image = image;
    params.imageLayout = imageLayout;
    params.pColor = pColor;
    params.rangeCount = rangeCount;
    params.pRanges = pRanges;
    status = UNIX_CALL(vkCmdClearColorImage, &params);
    assert(!status);
}

void WINAPI vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue *pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange *pRanges)
{
    struct vkCmdClearDepthStencilImage_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.image = image;
    params.imageLayout = imageLayout;
    params.pDepthStencil = pDepthStencil;
    params.rangeCount = rangeCount;
    params.pRanges = pRanges;
    status = UNIX_CALL(vkCmdClearDepthStencilImage, &params);
    assert(!status);
}

void WINAPI vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR *pInfo)
{
    struct vkCmdCopyAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCmdCopyAccelerationStructureKHR, &params);
    assert(!status);
}

void WINAPI vkCmdCopyAccelerationStructureNV(VkCommandBuffer commandBuffer, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkCopyAccelerationStructureModeKHR mode)
{
    struct vkCmdCopyAccelerationStructureNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.dst = dst;
    params.src = src;
    params.mode = mode;
    status = UNIX_CALL(vkCmdCopyAccelerationStructureNV, &params);
    assert(!status);
}

void WINAPI vkCmdCopyAccelerationStructureToMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
    struct vkCmdCopyAccelerationStructureToMemoryKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCmdCopyAccelerationStructureToMemoryKHR, &params);
    assert(!status);
}

void WINAPI vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy *pRegions)
{
    struct vkCmdCopyBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.srcBuffer = srcBuffer;
    params.dstBuffer = dstBuffer;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    status = UNIX_CALL(vkCmdCopyBuffer, &params);
    assert(!status);
}

void WINAPI vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2 *pCopyBufferInfo)
{
    struct vkCmdCopyBuffer2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCopyBufferInfo = pCopyBufferInfo;
    status = UNIX_CALL(vkCmdCopyBuffer2, &params);
    assert(!status);
}

void WINAPI vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2 *pCopyBufferInfo)
{
    struct vkCmdCopyBuffer2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCopyBufferInfo = pCopyBufferInfo;
    status = UNIX_CALL(vkCmdCopyBuffer2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
    struct vkCmdCopyBufferToImage_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.srcBuffer = srcBuffer;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    status = UNIX_CALL(vkCmdCopyBufferToImage, &params);
    assert(!status);
}

void WINAPI vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2 *pCopyBufferToImageInfo)
{
    struct vkCmdCopyBufferToImage2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCopyBufferToImageInfo = pCopyBufferToImageInfo;
    status = UNIX_CALL(vkCmdCopyBufferToImage2, &params);
    assert(!status);
}

void WINAPI vkCmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2 *pCopyBufferToImageInfo)
{
    struct vkCmdCopyBufferToImage2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCopyBufferToImageInfo = pCopyBufferToImageInfo;
    status = UNIX_CALL(vkCmdCopyBufferToImage2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy *pRegions)
{
    struct vkCmdCopyImage_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.srcImage = srcImage;
    params.srcImageLayout = srcImageLayout;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    status = UNIX_CALL(vkCmdCopyImage, &params);
    assert(!status);
}

void WINAPI vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2 *pCopyImageInfo)
{
    struct vkCmdCopyImage2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCopyImageInfo = pCopyImageInfo;
    status = UNIX_CALL(vkCmdCopyImage2, &params);
    assert(!status);
}

void WINAPI vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer, const VkCopyImageInfo2 *pCopyImageInfo)
{
    struct vkCmdCopyImage2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCopyImageInfo = pCopyImageInfo;
    status = UNIX_CALL(vkCmdCopyImage2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
    struct vkCmdCopyImageToBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.srcImage = srcImage;
    params.srcImageLayout = srcImageLayout;
    params.dstBuffer = dstBuffer;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    status = UNIX_CALL(vkCmdCopyImageToBuffer, &params);
    assert(!status);
}

void WINAPI vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2 *pCopyImageToBufferInfo)
{
    struct vkCmdCopyImageToBuffer2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCopyImageToBufferInfo = pCopyImageToBufferInfo;
    status = UNIX_CALL(vkCmdCopyImageToBuffer2, &params);
    assert(!status);
}

void WINAPI vkCmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2 *pCopyImageToBufferInfo)
{
    struct vkCmdCopyImageToBuffer2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCopyImageToBufferInfo = pCopyImageToBufferInfo;
    status = UNIX_CALL(vkCmdCopyImageToBuffer2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdCopyMemoryIndirectNV(VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride)
{
    struct vkCmdCopyMemoryIndirectNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.copyBufferAddress = copyBufferAddress;
    params.copyCount = copyCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdCopyMemoryIndirectNV, &params);
    assert(!status);
}

void WINAPI vkCmdCopyMemoryToAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
    struct vkCmdCopyMemoryToAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCmdCopyMemoryToAccelerationStructureKHR, &params);
    assert(!status);
}

void WINAPI vkCmdCopyMemoryToImageIndirectNV(VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride, VkImage dstImage, VkImageLayout dstImageLayout, const VkImageSubresourceLayers *pImageSubresources)
{
    struct vkCmdCopyMemoryToImageIndirectNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.copyBufferAddress = copyBufferAddress;
    params.copyCount = copyCount;
    params.stride = stride;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.pImageSubresources = pImageSubresources;
    status = UNIX_CALL(vkCmdCopyMemoryToImageIndirectNV, &params);
    assert(!status);
}

void WINAPI vkCmdCopyMemoryToMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMemoryToMicromapInfoEXT *pInfo)
{
    struct vkCmdCopyMemoryToMicromapEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCmdCopyMemoryToMicromapEXT, &params);
    assert(!status);
}

void WINAPI vkCmdCopyMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapInfoEXT *pInfo)
{
    struct vkCmdCopyMicromapEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCmdCopyMicromapEXT, &params);
    assert(!status);
}

void WINAPI vkCmdCopyMicromapToMemoryEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapToMemoryInfoEXT *pInfo)
{
    struct vkCmdCopyMicromapToMemoryEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCmdCopyMicromapToMemoryEXT, &params);
    assert(!status);
}

void WINAPI vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
    struct vkCmdCopyQueryPoolResults_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.stride = stride;
    params.flags = flags;
    status = UNIX_CALL(vkCmdCopyQueryPoolResults, &params);
    assert(!status);
}

void WINAPI vkCmdCuLaunchKernelNVX(VkCommandBuffer commandBuffer, const VkCuLaunchInfoNVX *pLaunchInfo)
{
    struct vkCmdCuLaunchKernelNVX_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pLaunchInfo = pLaunchInfo;
    status = UNIX_CALL(vkCmdCuLaunchKernelNVX, &params);
    assert(!status);
}

void WINAPI vkCmdDebugMarkerBeginEXT(VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT *pMarkerInfo)
{
    struct vkCmdDebugMarkerBeginEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pMarkerInfo = pMarkerInfo;
    status = UNIX_CALL(vkCmdDebugMarkerBeginEXT, &params);
    assert(!status);
}

void WINAPI vkCmdDebugMarkerEndEXT(VkCommandBuffer commandBuffer)
{
    struct vkCmdDebugMarkerEndEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkCmdDebugMarkerEndEXT, &params);
    assert(!status);
}

void WINAPI vkCmdDebugMarkerInsertEXT(VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT *pMarkerInfo)
{
    struct vkCmdDebugMarkerInsertEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pMarkerInfo = pMarkerInfo;
    status = UNIX_CALL(vkCmdDebugMarkerInsertEXT, &params);
    assert(!status);
}

void WINAPI vkCmdDecompressMemoryIndirectCountNV(VkCommandBuffer commandBuffer, VkDeviceAddress indirectCommandsAddress, VkDeviceAddress indirectCommandsCountAddress, uint32_t stride)
{
    struct vkCmdDecompressMemoryIndirectCountNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.indirectCommandsAddress = indirectCommandsAddress;
    params.indirectCommandsCountAddress = indirectCommandsCountAddress;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDecompressMemoryIndirectCountNV, &params);
    assert(!status);
}

void WINAPI vkCmdDecompressMemoryNV(VkCommandBuffer commandBuffer, uint32_t decompressRegionCount, const VkDecompressMemoryRegionNV *pDecompressMemoryRegions)
{
    struct vkCmdDecompressMemoryNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.decompressRegionCount = decompressRegionCount;
    params.pDecompressMemoryRegions = pDecompressMemoryRegions;
    status = UNIX_CALL(vkCmdDecompressMemoryNV, &params);
    assert(!status);
}

void WINAPI vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDispatch_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    status = UNIX_CALL(vkCmdDispatch, &params);
    assert(!status);
}

void WINAPI vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDispatchBase_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.baseGroupX = baseGroupX;
    params.baseGroupY = baseGroupY;
    params.baseGroupZ = baseGroupZ;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    status = UNIX_CALL(vkCmdDispatchBase, &params);
    assert(!status);
}

void WINAPI vkCmdDispatchBaseKHR(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDispatchBaseKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.baseGroupX = baseGroupX;
    params.baseGroupY = baseGroupY;
    params.baseGroupZ = baseGroupZ;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    status = UNIX_CALL(vkCmdDispatchBaseKHR, &params);
    assert(!status);
}

void WINAPI vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset)
{
    struct vkCmdDispatchIndirect_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    status = UNIX_CALL(vkCmdDispatchIndirect, &params);
    assert(!status);
}

void WINAPI vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    struct vkCmdDraw_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.vertexCount = vertexCount;
    params.instanceCount = instanceCount;
    params.firstVertex = firstVertex;
    params.firstInstance = firstInstance;
    status = UNIX_CALL(vkCmdDraw, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    struct vkCmdDrawIndexed_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.indexCount = indexCount;
    params.instanceCount = instanceCount;
    params.firstIndex = firstIndex;
    params.vertexOffset = vertexOffset;
    params.firstInstance = firstInstance;
    status = UNIX_CALL(vkCmdDrawIndexed, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    struct vkCmdDrawIndexedIndirect_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.drawCount = drawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawIndexedIndirect, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndexedIndirectCount_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawIndexedIndirectCount, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndexedIndirectCountAMD(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndexedIndirectCountAMD_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawIndexedIndirectCountAMD, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndexedIndirectCountKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawIndexedIndirectCountKHR, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    struct vkCmdDrawIndirect_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.drawCount = drawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawIndirect, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride)
{
    struct vkCmdDrawIndirectByteCountEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.instanceCount = instanceCount;
    params.firstInstance = firstInstance;
    params.counterBuffer = counterBuffer;
    params.counterBufferOffset = counterBufferOffset;
    params.counterOffset = counterOffset;
    params.vertexStride = vertexStride;
    status = UNIX_CALL(vkCmdDrawIndirectByteCountEXT, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndirectCount_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawIndirectCount, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndirectCountAMD(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndirectCountAMD_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawIndirectCountAMD, &params);
    assert(!status);
}

void WINAPI vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndirectCountKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawIndirectCountKHR, &params);
    assert(!status);
}

void WINAPI vkCmdDrawMeshTasksEXT(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDrawMeshTasksEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    status = UNIX_CALL(vkCmdDrawMeshTasksEXT, &params);
    assert(!status);
}

void WINAPI vkCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawMeshTasksIndirectCountEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawMeshTasksIndirectCountEXT, &params);
    assert(!status);
}

void WINAPI vkCmdDrawMeshTasksIndirectCountNV(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawMeshTasksIndirectCountNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawMeshTasksIndirectCountNV, &params);
    assert(!status);
}

void WINAPI vkCmdDrawMeshTasksIndirectEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    struct vkCmdDrawMeshTasksIndirectEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.drawCount = drawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawMeshTasksIndirectEXT, &params);
    assert(!status);
}

void WINAPI vkCmdDrawMeshTasksIndirectNV(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    struct vkCmdDrawMeshTasksIndirectNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.drawCount = drawCount;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawMeshTasksIndirectNV, &params);
    assert(!status);
}

void WINAPI vkCmdDrawMeshTasksNV(VkCommandBuffer commandBuffer, uint32_t taskCount, uint32_t firstTask)
{
    struct vkCmdDrawMeshTasksNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.taskCount = taskCount;
    params.firstTask = firstTask;
    status = UNIX_CALL(vkCmdDrawMeshTasksNV, &params);
    assert(!status);
}

void WINAPI vkCmdDrawMultiEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawInfoEXT *pVertexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride)
{
    struct vkCmdDrawMultiEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.drawCount = drawCount;
    params.pVertexInfo = pVertexInfo;
    params.instanceCount = instanceCount;
    params.firstInstance = firstInstance;
    params.stride = stride;
    status = UNIX_CALL(vkCmdDrawMultiEXT, &params);
    assert(!status);
}

void WINAPI vkCmdDrawMultiIndexedEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawIndexedInfoEXT *pIndexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride, const int32_t *pVertexOffset)
{
    struct vkCmdDrawMultiIndexedEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.drawCount = drawCount;
    params.pIndexInfo = pIndexInfo;
    params.instanceCount = instanceCount;
    params.firstInstance = firstInstance;
    params.stride = stride;
    params.pVertexOffset = pVertexOffset;
    status = UNIX_CALL(vkCmdDrawMultiIndexedEXT, &params);
    assert(!status);
}

void WINAPI vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndConditionalRenderingEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkCmdEndConditionalRenderingEXT, &params);
    assert(!status);
}

void WINAPI vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkCmdEndDebugUtilsLabelEXT, &params);
    assert(!status);
}

void WINAPI vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query)
{
    struct vkCmdEndQuery_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.query = query;
    status = UNIX_CALL(vkCmdEndQuery, &params);
    assert(!status);
}

void WINAPI vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index)
{
    struct vkCmdEndQueryIndexedEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.query = query;
    params.index = index;
    status = UNIX_CALL(vkCmdEndQueryIndexedEXT, &params);
    assert(!status);
}

void WINAPI vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndRenderPass_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkCmdEndRenderPass, &params);
    assert(!status);
}

void WINAPI vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo *pSubpassEndInfo)
{
    struct vkCmdEndRenderPass2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pSubpassEndInfo = pSubpassEndInfo;
    status = UNIX_CALL(vkCmdEndRenderPass2, &params);
    assert(!status);
}

void WINAPI vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer, const VkSubpassEndInfo *pSubpassEndInfo)
{
    struct vkCmdEndRenderPass2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pSubpassEndInfo = pSubpassEndInfo;
    status = UNIX_CALL(vkCmdEndRenderPass2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdEndRendering(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndRendering_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkCmdEndRendering, &params);
    assert(!status);
}

void WINAPI vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndRenderingKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkCmdEndRenderingKHR, &params);
    assert(!status);
}

void WINAPI vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer *pCounterBuffers, const VkDeviceSize *pCounterBufferOffsets)
{
    struct vkCmdEndTransformFeedbackEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstCounterBuffer = firstCounterBuffer;
    params.counterBufferCount = counterBufferCount;
    params.pCounterBuffers = pCounterBuffers;
    params.pCounterBufferOffsets = pCounterBufferOffsets;
    status = UNIX_CALL(vkCmdEndTransformFeedbackEXT, &params);
    assert(!status);
}

void WINAPI vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
    struct vkCmdExecuteCommands_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.commandBufferCount = commandBufferCount;
    params.pCommandBuffers = pCommandBuffers;
    status = UNIX_CALL(vkCmdExecuteCommands, &params);
    assert(!status);
}

void WINAPI vkCmdExecuteGeneratedCommandsNV(VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoNV *pGeneratedCommandsInfo)
{
    struct vkCmdExecuteGeneratedCommandsNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.isPreprocessed = isPreprocessed;
    params.pGeneratedCommandsInfo = pGeneratedCommandsInfo;
    status = UNIX_CALL(vkCmdExecuteGeneratedCommandsNV, &params);
    assert(!status);
}

void WINAPI vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data)
{
    struct vkCmdFillBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.size = size;
    params.data = data;
    status = UNIX_CALL(vkCmdFillBuffer, &params);
    assert(!status);
}

void WINAPI vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT *pLabelInfo)
{
    struct vkCmdInsertDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pLabelInfo = pLabelInfo;
    status = UNIX_CALL(vkCmdInsertDebugUtilsLabelEXT, &params);
    assert(!status);
}

void WINAPI vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
    struct vkCmdNextSubpass_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.contents = contents;
    status = UNIX_CALL(vkCmdNextSubpass, &params);
    assert(!status);
}

void WINAPI vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo *pSubpassBeginInfo, const VkSubpassEndInfo *pSubpassEndInfo)
{
    struct vkCmdNextSubpass2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pSubpassBeginInfo = pSubpassBeginInfo;
    params.pSubpassEndInfo = pSubpassEndInfo;
    status = UNIX_CALL(vkCmdNextSubpass2, &params);
    assert(!status);
}

void WINAPI vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo *pSubpassBeginInfo, const VkSubpassEndInfo *pSubpassEndInfo)
{
    struct vkCmdNextSubpass2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pSubpassBeginInfo = pSubpassBeginInfo;
    params.pSubpassEndInfo = pSubpassEndInfo;
    status = UNIX_CALL(vkCmdNextSubpass2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdOpticalFlowExecuteNV(VkCommandBuffer commandBuffer, VkOpticalFlowSessionNV session, const VkOpticalFlowExecuteInfoNV *pExecuteInfo)
{
    struct vkCmdOpticalFlowExecuteNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.session = session;
    params.pExecuteInfo = pExecuteInfo;
    status = UNIX_CALL(vkCmdOpticalFlowExecuteNV, &params);
    assert(!status);
}

void WINAPI vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
    struct vkCmdPipelineBarrier_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.srcStageMask = srcStageMask;
    params.dstStageMask = dstStageMask;
    params.dependencyFlags = dependencyFlags;
    params.memoryBarrierCount = memoryBarrierCount;
    params.pMemoryBarriers = pMemoryBarriers;
    params.bufferMemoryBarrierCount = bufferMemoryBarrierCount;
    params.pBufferMemoryBarriers = pBufferMemoryBarriers;
    params.imageMemoryBarrierCount = imageMemoryBarrierCount;
    params.pImageMemoryBarriers = pImageMemoryBarriers;
    status = UNIX_CALL(vkCmdPipelineBarrier, &params);
    assert(!status);
}

void WINAPI vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo *pDependencyInfo)
{
    struct vkCmdPipelineBarrier2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pDependencyInfo = pDependencyInfo;
    status = UNIX_CALL(vkCmdPipelineBarrier2, &params);
    assert(!status);
}

void WINAPI vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo *pDependencyInfo)
{
    struct vkCmdPipelineBarrier2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pDependencyInfo = pDependencyInfo;
    status = UNIX_CALL(vkCmdPipelineBarrier2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdPreprocessGeneratedCommandsNV(VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoNV *pGeneratedCommandsInfo)
{
    struct vkCmdPreprocessGeneratedCommandsNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pGeneratedCommandsInfo = pGeneratedCommandsInfo;
    status = UNIX_CALL(vkCmdPreprocessGeneratedCommandsNV, &params);
    assert(!status);
}

void WINAPI vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *pValues)
{
    struct vkCmdPushConstants_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.layout = layout;
    params.stageFlags = stageFlags;
    params.offset = offset;
    params.size = size;
    params.pValues = pValues;
    status = UNIX_CALL(vkCmdPushConstants, &params);
    assert(!status);
}

void WINAPI vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites)
{
    struct vkCmdPushDescriptorSetKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.set = set;
    params.descriptorWriteCount = descriptorWriteCount;
    params.pDescriptorWrites = pDescriptorWrites;
    status = UNIX_CALL(vkCmdPushDescriptorSetKHR, &params);
    assert(!status);
}

void WINAPI vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void *pData)
{
    struct vkCmdPushDescriptorSetWithTemplateKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.layout = layout;
    params.set = set;
    params.pData = pData;
    status = UNIX_CALL(vkCmdPushDescriptorSetWithTemplateKHR, &params);
    assert(!status);
}

void WINAPI vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    struct vkCmdResetEvent_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.stageMask = stageMask;
    status = UNIX_CALL(vkCmdResetEvent, &params);
    assert(!status);
}

void WINAPI vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask)
{
    struct vkCmdResetEvent2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.stageMask = stageMask;
    status = UNIX_CALL(vkCmdResetEvent2, &params);
    assert(!status);
}

void WINAPI vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask)
{
    struct vkCmdResetEvent2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.stageMask = stageMask;
    status = UNIX_CALL(vkCmdResetEvent2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    struct vkCmdResetQueryPool_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    status = UNIX_CALL(vkCmdResetQueryPool, &params);
    assert(!status);
}

void WINAPI vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve *pRegions)
{
    struct vkCmdResolveImage_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.srcImage = srcImage;
    params.srcImageLayout = srcImageLayout;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    status = UNIX_CALL(vkCmdResolveImage, &params);
    assert(!status);
}

void WINAPI vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2 *pResolveImageInfo)
{
    struct vkCmdResolveImage2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pResolveImageInfo = pResolveImageInfo;
    status = UNIX_CALL(vkCmdResolveImage2, &params);
    assert(!status);
}

void WINAPI vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer, const VkResolveImageInfo2 *pResolveImageInfo)
{
    struct vkCmdResolveImage2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pResolveImageInfo = pResolveImageInfo;
    status = UNIX_CALL(vkCmdResolveImage2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable)
{
    struct vkCmdSetAlphaToCoverageEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.alphaToCoverageEnable = alphaToCoverageEnable;
    status = UNIX_CALL(vkCmdSetAlphaToCoverageEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable)
{
    struct vkCmdSetAlphaToOneEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.alphaToOneEnable = alphaToOneEnable;
    status = UNIX_CALL(vkCmdSetAlphaToOneEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4])
{
    struct vkCmdSetBlendConstants_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.blendConstants = blendConstants;
    status = UNIX_CALL(vkCmdSetBlendConstants, &params);
    assert(!status);
}

void WINAPI vkCmdSetCheckpointNV(VkCommandBuffer commandBuffer, const void *pCheckpointMarker)
{
    struct vkCmdSetCheckpointNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pCheckpointMarker = pCheckpointMarker;
    status = UNIX_CALL(vkCmdSetCheckpointNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetCoarseSampleOrderNV(VkCommandBuffer commandBuffer, VkCoarseSampleOrderTypeNV sampleOrderType, uint32_t customSampleOrderCount, const VkCoarseSampleOrderCustomNV *pCustomSampleOrders)
{
    struct vkCmdSetCoarseSampleOrderNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.sampleOrderType = sampleOrderType;
    params.customSampleOrderCount = customSampleOrderCount;
    params.pCustomSampleOrders = pCustomSampleOrders;
    status = UNIX_CALL(vkCmdSetCoarseSampleOrderNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT *pColorBlendAdvanced)
{
    struct vkCmdSetColorBlendAdvancedEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstAttachment = firstAttachment;
    params.attachmentCount = attachmentCount;
    params.pColorBlendAdvanced = pColorBlendAdvanced;
    status = UNIX_CALL(vkCmdSetColorBlendAdvancedEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32 *pColorBlendEnables)
{
    struct vkCmdSetColorBlendEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstAttachment = firstAttachment;
    params.attachmentCount = attachmentCount;
    params.pColorBlendEnables = pColorBlendEnables;
    status = UNIX_CALL(vkCmdSetColorBlendEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT *pColorBlendEquations)
{
    struct vkCmdSetColorBlendEquationEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstAttachment = firstAttachment;
    params.attachmentCount = attachmentCount;
    params.pColorBlendEquations = pColorBlendEquations;
    status = UNIX_CALL(vkCmdSetColorBlendEquationEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32 *pColorWriteEnables)
{
    struct vkCmdSetColorWriteEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.attachmentCount = attachmentCount;
    params.pColorWriteEnables = pColorWriteEnables;
    status = UNIX_CALL(vkCmdSetColorWriteEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags *pColorWriteMasks)
{
    struct vkCmdSetColorWriteMaskEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstAttachment = firstAttachment;
    params.attachmentCount = attachmentCount;
    params.pColorWriteMasks = pColorWriteMasks;
    status = UNIX_CALL(vkCmdSetColorWriteMaskEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode)
{
    struct vkCmdSetConservativeRasterizationModeEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.conservativeRasterizationMode = conservativeRasterizationMode;
    status = UNIX_CALL(vkCmdSetConservativeRasterizationModeEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode)
{
    struct vkCmdSetCoverageModulationModeNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.coverageModulationMode = coverageModulationMode;
    status = UNIX_CALL(vkCmdSetCoverageModulationModeNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable)
{
    struct vkCmdSetCoverageModulationTableEnableNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.coverageModulationTableEnable = coverageModulationTableEnable;
    status = UNIX_CALL(vkCmdSetCoverageModulationTableEnableNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float *pCoverageModulationTable)
{
    struct vkCmdSetCoverageModulationTableNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.coverageModulationTableCount = coverageModulationTableCount;
    params.pCoverageModulationTable = pCoverageModulationTable;
    status = UNIX_CALL(vkCmdSetCoverageModulationTableNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode)
{
    struct vkCmdSetCoverageReductionModeNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.coverageReductionMode = coverageReductionMode;
    status = UNIX_CALL(vkCmdSetCoverageReductionModeNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable)
{
    struct vkCmdSetCoverageToColorEnableNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.coverageToColorEnable = coverageToColorEnable;
    status = UNIX_CALL(vkCmdSetCoverageToColorEnableNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation)
{
    struct vkCmdSetCoverageToColorLocationNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.coverageToColorLocation = coverageToColorLocation;
    status = UNIX_CALL(vkCmdSetCoverageToColorLocationNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode)
{
    struct vkCmdSetCullMode_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.cullMode = cullMode;
    status = UNIX_CALL(vkCmdSetCullMode, &params);
    assert(!status);
}

void WINAPI vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode)
{
    struct vkCmdSetCullModeEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.cullMode = cullMode;
    status = UNIX_CALL(vkCmdSetCullModeEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
    struct vkCmdSetDepthBias_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthBiasConstantFactor = depthBiasConstantFactor;
    params.depthBiasClamp = depthBiasClamp;
    params.depthBiasSlopeFactor = depthBiasSlopeFactor;
    status = UNIX_CALL(vkCmdSetDepthBias, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable)
{
    struct vkCmdSetDepthBiasEnable_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthBiasEnable = depthBiasEnable;
    status = UNIX_CALL(vkCmdSetDepthBiasEnable, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable)
{
    struct vkCmdSetDepthBiasEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthBiasEnable = depthBiasEnable;
    status = UNIX_CALL(vkCmdSetDepthBiasEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds)
{
    struct vkCmdSetDepthBounds_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.minDepthBounds = minDepthBounds;
    params.maxDepthBounds = maxDepthBounds;
    status = UNIX_CALL(vkCmdSetDepthBounds, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable)
{
    struct vkCmdSetDepthBoundsTestEnable_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthBoundsTestEnable = depthBoundsTestEnable;
    status = UNIX_CALL(vkCmdSetDepthBoundsTestEnable, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable)
{
    struct vkCmdSetDepthBoundsTestEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthBoundsTestEnable = depthBoundsTestEnable;
    status = UNIX_CALL(vkCmdSetDepthBoundsTestEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable)
{
    struct vkCmdSetDepthClampEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthClampEnable = depthClampEnable;
    status = UNIX_CALL(vkCmdSetDepthClampEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable)
{
    struct vkCmdSetDepthClipEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthClipEnable = depthClipEnable;
    status = UNIX_CALL(vkCmdSetDepthClipEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne)
{
    struct vkCmdSetDepthClipNegativeOneToOneEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.negativeOneToOne = negativeOneToOne;
    status = UNIX_CALL(vkCmdSetDepthClipNegativeOneToOneEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp)
{
    struct vkCmdSetDepthCompareOp_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthCompareOp = depthCompareOp;
    status = UNIX_CALL(vkCmdSetDepthCompareOp, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp)
{
    struct vkCmdSetDepthCompareOpEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthCompareOp = depthCompareOp;
    status = UNIX_CALL(vkCmdSetDepthCompareOpEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable)
{
    struct vkCmdSetDepthTestEnable_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthTestEnable = depthTestEnable;
    status = UNIX_CALL(vkCmdSetDepthTestEnable, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable)
{
    struct vkCmdSetDepthTestEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthTestEnable = depthTestEnable;
    status = UNIX_CALL(vkCmdSetDepthTestEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable)
{
    struct vkCmdSetDepthWriteEnable_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthWriteEnable = depthWriteEnable;
    status = UNIX_CALL(vkCmdSetDepthWriteEnable, &params);
    assert(!status);
}

void WINAPI vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable)
{
    struct vkCmdSetDepthWriteEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.depthWriteEnable = depthWriteEnable;
    status = UNIX_CALL(vkCmdSetDepthWriteEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const uint32_t *pBufferIndices, const VkDeviceSize *pOffsets)
{
    struct vkCmdSetDescriptorBufferOffsetsEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.firstSet = firstSet;
    params.setCount = setCount;
    params.pBufferIndices = pBufferIndices;
    params.pOffsets = pOffsets;
    status = UNIX_CALL(vkCmdSetDescriptorBufferOffsetsEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask)
{
    struct vkCmdSetDeviceMask_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.deviceMask = deviceMask;
    status = UNIX_CALL(vkCmdSetDeviceMask, &params);
    assert(!status);
}

void WINAPI vkCmdSetDeviceMaskKHR(VkCommandBuffer commandBuffer, uint32_t deviceMask)
{
    struct vkCmdSetDeviceMaskKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.deviceMask = deviceMask;
    status = UNIX_CALL(vkCmdSetDeviceMaskKHR, &params);
    assert(!status);
}

void WINAPI vkCmdSetDiscardRectangleEXT(VkCommandBuffer commandBuffer, uint32_t firstDiscardRectangle, uint32_t discardRectangleCount, const VkRect2D *pDiscardRectangles)
{
    struct vkCmdSetDiscardRectangleEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstDiscardRectangle = firstDiscardRectangle;
    params.discardRectangleCount = discardRectangleCount;
    params.pDiscardRectangles = pDiscardRectangles;
    status = UNIX_CALL(vkCmdSetDiscardRectangleEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    struct vkCmdSetEvent_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.stageMask = stageMask;
    status = UNIX_CALL(vkCmdSetEvent, &params);
    assert(!status);
}

void WINAPI vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo *pDependencyInfo)
{
    struct vkCmdSetEvent2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.pDependencyInfo = pDependencyInfo;
    status = UNIX_CALL(vkCmdSetEvent2, &params);
    assert(!status);
}

void WINAPI vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo *pDependencyInfo)
{
    struct vkCmdSetEvent2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.pDependencyInfo = pDependencyInfo;
    status = UNIX_CALL(vkCmdSetEvent2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdSetExclusiveScissorNV(VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkRect2D *pExclusiveScissors)
{
    struct vkCmdSetExclusiveScissorNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstExclusiveScissor = firstExclusiveScissor;
    params.exclusiveScissorCount = exclusiveScissorCount;
    params.pExclusiveScissors = pExclusiveScissors;
    status = UNIX_CALL(vkCmdSetExclusiveScissorNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize)
{
    struct vkCmdSetExtraPrimitiveOverestimationSizeEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.extraPrimitiveOverestimationSize = extraPrimitiveOverestimationSize;
    status = UNIX_CALL(vkCmdSetExtraPrimitiveOverestimationSizeEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetFragmentShadingRateEnumNV(VkCommandBuffer commandBuffer, VkFragmentShadingRateNV shadingRate, const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
    struct vkCmdSetFragmentShadingRateEnumNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.shadingRate = shadingRate;
    params.combinerOps = combinerOps;
    status = UNIX_CALL(vkCmdSetFragmentShadingRateEnumNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetFragmentShadingRateKHR(VkCommandBuffer commandBuffer, const VkExtent2D *pFragmentSize, const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
    struct vkCmdSetFragmentShadingRateKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pFragmentSize = pFragmentSize;
    params.combinerOps = combinerOps;
    status = UNIX_CALL(vkCmdSetFragmentShadingRateKHR, &params);
    assert(!status);
}

void WINAPI vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace)
{
    struct vkCmdSetFrontFace_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.frontFace = frontFace;
    status = UNIX_CALL(vkCmdSetFrontFace, &params);
    assert(!status);
}

void WINAPI vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace)
{
    struct vkCmdSetFrontFaceEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.frontFace = frontFace;
    status = UNIX_CALL(vkCmdSetFrontFaceEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode)
{
    struct vkCmdSetLineRasterizationModeEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.lineRasterizationMode = lineRasterizationMode;
    status = UNIX_CALL(vkCmdSetLineRasterizationModeEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern)
{
    struct vkCmdSetLineStippleEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.lineStippleFactor = lineStippleFactor;
    params.lineStipplePattern = lineStipplePattern;
    status = UNIX_CALL(vkCmdSetLineStippleEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable)
{
    struct vkCmdSetLineStippleEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.stippledLineEnable = stippledLineEnable;
    status = UNIX_CALL(vkCmdSetLineStippleEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth)
{
    struct vkCmdSetLineWidth_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.lineWidth = lineWidth;
    status = UNIX_CALL(vkCmdSetLineWidth, &params);
    assert(!status);
}

void WINAPI vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp)
{
    struct vkCmdSetLogicOpEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.logicOp = logicOp;
    status = UNIX_CALL(vkCmdSetLogicOpEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable)
{
    struct vkCmdSetLogicOpEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.logicOpEnable = logicOpEnable;
    status = UNIX_CALL(vkCmdSetLogicOpEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints)
{
    struct vkCmdSetPatchControlPointsEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.patchControlPoints = patchControlPoints;
    status = UNIX_CALL(vkCmdSetPatchControlPointsEXT, &params);
    assert(!status);
}

VkResult WINAPI vkCmdSetPerformanceMarkerINTEL(VkCommandBuffer commandBuffer, const VkPerformanceMarkerInfoINTEL *pMarkerInfo)
{
    struct vkCmdSetPerformanceMarkerINTEL_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pMarkerInfo = pMarkerInfo;
    status = UNIX_CALL(vkCmdSetPerformanceMarkerINTEL, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCmdSetPerformanceOverrideINTEL(VkCommandBuffer commandBuffer, const VkPerformanceOverrideInfoINTEL *pOverrideInfo)
{
    struct vkCmdSetPerformanceOverrideINTEL_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pOverrideInfo = pOverrideInfo;
    status = UNIX_CALL(vkCmdSetPerformanceOverrideINTEL, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCmdSetPerformanceStreamMarkerINTEL(VkCommandBuffer commandBuffer, const VkPerformanceStreamMarkerInfoINTEL *pMarkerInfo)
{
    struct vkCmdSetPerformanceStreamMarkerINTEL_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pMarkerInfo = pMarkerInfo;
    status = UNIX_CALL(vkCmdSetPerformanceStreamMarkerINTEL, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode)
{
    struct vkCmdSetPolygonModeEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.polygonMode = polygonMode;
    status = UNIX_CALL(vkCmdSetPolygonModeEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable)
{
    struct vkCmdSetPrimitiveRestartEnable_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.primitiveRestartEnable = primitiveRestartEnable;
    status = UNIX_CALL(vkCmdSetPrimitiveRestartEnable, &params);
    assert(!status);
}

void WINAPI vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable)
{
    struct vkCmdSetPrimitiveRestartEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.primitiveRestartEnable = primitiveRestartEnable;
    status = UNIX_CALL(vkCmdSetPrimitiveRestartEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology)
{
    struct vkCmdSetPrimitiveTopology_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.primitiveTopology = primitiveTopology;
    status = UNIX_CALL(vkCmdSetPrimitiveTopology, &params);
    assert(!status);
}

void WINAPI vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology)
{
    struct vkCmdSetPrimitiveTopologyEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.primitiveTopology = primitiveTopology;
    status = UNIX_CALL(vkCmdSetPrimitiveTopologyEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode)
{
    struct vkCmdSetProvokingVertexModeEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.provokingVertexMode = provokingVertexMode;
    status = UNIX_CALL(vkCmdSetProvokingVertexModeEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples)
{
    struct vkCmdSetRasterizationSamplesEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.rasterizationSamples = rasterizationSamples;
    status = UNIX_CALL(vkCmdSetRasterizationSamplesEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream)
{
    struct vkCmdSetRasterizationStreamEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.rasterizationStream = rasterizationStream;
    status = UNIX_CALL(vkCmdSetRasterizationStreamEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable)
{
    struct vkCmdSetRasterizerDiscardEnable_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.rasterizerDiscardEnable = rasterizerDiscardEnable;
    status = UNIX_CALL(vkCmdSetRasterizerDiscardEnable, &params);
    assert(!status);
}

void WINAPI vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable)
{
    struct vkCmdSetRasterizerDiscardEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.rasterizerDiscardEnable = rasterizerDiscardEnable;
    status = UNIX_CALL(vkCmdSetRasterizerDiscardEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer commandBuffer, uint32_t pipelineStackSize)
{
    struct vkCmdSetRayTracingPipelineStackSizeKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineStackSize = pipelineStackSize;
    status = UNIX_CALL(vkCmdSetRayTracingPipelineStackSizeKHR, &params);
    assert(!status);
}

void WINAPI vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable)
{
    struct vkCmdSetRepresentativeFragmentTestEnableNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.representativeFragmentTestEnable = representativeFragmentTestEnable;
    status = UNIX_CALL(vkCmdSetRepresentativeFragmentTestEnableNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT *pSampleLocationsInfo)
{
    struct vkCmdSetSampleLocationsEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pSampleLocationsInfo = pSampleLocationsInfo;
    status = UNIX_CALL(vkCmdSetSampleLocationsEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable)
{
    struct vkCmdSetSampleLocationsEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.sampleLocationsEnable = sampleLocationsEnable;
    status = UNIX_CALL(vkCmdSetSampleLocationsEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask *pSampleMask)
{
    struct vkCmdSetSampleMaskEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.samples = samples;
    params.pSampleMask = pSampleMask;
    status = UNIX_CALL(vkCmdSetSampleMaskEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D *pScissors)
{
    struct vkCmdSetScissor_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstScissor = firstScissor;
    params.scissorCount = scissorCount;
    params.pScissors = pScissors;
    status = UNIX_CALL(vkCmdSetScissor, &params);
    assert(!status);
}

void WINAPI vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D *pScissors)
{
    struct vkCmdSetScissorWithCount_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.scissorCount = scissorCount;
    params.pScissors = pScissors;
    status = UNIX_CALL(vkCmdSetScissorWithCount, &params);
    assert(!status);
}

void WINAPI vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D *pScissors)
{
    struct vkCmdSetScissorWithCountEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.scissorCount = scissorCount;
    params.pScissors = pScissors;
    status = UNIX_CALL(vkCmdSetScissorWithCountEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable)
{
    struct vkCmdSetShadingRateImageEnableNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.shadingRateImageEnable = shadingRateImageEnable;
    status = UNIX_CALL(vkCmdSetShadingRateImageEnableNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask)
{
    struct vkCmdSetStencilCompareMask_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.compareMask = compareMask;
    status = UNIX_CALL(vkCmdSetStencilCompareMask, &params);
    assert(!status);
}

void WINAPI vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp)
{
    struct vkCmdSetStencilOp_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.failOp = failOp;
    params.passOp = passOp;
    params.depthFailOp = depthFailOp;
    params.compareOp = compareOp;
    status = UNIX_CALL(vkCmdSetStencilOp, &params);
    assert(!status);
}

void WINAPI vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp)
{
    struct vkCmdSetStencilOpEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.failOp = failOp;
    params.passOp = passOp;
    params.depthFailOp = depthFailOp;
    params.compareOp = compareOp;
    status = UNIX_CALL(vkCmdSetStencilOpEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference)
{
    struct vkCmdSetStencilReference_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.reference = reference;
    status = UNIX_CALL(vkCmdSetStencilReference, &params);
    assert(!status);
}

void WINAPI vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable)
{
    struct vkCmdSetStencilTestEnable_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.stencilTestEnable = stencilTestEnable;
    status = UNIX_CALL(vkCmdSetStencilTestEnable, &params);
    assert(!status);
}

void WINAPI vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable)
{
    struct vkCmdSetStencilTestEnableEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.stencilTestEnable = stencilTestEnable;
    status = UNIX_CALL(vkCmdSetStencilTestEnableEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask)
{
    struct vkCmdSetStencilWriteMask_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.writeMask = writeMask;
    status = UNIX_CALL(vkCmdSetStencilWriteMask, &params);
    assert(!status);
}

void WINAPI vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin)
{
    struct vkCmdSetTessellationDomainOriginEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.domainOrigin = domainOrigin;
    status = UNIX_CALL(vkCmdSetTessellationDomainOriginEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
{
    struct vkCmdSetVertexInputEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.vertexBindingDescriptionCount = vertexBindingDescriptionCount;
    params.pVertexBindingDescriptions = pVertexBindingDescriptions;
    params.vertexAttributeDescriptionCount = vertexAttributeDescriptionCount;
    params.pVertexAttributeDescriptions = pVertexAttributeDescriptions;
    status = UNIX_CALL(vkCmdSetVertexInputEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport *pViewports)
{
    struct vkCmdSetViewport_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstViewport = firstViewport;
    params.viewportCount = viewportCount;
    params.pViewports = pViewports;
    status = UNIX_CALL(vkCmdSetViewport, &params);
    assert(!status);
}

void WINAPI vkCmdSetViewportShadingRatePaletteNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkShadingRatePaletteNV *pShadingRatePalettes)
{
    struct vkCmdSetViewportShadingRatePaletteNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstViewport = firstViewport;
    params.viewportCount = viewportCount;
    params.pShadingRatePalettes = pShadingRatePalettes;
    status = UNIX_CALL(vkCmdSetViewportShadingRatePaletteNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV *pViewportSwizzles)
{
    struct vkCmdSetViewportSwizzleNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstViewport = firstViewport;
    params.viewportCount = viewportCount;
    params.pViewportSwizzles = pViewportSwizzles;
    status = UNIX_CALL(vkCmdSetViewportSwizzleNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable)
{
    struct vkCmdSetViewportWScalingEnableNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.viewportWScalingEnable = viewportWScalingEnable;
    status = UNIX_CALL(vkCmdSetViewportWScalingEnableNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetViewportWScalingNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportWScalingNV *pViewportWScalings)
{
    struct vkCmdSetViewportWScalingNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.firstViewport = firstViewport;
    params.viewportCount = viewportCount;
    params.pViewportWScalings = pViewportWScalings;
    status = UNIX_CALL(vkCmdSetViewportWScalingNV, &params);
    assert(!status);
}

void WINAPI vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport *pViewports)
{
    struct vkCmdSetViewportWithCount_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.viewportCount = viewportCount;
    params.pViewports = pViewports;
    status = UNIX_CALL(vkCmdSetViewportWithCount, &params);
    assert(!status);
}

void WINAPI vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport *pViewports)
{
    struct vkCmdSetViewportWithCountEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.viewportCount = viewportCount;
    params.pViewports = pViewports;
    status = UNIX_CALL(vkCmdSetViewportWithCountEXT, &params);
    assert(!status);
}

void WINAPI vkCmdSubpassShadingHUAWEI(VkCommandBuffer commandBuffer)
{
    struct vkCmdSubpassShadingHUAWEI_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkCmdSubpassShadingHUAWEI, &params);
    assert(!status);
}

void WINAPI vkCmdTraceRaysIndirect2KHR(VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress)
{
    struct vkCmdTraceRaysIndirect2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.indirectDeviceAddress = indirectDeviceAddress;
    status = UNIX_CALL(vkCmdTraceRaysIndirect2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress)
{
    struct vkCmdTraceRaysIndirectKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pRaygenShaderBindingTable = pRaygenShaderBindingTable;
    params.pMissShaderBindingTable = pMissShaderBindingTable;
    params.pHitShaderBindingTable = pHitShaderBindingTable;
    params.pCallableShaderBindingTable = pCallableShaderBindingTable;
    params.indirectDeviceAddress = indirectDeviceAddress;
    status = UNIX_CALL(vkCmdTraceRaysIndirectKHR, &params);
    assert(!status);
}

void WINAPI vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth)
{
    struct vkCmdTraceRaysKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pRaygenShaderBindingTable = pRaygenShaderBindingTable;
    params.pMissShaderBindingTable = pMissShaderBindingTable;
    params.pHitShaderBindingTable = pHitShaderBindingTable;
    params.pCallableShaderBindingTable = pCallableShaderBindingTable;
    params.width = width;
    params.height = height;
    params.depth = depth;
    status = UNIX_CALL(vkCmdTraceRaysKHR, &params);
    assert(!status);
}

void WINAPI vkCmdTraceRaysNV(VkCommandBuffer commandBuffer, VkBuffer raygenShaderBindingTableBuffer, VkDeviceSize raygenShaderBindingOffset, VkBuffer missShaderBindingTableBuffer, VkDeviceSize missShaderBindingOffset, VkDeviceSize missShaderBindingStride, VkBuffer hitShaderBindingTableBuffer, VkDeviceSize hitShaderBindingOffset, VkDeviceSize hitShaderBindingStride, VkBuffer callableShaderBindingTableBuffer, VkDeviceSize callableShaderBindingOffset, VkDeviceSize callableShaderBindingStride, uint32_t width, uint32_t height, uint32_t depth)
{
    struct vkCmdTraceRaysNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.raygenShaderBindingTableBuffer = raygenShaderBindingTableBuffer;
    params.raygenShaderBindingOffset = raygenShaderBindingOffset;
    params.missShaderBindingTableBuffer = missShaderBindingTableBuffer;
    params.missShaderBindingOffset = missShaderBindingOffset;
    params.missShaderBindingStride = missShaderBindingStride;
    params.hitShaderBindingTableBuffer = hitShaderBindingTableBuffer;
    params.hitShaderBindingOffset = hitShaderBindingOffset;
    params.hitShaderBindingStride = hitShaderBindingStride;
    params.callableShaderBindingTableBuffer = callableShaderBindingTableBuffer;
    params.callableShaderBindingOffset = callableShaderBindingOffset;
    params.callableShaderBindingStride = callableShaderBindingStride;
    params.width = width;
    params.height = height;
    params.depth = depth;
    status = UNIX_CALL(vkCmdTraceRaysNV, &params);
    assert(!status);
}

void WINAPI vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void *pData)
{
    struct vkCmdUpdateBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkCmdUpdateBuffer, &params);
    assert(!status);
}

void WINAPI vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
    struct vkCmdWaitEvents_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.eventCount = eventCount;
    params.pEvents = pEvents;
    params.srcStageMask = srcStageMask;
    params.dstStageMask = dstStageMask;
    params.memoryBarrierCount = memoryBarrierCount;
    params.pMemoryBarriers = pMemoryBarriers;
    params.bufferMemoryBarrierCount = bufferMemoryBarrierCount;
    params.pBufferMemoryBarriers = pBufferMemoryBarriers;
    params.imageMemoryBarrierCount = imageMemoryBarrierCount;
    params.pImageMemoryBarriers = pImageMemoryBarriers;
    status = UNIX_CALL(vkCmdWaitEvents, &params);
    assert(!status);
}

void WINAPI vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents, const VkDependencyInfo *pDependencyInfos)
{
    struct vkCmdWaitEvents2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.eventCount = eventCount;
    params.pEvents = pEvents;
    params.pDependencyInfos = pDependencyInfos;
    status = UNIX_CALL(vkCmdWaitEvents2, &params);
    assert(!status);
}

void WINAPI vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents, const VkDependencyInfo *pDependencyInfos)
{
    struct vkCmdWaitEvents2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.eventCount = eventCount;
    params.pEvents = pEvents;
    params.pDependencyInfos = pDependencyInfos;
    status = UNIX_CALL(vkCmdWaitEvents2KHR, &params);
    assert(!status);
}

void WINAPI vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
{
    struct vkCmdWriteAccelerationStructuresPropertiesKHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.accelerationStructureCount = accelerationStructureCount;
    params.pAccelerationStructures = pAccelerationStructures;
    params.queryType = queryType;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    status = UNIX_CALL(vkCmdWriteAccelerationStructuresPropertiesKHR, &params);
    assert(!status);
}

void WINAPI vkCmdWriteAccelerationStructuresPropertiesNV(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureNV *pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
{
    struct vkCmdWriteAccelerationStructuresPropertiesNV_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.accelerationStructureCount = accelerationStructureCount;
    params.pAccelerationStructures = pAccelerationStructures;
    params.queryType = queryType;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    status = UNIX_CALL(vkCmdWriteAccelerationStructuresPropertiesNV, &params);
    assert(!status);
}

void WINAPI vkCmdWriteBufferMarker2AMD(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker)
{
    struct vkCmdWriteBufferMarker2AMD_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.stage = stage;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.marker = marker;
    status = UNIX_CALL(vkCmdWriteBufferMarker2AMD, &params);
    assert(!status);
}

void WINAPI vkCmdWriteBufferMarkerAMD(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker)
{
    struct vkCmdWriteBufferMarkerAMD_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineStage = pipelineStage;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.marker = marker;
    status = UNIX_CALL(vkCmdWriteBufferMarkerAMD, &params);
    assert(!status);
}

void WINAPI vkCmdWriteMicromapsPropertiesEXT(VkCommandBuffer commandBuffer, uint32_t micromapCount, const VkMicromapEXT *pMicromaps, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
{
    struct vkCmdWriteMicromapsPropertiesEXT_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.micromapCount = micromapCount;
    params.pMicromaps = pMicromaps;
    params.queryType = queryType;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    status = UNIX_CALL(vkCmdWriteMicromapsPropertiesEXT, &params);
    assert(!status);
}

void WINAPI vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query)
{
    struct vkCmdWriteTimestamp_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pipelineStage = pipelineStage;
    params.queryPool = queryPool;
    params.query = query;
    status = UNIX_CALL(vkCmdWriteTimestamp, &params);
    assert(!status);
}

void WINAPI vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query)
{
    struct vkCmdWriteTimestamp2_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.stage = stage;
    params.queryPool = queryPool;
    params.query = query;
    status = UNIX_CALL(vkCmdWriteTimestamp2, &params);
    assert(!status);
}

void WINAPI vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query)
{
    struct vkCmdWriteTimestamp2KHR_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.stage = stage;
    params.queryPool = queryPool;
    params.query = query;
    status = UNIX_CALL(vkCmdWriteTimestamp2KHR, &params);
    assert(!status);
}

VkResult WINAPI vkCompileDeferredNV(VkDevice device, VkPipeline pipeline, uint32_t shader)
{
    struct vkCompileDeferredNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.shader = shader;
    status = UNIX_CALL(vkCompileDeferredNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCopyAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR *pInfo)
{
    struct vkCopyAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyAccelerationStructureKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCopyAccelerationStructureToMemoryKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
    struct vkCopyAccelerationStructureToMemoryKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyAccelerationStructureToMemoryKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCopyMemoryToAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
    struct vkCopyMemoryToAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyMemoryToAccelerationStructureKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCopyMemoryToMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToMicromapInfoEXT *pInfo)
{
    struct vkCopyMemoryToMicromapEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyMemoryToMicromapEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCopyMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMicromapInfoEXT *pInfo)
{
    struct vkCopyMicromapEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyMicromapEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCopyMicromapToMemoryEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMicromapToMemoryInfoEXT *pInfo)
{
    struct vkCopyMicromapToMemoryEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyMicromapToMemoryEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkAccelerationStructureKHR *pAccelerationStructure)
{
    struct vkCreateAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pAccelerationStructure = pAccelerationStructure;
    status = UNIX_CALL(vkCreateAccelerationStructureKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateAccelerationStructureNV(VkDevice device, const VkAccelerationStructureCreateInfoNV *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkAccelerationStructureNV *pAccelerationStructure)
{
    struct vkCreateAccelerationStructureNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pAccelerationStructure = pAccelerationStructure;
    status = UNIX_CALL(vkCreateAccelerationStructureNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer)
{
    struct vkCreateBuffer_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pBuffer = pBuffer;
    status = UNIX_CALL(vkCreateBuffer, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBufferView *pView)
{
    struct vkCreateBufferView_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pView = pView;
    status = UNIX_CALL(vkCreateBufferView, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
    struct vkCreateComputePipelines_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pPipelines = pPipelines;
    status = UNIX_CALL(vkCreateComputePipelines, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateCuFunctionNVX(VkDevice device, const VkCuFunctionCreateInfoNVX *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkCuFunctionNVX *pFunction)
{
    struct vkCreateCuFunctionNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pFunction = pFunction;
    status = UNIX_CALL(vkCreateCuFunctionNVX, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateCuModuleNVX(VkDevice device, const VkCuModuleCreateInfoNVX *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkCuModuleNVX *pModule)
{
    struct vkCreateCuModuleNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pModule = pModule;
    status = UNIX_CALL(vkCreateCuModuleNVX, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback)
{
    struct vkCreateDebugReportCallbackEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pCallback = pCallback;
    status = UNIX_CALL(vkCreateDebugReportCallbackEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger)
{
    struct vkCreateDebugUtilsMessengerEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pMessenger = pMessenger;
    status = UNIX_CALL(vkCreateDebugUtilsMessengerEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks *pAllocator, VkDeferredOperationKHR *pDeferredOperation)
{
    struct vkCreateDeferredOperationKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pAllocator = pAllocator;
    params.pDeferredOperation = pDeferredOperation;
    status = UNIX_CALL(vkCreateDeferredOperationKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool)
{
    struct vkCreateDescriptorPool_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pDescriptorPool = pDescriptorPool;
    status = UNIX_CALL(vkCreateDescriptorPool, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
    struct vkCreateDescriptorSetLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSetLayout = pSetLayout;
    status = UNIX_CALL(vkCreateDescriptorSetLayout, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    struct vkCreateDescriptorUpdateTemplate_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pDescriptorUpdateTemplate = pDescriptorUpdateTemplate;
    status = UNIX_CALL(vkCreateDescriptorUpdateTemplate, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateDescriptorUpdateTemplateKHR(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    struct vkCreateDescriptorUpdateTemplateKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pDescriptorUpdateTemplate = pDescriptorUpdateTemplate;
    status = UNIX_CALL(vkCreateDescriptorUpdateTemplateKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateEvent(VkDevice device, const VkEventCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkEvent *pEvent)
{
    struct vkCreateEvent_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pEvent = pEvent;
    status = UNIX_CALL(vkCreateEvent, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateFence(VkDevice device, const VkFenceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFence *pFence)
{
    struct vkCreateFence_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pFence = pFence;
    status = UNIX_CALL(vkCreateFence, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer)
{
    struct vkCreateFramebuffer_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pFramebuffer = pFramebuffer;
    status = UNIX_CALL(vkCreateFramebuffer, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
    struct vkCreateGraphicsPipelines_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pPipelines = pPipelines;
    status = UNIX_CALL(vkCreateGraphicsPipelines, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
    struct vkCreateImage_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pImage = pImage;
    status = UNIX_CALL(vkCreateImage, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImageView *pView)
{
    struct vkCreateImageView_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pView = pView;
    status = UNIX_CALL(vkCreateImageView, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateIndirectCommandsLayoutNV(VkDevice device, const VkIndirectCommandsLayoutCreateInfoNV *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkIndirectCommandsLayoutNV *pIndirectCommandsLayout)
{
    struct vkCreateIndirectCommandsLayoutNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pIndirectCommandsLayout = pIndirectCommandsLayout;
    status = UNIX_CALL(vkCreateIndirectCommandsLayoutNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateMicromapEXT(VkDevice device, const VkMicromapCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkMicromapEXT *pMicromap)
{
    struct vkCreateMicromapEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pMicromap = pMicromap;
    status = UNIX_CALL(vkCreateMicromapEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateOpticalFlowSessionNV(VkDevice device, const VkOpticalFlowSessionCreateInfoNV *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkOpticalFlowSessionNV *pSession)
{
    struct vkCreateOpticalFlowSessionNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSession = pSession;
    status = UNIX_CALL(vkCreateOpticalFlowSessionNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineCache *pPipelineCache)
{
    struct vkCreatePipelineCache_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pPipelineCache = pPipelineCache;
    status = UNIX_CALL(vkCreatePipelineCache, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout)
{
    struct vkCreatePipelineLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pPipelineLayout = pPipelineLayout;
    status = UNIX_CALL(vkCreatePipelineLayout, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPrivateDataSlot *pPrivateDataSlot)
{
    struct vkCreatePrivateDataSlot_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pPrivateDataSlot = pPrivateDataSlot;
    status = UNIX_CALL(vkCreatePrivateDataSlot, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreatePrivateDataSlotEXT(VkDevice device, const VkPrivateDataSlotCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPrivateDataSlot *pPrivateDataSlot)
{
    struct vkCreatePrivateDataSlotEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pPrivateDataSlot = pPrivateDataSlot;
    status = UNIX_CALL(vkCreatePrivateDataSlotEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkQueryPool *pQueryPool)
{
    struct vkCreateQueryPool_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pQueryPool = pQueryPool;
    status = UNIX_CALL(vkCreateQueryPool, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
    struct vkCreateRayTracingPipelinesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pipelineCache = pipelineCache;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pPipelines = pPipelines;
    status = UNIX_CALL(vkCreateRayTracingPipelinesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateRayTracingPipelinesNV(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoNV *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
    struct vkCreateRayTracingPipelinesNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pPipelines = pPipelines;
    status = UNIX_CALL(vkCreateRayTracingPipelinesNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
    struct vkCreateRenderPass_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pRenderPass = pRenderPass;
    status = UNIX_CALL(vkCreateRenderPass, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
    struct vkCreateRenderPass2_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pRenderPass = pRenderPass;
    status = UNIX_CALL(vkCreateRenderPass2, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateRenderPass2KHR(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
    struct vkCreateRenderPass2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pRenderPass = pRenderPass;
    status = UNIX_CALL(vkCreateRenderPass2KHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSampler *pSampler)
{
    struct vkCreateSampler_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSampler = pSampler;
    status = UNIX_CALL(vkCreateSampler, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSamplerYcbcrConversion *pYcbcrConversion)
{
    struct vkCreateSamplerYcbcrConversion_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pYcbcrConversion = pYcbcrConversion;
    status = UNIX_CALL(vkCreateSamplerYcbcrConversion, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateSamplerYcbcrConversionKHR(VkDevice device, const VkSamplerYcbcrConversionCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSamplerYcbcrConversion *pYcbcrConversion)
{
    struct vkCreateSamplerYcbcrConversionKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pYcbcrConversion = pYcbcrConversion;
    status = UNIX_CALL(vkCreateSamplerYcbcrConversionKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore)
{
    struct vkCreateSemaphore_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSemaphore = pSemaphore;
    status = UNIX_CALL(vkCreateSemaphore, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule)
{
    struct vkCreateShaderModule_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pShaderModule = pShaderModule;
    status = UNIX_CALL(vkCreateShaderModule, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
    struct vkCreateSwapchainKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSwapchain = pSwapchain;
    status = UNIX_CALL(vkCreateSwapchainKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateValidationCacheEXT(VkDevice device, const VkValidationCacheCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkValidationCacheEXT *pValidationCache)
{
    struct vkCreateValidationCacheEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pValidationCache = pValidationCache;
    status = UNIX_CALL(vkCreateValidationCacheEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
    struct vkCreateWin32SurfaceKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSurface = pSurface;
    status = UNIX_CALL(vkCreateWin32SurfaceKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkDebugMarkerSetObjectNameEXT(VkDevice device, const VkDebugMarkerObjectNameInfoEXT *pNameInfo)
{
    struct vkDebugMarkerSetObjectNameEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pNameInfo = pNameInfo;
    status = UNIX_CALL(vkDebugMarkerSetObjectNameEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkDebugMarkerSetObjectTagEXT(VkDevice device, const VkDebugMarkerObjectTagInfoEXT *pTagInfo)
{
    struct vkDebugMarkerSetObjectTagEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pTagInfo = pTagInfo;
    status = UNIX_CALL(vkDebugMarkerSetObjectTagEXT, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkDebugReportMessageEXT(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char *pLayerPrefix, const char *pMessage)
{
    struct vkDebugReportMessageEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.flags = flags;
    params.objectType = objectType;
    params.object = object;
    params.location = location;
    params.messageCode = messageCode;
    params.pLayerPrefix = pLayerPrefix;
    params.pMessage = pMessage;
    status = UNIX_CALL(vkDebugReportMessageEXT, &params);
    assert(!status);
}

VkResult WINAPI vkDeferredOperationJoinKHR(VkDevice device, VkDeferredOperationKHR operation)
{
    struct vkDeferredOperationJoinKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.operation = operation;
    status = UNIX_CALL(vkDeferredOperationJoinKHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.accelerationStructure = accelerationStructure;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyAccelerationStructureKHR, &params);
    assert(!status);
}

void WINAPI vkDestroyAccelerationStructureNV(VkDevice device, VkAccelerationStructureNV accelerationStructure, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyAccelerationStructureNV_params params;
    NTSTATUS status;
    params.device = device;
    params.accelerationStructure = accelerationStructure;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyAccelerationStructureNV, &params);
    assert(!status);
}

void WINAPI vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyBuffer_params params;
    NTSTATUS status;
    params.device = device;
    params.buffer = buffer;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyBuffer, &params);
    assert(!status);
}

void WINAPI vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyBufferView_params params;
    NTSTATUS status;
    params.device = device;
    params.bufferView = bufferView;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyBufferView, &params);
    assert(!status);
}

void WINAPI vkDestroyCuFunctionNVX(VkDevice device, VkCuFunctionNVX function, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyCuFunctionNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.function = function;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyCuFunctionNVX, &params);
    assert(!status);
}

void WINAPI vkDestroyCuModuleNVX(VkDevice device, VkCuModuleNVX module, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyCuModuleNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.module = module;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyCuModuleNVX, &params);
    assert(!status);
}

void WINAPI vkDestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDebugReportCallbackEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.callback = callback;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDebugReportCallbackEXT, &params);
    assert(!status);
}

void WINAPI vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDebugUtilsMessengerEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.messenger = messenger;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDebugUtilsMessengerEXT, &params);
    assert(!status);
}

void WINAPI vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDeferredOperationKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.operation = operation;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDeferredOperationKHR, &params);
    assert(!status);
}

void WINAPI vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDescriptorPool_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorPool = descriptorPool;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDescriptorPool, &params);
    assert(!status);
}

void WINAPI vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDescriptorSetLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorSetLayout = descriptorSetLayout;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDescriptorSetLayout, &params);
    assert(!status);
}

void WINAPI vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDescriptorUpdateTemplate_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDescriptorUpdateTemplate, &params);
    assert(!status);
}

void WINAPI vkDestroyDescriptorUpdateTemplateKHR(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDescriptorUpdateTemplateKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDescriptorUpdateTemplateKHR, &params);
    assert(!status);
}

void WINAPI vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyEvent_params params;
    NTSTATUS status;
    params.device = device;
    params.event = event;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyEvent, &params);
    assert(!status);
}

void WINAPI vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyFence_params params;
    NTSTATUS status;
    params.device = device;
    params.fence = fence;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyFence, &params);
    assert(!status);
}

void WINAPI vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyFramebuffer_params params;
    NTSTATUS status;
    params.device = device;
    params.framebuffer = framebuffer;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyFramebuffer, &params);
    assert(!status);
}

void WINAPI vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyImage_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyImage, &params);
    assert(!status);
}

void WINAPI vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyImageView_params params;
    NTSTATUS status;
    params.device = device;
    params.imageView = imageView;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyImageView, &params);
    assert(!status);
}

void WINAPI vkDestroyIndirectCommandsLayoutNV(VkDevice device, VkIndirectCommandsLayoutNV indirectCommandsLayout, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyIndirectCommandsLayoutNV_params params;
    NTSTATUS status;
    params.device = device;
    params.indirectCommandsLayout = indirectCommandsLayout;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyIndirectCommandsLayoutNV, &params);
    assert(!status);
}

void WINAPI vkDestroyMicromapEXT(VkDevice device, VkMicromapEXT micromap, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyMicromapEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.micromap = micromap;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyMicromapEXT, &params);
    assert(!status);
}

void WINAPI vkDestroyOpticalFlowSessionNV(VkDevice device, VkOpticalFlowSessionNV session, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyOpticalFlowSessionNV_params params;
    NTSTATUS status;
    params.device = device;
    params.session = session;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyOpticalFlowSessionNV, &params);
    assert(!status);
}

void WINAPI vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPipeline_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPipeline, &params);
    assert(!status);
}

void WINAPI vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPipelineCache_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPipelineCache, &params);
    assert(!status);
}

void WINAPI vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPipelineLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineLayout = pipelineLayout;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPipelineLayout, &params);
    assert(!status);
}

void WINAPI vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPrivateDataSlot_params params;
    NTSTATUS status;
    params.device = device;
    params.privateDataSlot = privateDataSlot;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPrivateDataSlot, &params);
    assert(!status);
}

void WINAPI vkDestroyPrivateDataSlotEXT(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPrivateDataSlotEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.privateDataSlot = privateDataSlot;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPrivateDataSlotEXT, &params);
    assert(!status);
}

void WINAPI vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyQueryPool_params params;
    NTSTATUS status;
    params.device = device;
    params.queryPool = queryPool;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyQueryPool, &params);
    assert(!status);
}

void WINAPI vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyRenderPass_params params;
    NTSTATUS status;
    params.device = device;
    params.renderPass = renderPass;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyRenderPass, &params);
    assert(!status);
}

void WINAPI vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySampler_params params;
    NTSTATUS status;
    params.device = device;
    params.sampler = sampler;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySampler, &params);
    assert(!status);
}

void WINAPI vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySamplerYcbcrConversion_params params;
    NTSTATUS status;
    params.device = device;
    params.ycbcrConversion = ycbcrConversion;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySamplerYcbcrConversion, &params);
    assert(!status);
}

void WINAPI vkDestroySamplerYcbcrConversionKHR(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySamplerYcbcrConversionKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.ycbcrConversion = ycbcrConversion;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySamplerYcbcrConversionKHR, &params);
    assert(!status);
}

void WINAPI vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySemaphore_params params;
    NTSTATUS status;
    params.device = device;
    params.semaphore = semaphore;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySemaphore, &params);
    assert(!status);
}

void WINAPI vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyShaderModule_params params;
    NTSTATUS status;
    params.device = device;
    params.shaderModule = shaderModule;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyShaderModule, &params);
    assert(!status);
}

void WINAPI vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySurfaceKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.surface = surface;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySurfaceKHR, &params);
    assert(!status);
}

void WINAPI vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySwapchainKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySwapchainKHR, &params);
    assert(!status);
}

void WINAPI vkDestroyValidationCacheEXT(VkDevice device, VkValidationCacheEXT validationCache, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyValidationCacheEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.validationCache = validationCache;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyValidationCacheEXT, &params);
    assert(!status);
}

VkResult WINAPI vkDeviceWaitIdle(VkDevice device)
{
    struct vkDeviceWaitIdle_params params;
    NTSTATUS status;
    params.device = device;
    status = UNIX_CALL(vkDeviceWaitIdle, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{
    struct vkEndCommandBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkEndCommandBuffer, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
    struct vkEnumerateDeviceExtensionProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pLayerName = pLayerName;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkEnumerateDeviceExtensionProperties, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
    struct vkEnumerateDeviceLayerProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkEnumerateDeviceLayerProperties, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
    struct vkEnumeratePhysicalDeviceGroups_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pPhysicalDeviceGroupCount = pPhysicalDeviceGroupCount;
    params.pPhysicalDeviceGroupProperties = pPhysicalDeviceGroupProperties;
    status = UNIX_CALL(vkEnumeratePhysicalDeviceGroups, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
    struct vkEnumeratePhysicalDeviceGroupsKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pPhysicalDeviceGroupCount = pPhysicalDeviceGroupCount;
    params.pPhysicalDeviceGroupProperties = pPhysicalDeviceGroupProperties;
    status = UNIX_CALL(vkEnumeratePhysicalDeviceGroupsKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t *pCounterCount, VkPerformanceCounterKHR *pCounters, VkPerformanceCounterDescriptionKHR *pCounterDescriptions)
{
    struct vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.queueFamilyIndex = queueFamilyIndex;
    params.pCounterCount = pCounterCount;
    params.pCounters = pCounters;
    params.pCounterDescriptions = pCounterDescriptions;
    status = UNIX_CALL(vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount, VkPhysicalDevice *pPhysicalDevices)
{
    struct vkEnumeratePhysicalDevices_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pPhysicalDeviceCount = pPhysicalDeviceCount;
    params.pPhysicalDevices = pPhysicalDevices;
    status = UNIX_CALL(vkEnumeratePhysicalDevices, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges)
{
    struct vkFlushMappedMemoryRanges_params params;
    NTSTATUS status;
    params.device = device;
    params.memoryRangeCount = memoryRangeCount;
    params.pMemoryRanges = pMemoryRanges;
    status = UNIX_CALL(vkFlushMappedMemoryRanges, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets)
{
    struct vkFreeDescriptorSets_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorPool = descriptorPool;
    params.descriptorSetCount = descriptorSetCount;
    params.pDescriptorSets = pDescriptorSets;
    status = UNIX_CALL(vkFreeDescriptorSets, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *pAllocator)
{
    struct vkFreeMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkFreeMemory, &params);
    assert(!status);
}

void WINAPI vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo, const uint32_t *pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
    struct vkGetAccelerationStructureBuildSizesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.buildType = buildType;
    params.pBuildInfo = pBuildInfo;
    params.pMaxPrimitiveCounts = pMaxPrimitiveCounts;
    params.pSizeInfo = pSizeInfo;
    status = UNIX_CALL(vkGetAccelerationStructureBuildSizesKHR, &params);
    assert(!status);
}

VkDeviceAddress WINAPI vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR *pInfo)
{
    struct vkGetAccelerationStructureDeviceAddressKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetAccelerationStructureDeviceAddressKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetAccelerationStructureHandleNV(VkDevice device, VkAccelerationStructureNV accelerationStructure, size_t dataSize, void *pData)
{
    struct vkGetAccelerationStructureHandleNV_params params;
    NTSTATUS status;
    params.device = device;
    params.accelerationStructure = accelerationStructure;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetAccelerationStructureHandleNV, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetAccelerationStructureMemoryRequirementsNV(VkDevice device, const VkAccelerationStructureMemoryRequirementsInfoNV *pInfo, VkMemoryRequirements2KHR *pMemoryRequirements)
{
    struct vkGetAccelerationStructureMemoryRequirementsNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetAccelerationStructureMemoryRequirementsNV, &params);
    assert(!status);
}

VkResult WINAPI vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkAccelerationStructureCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status);
    return params.result;
}

VkDeviceAddress WINAPI vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferDeviceAddress_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferDeviceAddress, &params);
    assert(!status);
    return params.result;
}

VkDeviceAddress WINAPI vkGetBufferDeviceAddressEXT(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferDeviceAddressEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferDeviceAddressEXT, &params);
    assert(!status);
    return params.result;
}

VkDeviceAddress WINAPI vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferDeviceAddressKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferDeviceAddressKHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements *pMemoryRequirements)
{
    struct vkGetBufferMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.buffer = buffer;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetBufferMemoryRequirements, &params);
    assert(!status);
}

void WINAPI vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetBufferMemoryRequirements2_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetBufferMemoryRequirements2, &params);
    assert(!status);
}

void WINAPI vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetBufferMemoryRequirements2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetBufferMemoryRequirements2KHR, &params);
    assert(!status);
}

uint64_t WINAPI vkGetBufferOpaqueCaptureAddress(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferOpaqueCaptureAddress_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferOpaqueCaptureAddress, &params);
    assert(!status);
    return params.result;
}

uint64_t WINAPI vkGetBufferOpaqueCaptureAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferOpaqueCaptureAddressKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferOpaqueCaptureAddressKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetBufferOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkBufferCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetBufferOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetBufferOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status);
    return params.result;
}

uint32_t WINAPI vkGetDeferredOperationMaxConcurrencyKHR(VkDevice device, VkDeferredOperationKHR operation)
{
    struct vkGetDeferredOperationMaxConcurrencyKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.operation = operation;
    status = UNIX_CALL(vkGetDeferredOperationMaxConcurrencyKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetDeferredOperationResultKHR(VkDevice device, VkDeferredOperationKHR operation)
{
    struct vkGetDeferredOperationResultKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.operation = operation;
    status = UNIX_CALL(vkGetDeferredOperationResultKHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetDescriptorEXT(VkDevice device, const VkDescriptorGetInfoEXT *pDescriptorInfo, size_t dataSize, void *pDescriptor)
{
    struct vkGetDescriptorEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pDescriptorInfo = pDescriptorInfo;
    params.dataSize = dataSize;
    params.pDescriptor = pDescriptor;
    status = UNIX_CALL(vkGetDescriptorEXT, &params);
    assert(!status);
}

void WINAPI vkGetDescriptorSetHostMappingVALVE(VkDevice device, VkDescriptorSet descriptorSet, void **ppData)
{
    struct vkGetDescriptorSetHostMappingVALVE_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorSet = descriptorSet;
    params.ppData = ppData;
    status = UNIX_CALL(vkGetDescriptorSetHostMappingVALVE, &params);
    assert(!status);
}

void WINAPI vkGetDescriptorSetLayoutBindingOffsetEXT(VkDevice device, VkDescriptorSetLayout layout, uint32_t binding, VkDeviceSize *pOffset)
{
    struct vkGetDescriptorSetLayoutBindingOffsetEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.layout = layout;
    params.binding = binding;
    params.pOffset = pOffset;
    status = UNIX_CALL(vkGetDescriptorSetLayoutBindingOffsetEXT, &params);
    assert(!status);
}

void WINAPI vkGetDescriptorSetLayoutHostMappingInfoVALVE(VkDevice device, const VkDescriptorSetBindingReferenceVALVE *pBindingReference, VkDescriptorSetLayoutHostMappingInfoVALVE *pHostMapping)
{
    struct vkGetDescriptorSetLayoutHostMappingInfoVALVE_params params;
    NTSTATUS status;
    params.device = device;
    params.pBindingReference = pBindingReference;
    params.pHostMapping = pHostMapping;
    status = UNIX_CALL(vkGetDescriptorSetLayoutHostMappingInfoVALVE, &params);
    assert(!status);
}

void WINAPI vkGetDescriptorSetLayoutSizeEXT(VkDevice device, VkDescriptorSetLayout layout, VkDeviceSize *pLayoutSizeInBytes)
{
    struct vkGetDescriptorSetLayoutSizeEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.layout = layout;
    params.pLayoutSizeInBytes = pLayoutSizeInBytes;
    status = UNIX_CALL(vkGetDescriptorSetLayoutSizeEXT, &params);
    assert(!status);
}

void WINAPI vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, VkDescriptorSetLayoutSupport *pSupport)
{
    struct vkGetDescriptorSetLayoutSupport_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pSupport = pSupport;
    status = UNIX_CALL(vkGetDescriptorSetLayoutSupport, &params);
    assert(!status);
}

void WINAPI vkGetDescriptorSetLayoutSupportKHR(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, VkDescriptorSetLayoutSupport *pSupport)
{
    struct vkGetDescriptorSetLayoutSupportKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pSupport = pSupport;
    status = UNIX_CALL(vkGetDescriptorSetLayoutSupportKHR, &params);
    assert(!status);
}

void WINAPI vkGetDeviceAccelerationStructureCompatibilityKHR(VkDevice device, const VkAccelerationStructureVersionInfoKHR *pVersionInfo, VkAccelerationStructureCompatibilityKHR *pCompatibility)
{
    struct vkGetDeviceAccelerationStructureCompatibilityKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pVersionInfo = pVersionInfo;
    params.pCompatibility = pCompatibility;
    status = UNIX_CALL(vkGetDeviceAccelerationStructureCompatibilityKHR, &params);
    assert(!status);
}

void WINAPI vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetDeviceBufferMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceBufferMemoryRequirements, &params);
    assert(!status);
}

void WINAPI vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetDeviceBufferMemoryRequirementsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceBufferMemoryRequirementsKHR, &params);
    assert(!status);
}

VkResult WINAPI vkGetDeviceFaultInfoEXT(VkDevice device, VkDeviceFaultCountsEXT *pFaultCounts, VkDeviceFaultInfoEXT *pFaultInfo)
{
    struct vkGetDeviceFaultInfoEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pFaultCounts = pFaultCounts;
    params.pFaultInfo = pFaultInfo;
    status = UNIX_CALL(vkGetDeviceFaultInfoEXT, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags *pPeerMemoryFeatures)
{
    struct vkGetDeviceGroupPeerMemoryFeatures_params params;
    NTSTATUS status;
    params.device = device;
    params.heapIndex = heapIndex;
    params.localDeviceIndex = localDeviceIndex;
    params.remoteDeviceIndex = remoteDeviceIndex;
    params.pPeerMemoryFeatures = pPeerMemoryFeatures;
    status = UNIX_CALL(vkGetDeviceGroupPeerMemoryFeatures, &params);
    assert(!status);
}

void WINAPI vkGetDeviceGroupPeerMemoryFeaturesKHR(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags *pPeerMemoryFeatures)
{
    struct vkGetDeviceGroupPeerMemoryFeaturesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.heapIndex = heapIndex;
    params.localDeviceIndex = localDeviceIndex;
    params.remoteDeviceIndex = remoteDeviceIndex;
    params.pPeerMemoryFeatures = pPeerMemoryFeatures;
    status = UNIX_CALL(vkGetDeviceGroupPeerMemoryFeaturesKHR, &params);
    assert(!status);
}

VkResult WINAPI vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice device, VkDeviceGroupPresentCapabilitiesKHR *pDeviceGroupPresentCapabilities)
{
    struct vkGetDeviceGroupPresentCapabilitiesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pDeviceGroupPresentCapabilities = pDeviceGroupPresentCapabilities;
    status = UNIX_CALL(vkGetDeviceGroupPresentCapabilitiesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR *pModes)
{
    struct vkGetDeviceGroupSurfacePresentModesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.surface = surface;
    params.pModes = pModes;
    status = UNIX_CALL(vkGetDeviceGroupSurfacePresentModesKHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetDeviceImageMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceImageMemoryRequirements, &params);
    assert(!status);
}

void WINAPI vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetDeviceImageMemoryRequirementsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceImageMemoryRequirementsKHR, &params);
    assert(!status);
}

void WINAPI vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    struct vkGetDeviceImageSparseMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceImageSparseMemoryRequirements, &params);
    assert(!status);
}

void WINAPI vkGetDeviceImageSparseMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    struct vkGetDeviceImageSparseMemoryRequirementsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceImageSparseMemoryRequirementsKHR, &params);
    assert(!status);
}

void WINAPI vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize *pCommittedMemoryInBytes)
{
    struct vkGetDeviceMemoryCommitment_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    params.pCommittedMemoryInBytes = pCommittedMemoryInBytes;
    status = UNIX_CALL(vkGetDeviceMemoryCommitment, &params);
    assert(!status);
}

uint64_t WINAPI vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo *pInfo)
{
    struct vkGetDeviceMemoryOpaqueCaptureAddress_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetDeviceMemoryOpaqueCaptureAddress, &params);
    assert(!status);
    return params.result;
}

uint64_t WINAPI vkGetDeviceMemoryOpaqueCaptureAddressKHR(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo *pInfo)
{
    struct vkGetDeviceMemoryOpaqueCaptureAddressKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetDeviceMemoryOpaqueCaptureAddressKHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetDeviceMicromapCompatibilityEXT(VkDevice device, const VkMicromapVersionInfoEXT *pVersionInfo, VkAccelerationStructureCompatibilityKHR *pCompatibility)
{
    struct vkGetDeviceMicromapCompatibilityEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pVersionInfo = pVersionInfo;
    params.pCompatibility = pCompatibility;
    status = UNIX_CALL(vkGetDeviceMicromapCompatibilityEXT, &params);
    assert(!status);
}

void WINAPI vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue)
{
    struct vkGetDeviceQueue_params params;
    NTSTATUS status;
    params.device = device;
    params.queueFamilyIndex = queueFamilyIndex;
    params.queueIndex = queueIndex;
    params.pQueue = pQueue;
    status = UNIX_CALL(vkGetDeviceQueue, &params);
    assert(!status);
}

void WINAPI vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue)
{
    struct vkGetDeviceQueue2_params params;
    NTSTATUS status;
    params.device = device;
    params.pQueueInfo = pQueueInfo;
    params.pQueue = pQueue;
    status = UNIX_CALL(vkGetDeviceQueue2, &params);
    assert(!status);
}

VkResult WINAPI vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI(VkDevice device, VkRenderPass renderpass, VkExtent2D *pMaxWorkgroupSize)
{
    struct vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI_params params;
    NTSTATUS status;
    params.device = device;
    params.renderpass = renderpass;
    params.pMaxWorkgroupSize = pMaxWorkgroupSize;
    status = UNIX_CALL(vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetDynamicRenderingTilePropertiesQCOM(VkDevice device, const VkRenderingInfo *pRenderingInfo, VkTilePropertiesQCOM *pProperties)
{
    struct vkGetDynamicRenderingTilePropertiesQCOM_params params;
    NTSTATUS status;
    params.device = device;
    params.pRenderingInfo = pRenderingInfo;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetDynamicRenderingTilePropertiesQCOM, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetEventStatus(VkDevice device, VkEvent event)
{
    struct vkGetEventStatus_params params;
    NTSTATUS status;
    params.device = device;
    params.event = event;
    status = UNIX_CALL(vkGetEventStatus, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetFenceStatus(VkDevice device, VkFence fence)
{
    struct vkGetFenceStatus_params params;
    NTSTATUS status;
    params.device = device;
    params.fence = fence;
    status = UNIX_CALL(vkGetFenceStatus, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetFramebufferTilePropertiesQCOM(VkDevice device, VkFramebuffer framebuffer, uint32_t *pPropertiesCount, VkTilePropertiesQCOM *pProperties)
{
    struct vkGetFramebufferTilePropertiesQCOM_params params;
    NTSTATUS status;
    params.device = device;
    params.framebuffer = framebuffer;
    params.pPropertiesCount = pPropertiesCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetFramebufferTilePropertiesQCOM, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetGeneratedCommandsMemoryRequirementsNV(VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoNV *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetGeneratedCommandsMemoryRequirementsNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetGeneratedCommandsMemoryRequirementsNV, &params);
    assert(!status);
}

void WINAPI vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements *pMemoryRequirements)
{
    struct vkGetImageMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetImageMemoryRequirements, &params);
    assert(!status);
}

void WINAPI vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetImageMemoryRequirements2_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetImageMemoryRequirements2, &params);
    assert(!status);
}

void WINAPI vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetImageMemoryRequirements2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetImageMemoryRequirements2KHR, &params);
    assert(!status);
}

VkResult WINAPI vkGetImageOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkImageCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetImageOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetImageOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements *pSparseMemoryRequirements)
{
    struct vkGetImageSparseMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetImageSparseMemoryRequirements, &params);
    assert(!status);
}

void WINAPI vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2 *pInfo, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    struct vkGetImageSparseMemoryRequirements2_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetImageSparseMemoryRequirements2, &params);
    assert(!status);
}

void WINAPI vkGetImageSparseMemoryRequirements2KHR(VkDevice device, const VkImageSparseMemoryRequirementsInfo2 *pInfo, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    struct vkGetImageSparseMemoryRequirements2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetImageSparseMemoryRequirements2KHR, &params);
    assert(!status);
}

void WINAPI vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource *pSubresource, VkSubresourceLayout *pLayout)
{
    struct vkGetImageSubresourceLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pSubresource = pSubresource;
    params.pLayout = pLayout;
    status = UNIX_CALL(vkGetImageSubresourceLayout, &params);
    assert(!status);
}

void WINAPI vkGetImageSubresourceLayout2EXT(VkDevice device, VkImage image, const VkImageSubresource2EXT *pSubresource, VkSubresourceLayout2EXT *pLayout)
{
    struct vkGetImageSubresourceLayout2EXT_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pSubresource = pSubresource;
    params.pLayout = pLayout;
    status = UNIX_CALL(vkGetImageSubresourceLayout2EXT, &params);
    assert(!status);
}

VkResult WINAPI vkGetImageViewAddressNVX(VkDevice device, VkImageView imageView, VkImageViewAddressPropertiesNVX *pProperties)
{
    struct vkGetImageViewAddressNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.imageView = imageView;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetImageViewAddressNVX, &params);
    assert(!status);
    return params.result;
}

uint32_t WINAPI vkGetImageViewHandleNVX(VkDevice device, const VkImageViewHandleInfoNVX *pInfo)
{
    struct vkGetImageViewHandleNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetImageViewHandleNVX, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetImageViewOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkImageViewCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetImageViewOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetImageViewOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetMemoryHostPointerPropertiesEXT(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, const void *pHostPointer, VkMemoryHostPointerPropertiesEXT *pMemoryHostPointerProperties)
{
    struct vkGetMemoryHostPointerPropertiesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.handleType = handleType;
    params.pHostPointer = pHostPointer;
    params.pMemoryHostPointerProperties = pMemoryHostPointerProperties;
    status = UNIX_CALL(vkGetMemoryHostPointerPropertiesEXT, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetMicromapBuildSizesEXT(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkMicromapBuildInfoEXT *pBuildInfo, VkMicromapBuildSizesInfoEXT *pSizeInfo)
{
    struct vkGetMicromapBuildSizesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.buildType = buildType;
    params.pBuildInfo = pBuildInfo;
    params.pSizeInfo = pSizeInfo;
    status = UNIX_CALL(vkGetMicromapBuildSizesEXT, &params);
    assert(!status);
}

VkResult WINAPI vkGetPerformanceParameterINTEL(VkDevice device, VkPerformanceParameterTypeINTEL parameter, VkPerformanceValueINTEL *pValue)
{
    struct vkGetPerformanceParameterINTEL_params params;
    NTSTATUS status;
    params.device = device;
    params.parameter = parameter;
    params.pValue = pValue;
    status = UNIX_CALL(vkGetPerformanceParameterINTEL, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount, VkTimeDomainEXT *pTimeDomains)
{
    struct vkGetPhysicalDeviceCalibrateableTimeDomainsEXT_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pTimeDomainCount = pTimeDomainCount;
    params.pTimeDomains = pTimeDomains;
    status = UNIX_CALL(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkCooperativeMatrixPropertiesNV *pProperties)
{
    struct vkGetPhysicalDeviceCooperativeMatrixPropertiesNV_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceCooperativeMatrixPropertiesNV, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo, VkExternalBufferProperties *pExternalBufferProperties)
{
    struct vkGetPhysicalDeviceExternalBufferProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalBufferInfo = pExternalBufferInfo;
    params.pExternalBufferProperties = pExternalBufferProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalBufferProperties, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo, VkExternalBufferProperties *pExternalBufferProperties)
{
    struct vkGetPhysicalDeviceExternalBufferPropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalBufferInfo = pExternalBufferInfo;
    params.pExternalBufferProperties = pExternalBufferProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalBufferPropertiesKHR, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo, VkExternalFenceProperties *pExternalFenceProperties)
{
    struct vkGetPhysicalDeviceExternalFenceProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalFenceInfo = pExternalFenceInfo;
    params.pExternalFenceProperties = pExternalFenceProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalFenceProperties, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo, VkExternalFenceProperties *pExternalFenceProperties)
{
    struct vkGetPhysicalDeviceExternalFencePropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalFenceInfo = pExternalFenceInfo;
    params.pExternalFenceProperties = pExternalFenceProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalFencePropertiesKHR, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo, VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
    struct vkGetPhysicalDeviceExternalSemaphoreProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalSemaphoreInfo = pExternalSemaphoreInfo;
    params.pExternalSemaphoreProperties = pExternalSemaphoreProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalSemaphoreProperties, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo, VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
    struct vkGetPhysicalDeviceExternalSemaphorePropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalSemaphoreInfo = pExternalSemaphoreInfo;
    params.pExternalSemaphoreProperties = pExternalSemaphoreProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures *pFeatures)
{
    struct vkGetPhysicalDeviceFeatures_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFeatures = pFeatures;
    status = UNIX_CALL(vkGetPhysicalDeviceFeatures, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures)
{
    struct vkGetPhysicalDeviceFeatures2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFeatures = pFeatures;
    status = UNIX_CALL(vkGetPhysicalDeviceFeatures2, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures)
{
    struct vkGetPhysicalDeviceFeatures2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFeatures = pFeatures;
    status = UNIX_CALL(vkGetPhysicalDeviceFeatures2KHR, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties *pFormatProperties)
{
    struct vkGetPhysicalDeviceFormatProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.pFormatProperties = pFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceFormatProperties, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2 *pFormatProperties)
{
    struct vkGetPhysicalDeviceFormatProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.pFormatProperties = pFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceFormatProperties2, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2 *pFormatProperties)
{
    struct vkGetPhysicalDeviceFormatProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.pFormatProperties = pFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceFormatProperties2KHR, &params);
    assert(!status);
}

VkResult WINAPI vkGetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice physicalDevice, uint32_t *pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR *pFragmentShadingRates)
{
    struct vkGetPhysicalDeviceFragmentShadingRatesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFragmentShadingRateCount = pFragmentShadingRateCount;
    params.pFragmentShadingRates = pFragmentShadingRates;
    status = UNIX_CALL(vkGetPhysicalDeviceFragmentShadingRatesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties *pImageFormatProperties)
{
    struct vkGetPhysicalDeviceImageFormatProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.type = type;
    params.tiling = tiling;
    params.usage = usage;
    params.flags = flags;
    params.pImageFormatProperties = pImageFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceImageFormatProperties, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo, VkImageFormatProperties2 *pImageFormatProperties)
{
    struct vkGetPhysicalDeviceImageFormatProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pImageFormatInfo = pImageFormatInfo;
    params.pImageFormatProperties = pImageFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceImageFormatProperties2, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo, VkImageFormatProperties2 *pImageFormatProperties)
{
    struct vkGetPhysicalDeviceImageFormatProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pImageFormatInfo = pImageFormatInfo;
    params.pImageFormatProperties = pImageFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceImageFormatProperties2KHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
    struct vkGetPhysicalDeviceMemoryProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pMemoryProperties = pMemoryProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceMemoryProperties, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
    struct vkGetPhysicalDeviceMemoryProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pMemoryProperties = pMemoryProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceMemoryProperties2, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
    struct vkGetPhysicalDeviceMemoryProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pMemoryProperties = pMemoryProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceMemoryProperties2KHR, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT *pMultisampleProperties)
{
    struct vkGetPhysicalDeviceMultisamplePropertiesEXT_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.samples = samples;
    params.pMultisampleProperties = pMultisampleProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceMultisamplePropertiesEXT, &params);
    assert(!status);
}

VkResult WINAPI vkGetPhysicalDeviceOpticalFlowImageFormatsNV(VkPhysicalDevice physicalDevice, const VkOpticalFlowImageFormatInfoNV *pOpticalFlowImageFormatInfo, uint32_t *pFormatCount, VkOpticalFlowImageFormatPropertiesNV *pImageFormatProperties)
{
    struct vkGetPhysicalDeviceOpticalFlowImageFormatsNV_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pOpticalFlowImageFormatInfo = pOpticalFlowImageFormatInfo;
    params.pFormatCount = pFormatCount;
    params.pImageFormatProperties = pImageFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceOpticalFlowImageFormatsNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *pRectCount, VkRect2D *pRects)
{
    struct vkGetPhysicalDevicePresentRectanglesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.surface = surface;
    params.pRectCount = pRectCount;
    params.pRects = pRects;
    status = UNIX_CALL(vkGetPhysicalDevicePresentRectanglesKHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties *pProperties)
{
    struct vkGetPhysicalDeviceProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceProperties, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(VkPhysicalDevice physicalDevice, const VkQueryPoolPerformanceCreateInfoKHR *pPerformanceQueryCreateInfo, uint32_t *pNumPasses)
{
    struct vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPerformanceQueryCreateInfo = pPerformanceQueryCreateInfo;
    params.pNumPasses = pNumPasses;
    status = UNIX_CALL(vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties *pQueueFamilyProperties)
{
    struct vkGetPhysicalDeviceQueueFamilyProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pQueueFamilyPropertyCount = pQueueFamilyPropertyCount;
    params.pQueueFamilyProperties = pQueueFamilyProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceQueueFamilyProperties, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
    struct vkGetPhysicalDeviceQueueFamilyProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pQueueFamilyPropertyCount = pQueueFamilyPropertyCount;
    params.pQueueFamilyProperties = pQueueFamilyProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceQueueFamilyProperties2, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
    struct vkGetPhysicalDeviceQueueFamilyProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pQueueFamilyPropertyCount = pQueueFamilyPropertyCount;
    params.pQueueFamilyProperties = pQueueFamilyProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceQueueFamilyProperties2KHR, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t *pPropertyCount, VkSparseImageFormatProperties *pProperties)
{
    struct vkGetPhysicalDeviceSparseImageFormatProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.type = type;
    params.samples = samples;
    params.usage = usage;
    params.tiling = tiling;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceSparseImageFormatProperties, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo, uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties)
{
    struct vkGetPhysicalDeviceSparseImageFormatProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFormatInfo = pFormatInfo;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceSparseImageFormatProperties2, &params);
    assert(!status);
}

void WINAPI vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo, uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties)
{
    struct vkGetPhysicalDeviceSparseImageFormatProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFormatInfo = pFormatInfo;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceSparseImageFormatProperties2KHR, &params);
    assert(!status);
}

VkResult WINAPI vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(VkPhysicalDevice physicalDevice, uint32_t *pCombinationCount, VkFramebufferMixedSamplesCombinationNV *pCombinations)
{
    struct vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pCombinationCount = pCombinationCount;
    params.pCombinations = pCombinations;
    status = UNIX_CALL(vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo, VkSurfaceCapabilities2KHR *pSurfaceCapabilities)
{
    struct vkGetPhysicalDeviceSurfaceCapabilities2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pSurfaceInfo = pSurfaceInfo;
    params.pSurfaceCapabilities = pSurfaceCapabilities;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceCapabilities2KHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
    struct vkGetPhysicalDeviceSurfaceCapabilitiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.surface = surface;
    params.pSurfaceCapabilities = pSurfaceCapabilities;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo, uint32_t *pSurfaceFormatCount, VkSurfaceFormat2KHR *pSurfaceFormats)
{
    struct vkGetPhysicalDeviceSurfaceFormats2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pSurfaceInfo = pSurfaceInfo;
    params.pSurfaceFormatCount = pSurfaceFormatCount;
    params.pSurfaceFormats = pSurfaceFormats;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceFormats2KHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *pSurfaceFormatCount, VkSurfaceFormatKHR *pSurfaceFormats)
{
    struct vkGetPhysicalDeviceSurfaceFormatsKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.surface = surface;
    params.pSurfaceFormatCount = pSurfaceFormatCount;
    params.pSurfaceFormats = pSurfaceFormats;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *pPresentModeCount, VkPresentModeKHR *pPresentModes)
{
    struct vkGetPhysicalDeviceSurfacePresentModesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.surface = surface;
    params.pPresentModeCount = pPresentModeCount;
    params.pPresentModes = pPresentModes;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32 *pSupported)
{
    struct vkGetPhysicalDeviceSurfaceSupportKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.queueFamilyIndex = queueFamilyIndex;
    params.surface = surface;
    params.pSupported = pSupported;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceSupportKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t *pToolCount, VkPhysicalDeviceToolProperties *pToolProperties)
{
    struct vkGetPhysicalDeviceToolProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pToolCount = pToolCount;
    params.pToolProperties = pToolProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceToolProperties, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceToolPropertiesEXT(VkPhysicalDevice physicalDevice, uint32_t *pToolCount, VkPhysicalDeviceToolProperties *pToolProperties)
{
    struct vkGetPhysicalDeviceToolPropertiesEXT_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pToolCount = pToolCount;
    params.pToolProperties = pToolProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceToolPropertiesEXT, &params);
    assert(!status);
    return params.result;
}

VkBool32 WINAPI vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
{
    struct vkGetPhysicalDeviceWin32PresentationSupportKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.queueFamilyIndex = queueFamilyIndex;
    status = UNIX_CALL(vkGetPhysicalDeviceWin32PresentationSupportKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t *pDataSize, void *pData)
{
    struct vkGetPipelineCacheData_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.pDataSize = pDataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetPipelineCacheData, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPipelineExecutableInternalRepresentationsKHR(VkDevice device, const VkPipelineExecutableInfoKHR *pExecutableInfo, uint32_t *pInternalRepresentationCount, VkPipelineExecutableInternalRepresentationKHR *pInternalRepresentations)
{
    struct vkGetPipelineExecutableInternalRepresentationsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pExecutableInfo = pExecutableInfo;
    params.pInternalRepresentationCount = pInternalRepresentationCount;
    params.pInternalRepresentations = pInternalRepresentations;
    status = UNIX_CALL(vkGetPipelineExecutableInternalRepresentationsKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPipelineExecutablePropertiesKHR(VkDevice device, const VkPipelineInfoKHR *pPipelineInfo, uint32_t *pExecutableCount, VkPipelineExecutablePropertiesKHR *pProperties)
{
    struct vkGetPipelineExecutablePropertiesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pPipelineInfo = pPipelineInfo;
    params.pExecutableCount = pExecutableCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPipelineExecutablePropertiesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPipelineExecutableStatisticsKHR(VkDevice device, const VkPipelineExecutableInfoKHR *pExecutableInfo, uint32_t *pStatisticCount, VkPipelineExecutableStatisticKHR *pStatistics)
{
    struct vkGetPipelineExecutableStatisticsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pExecutableInfo = pExecutableInfo;
    params.pStatisticCount = pStatisticCount;
    params.pStatistics = pStatistics;
    status = UNIX_CALL(vkGetPipelineExecutableStatisticsKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetPipelinePropertiesEXT(VkDevice device, const VkPipelineInfoEXT *pPipelineInfo, VkBaseOutStructure *pPipelineProperties)
{
    struct vkGetPipelinePropertiesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pPipelineInfo = pPipelineInfo;
    params.pPipelineProperties = pPipelineProperties;
    status = UNIX_CALL(vkGetPipelinePropertiesEXT, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t *pData)
{
    struct vkGetPrivateData_params params;
    NTSTATUS status;
    params.device = device;
    params.objectType = objectType;
    params.objectHandle = objectHandle;
    params.privateDataSlot = privateDataSlot;
    params.pData = pData;
    status = UNIX_CALL(vkGetPrivateData, &params);
    assert(!status);
}

void WINAPI vkGetPrivateDataEXT(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t *pData)
{
    struct vkGetPrivateDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.objectType = objectType;
    params.objectHandle = objectHandle;
    params.privateDataSlot = privateDataSlot;
    params.pData = pData;
    status = UNIX_CALL(vkGetPrivateDataEXT, &params);
    assert(!status);
}

VkResult WINAPI vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *pData, VkDeviceSize stride, VkQueryResultFlags flags)
{
    struct vkGetQueryPoolResults_params params;
    NTSTATUS status;
    params.device = device;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    params.dataSize = dataSize;
    params.pData = pData;
    params.stride = stride;
    params.flags = flags;
    status = UNIX_CALL(vkGetQueryPoolResults, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetQueueCheckpointData2NV(VkQueue queue, uint32_t *pCheckpointDataCount, VkCheckpointData2NV *pCheckpointData)
{
    struct vkGetQueueCheckpointData2NV_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pCheckpointDataCount = pCheckpointDataCount;
    params.pCheckpointData = pCheckpointData;
    status = UNIX_CALL(vkGetQueueCheckpointData2NV, &params);
    assert(!status);
}

void WINAPI vkGetQueueCheckpointDataNV(VkQueue queue, uint32_t *pCheckpointDataCount, VkCheckpointDataNV *pCheckpointData)
{
    struct vkGetQueueCheckpointDataNV_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pCheckpointDataCount = pCheckpointDataCount;
    params.pCheckpointData = pCheckpointData;
    status = UNIX_CALL(vkGetQueueCheckpointDataNV, &params);
    assert(!status);
}

VkResult WINAPI vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData)
{
    struct vkGetRayTracingCaptureReplayShaderGroupHandlesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.firstGroup = firstGroup;
    params.groupCount = groupCount;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData)
{
    struct vkGetRayTracingShaderGroupHandlesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.firstGroup = firstGroup;
    params.groupCount = groupCount;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetRayTracingShaderGroupHandlesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetRayTracingShaderGroupHandlesNV(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData)
{
    struct vkGetRayTracingShaderGroupHandlesNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.firstGroup = firstGroup;
    params.groupCount = groupCount;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetRayTracingShaderGroupHandlesNV, &params);
    assert(!status);
    return params.result;
}

VkDeviceSize WINAPI vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader)
{
    struct vkGetRayTracingShaderGroupStackSizeKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.group = group;
    params.groupShader = groupShader;
    status = UNIX_CALL(vkGetRayTracingShaderGroupStackSizeKHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D *pGranularity)
{
    struct vkGetRenderAreaGranularity_params params;
    NTSTATUS status;
    params.device = device;
    params.renderPass = renderPass;
    params.pGranularity = pGranularity;
    status = UNIX_CALL(vkGetRenderAreaGranularity, &params);
    assert(!status);
}

VkResult WINAPI vkGetSamplerOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkSamplerCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetSamplerOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetSamplerOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t *pValue)
{
    struct vkGetSemaphoreCounterValue_params params;
    NTSTATUS status;
    params.device = device;
    params.semaphore = semaphore;
    params.pValue = pValue;
    status = UNIX_CALL(vkGetSemaphoreCounterValue, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t *pValue)
{
    struct vkGetSemaphoreCounterValueKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.semaphore = semaphore;
    params.pValue = pValue;
    status = UNIX_CALL(vkGetSemaphoreCounterValueKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetShaderInfoAMD(VkDevice device, VkPipeline pipeline, VkShaderStageFlagBits shaderStage, VkShaderInfoTypeAMD infoType, size_t *pInfoSize, void *pInfo)
{
    struct vkGetShaderInfoAMD_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.shaderStage = shaderStage;
    params.infoType = infoType;
    params.pInfoSize = pInfoSize;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetShaderInfoAMD, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkGetShaderModuleCreateInfoIdentifierEXT(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo, VkShaderModuleIdentifierEXT *pIdentifier)
{
    struct vkGetShaderModuleCreateInfoIdentifierEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pIdentifier = pIdentifier;
    status = UNIX_CALL(vkGetShaderModuleCreateInfoIdentifierEXT, &params);
    assert(!status);
}

void WINAPI vkGetShaderModuleIdentifierEXT(VkDevice device, VkShaderModule shaderModule, VkShaderModuleIdentifierEXT *pIdentifier)
{
    struct vkGetShaderModuleIdentifierEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.shaderModule = shaderModule;
    params.pIdentifier = pIdentifier;
    status = UNIX_CALL(vkGetShaderModuleIdentifierEXT, &params);
    assert(!status);
}

VkResult WINAPI vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t *pSwapchainImageCount, VkImage *pSwapchainImages)
{
    struct vkGetSwapchainImagesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.pSwapchainImageCount = pSwapchainImageCount;
    params.pSwapchainImages = pSwapchainImages;
    status = UNIX_CALL(vkGetSwapchainImagesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkGetValidationCacheDataEXT(VkDevice device, VkValidationCacheEXT validationCache, size_t *pDataSize, void *pData)
{
    struct vkGetValidationCacheDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.validationCache = validationCache;
    params.pDataSize = pDataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetValidationCacheDataEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkInitializePerformanceApiINTEL(VkDevice device, const VkInitializePerformanceApiInfoINTEL *pInitializeInfo)
{
    struct vkInitializePerformanceApiINTEL_params params;
    NTSTATUS status;
    params.device = device;
    params.pInitializeInfo = pInitializeInfo;
    status = UNIX_CALL(vkInitializePerformanceApiINTEL, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges)
{
    struct vkInvalidateMappedMemoryRanges_params params;
    NTSTATUS status;
    params.device = device;
    params.memoryRangeCount = memoryRangeCount;
    params.pMemoryRanges = pMemoryRanges;
    status = UNIX_CALL(vkInvalidateMappedMemoryRanges, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
    struct vkMapMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    params.offset = offset;
    params.size = size;
    params.flags = flags;
    params.ppData = ppData;
    status = UNIX_CALL(vkMapMemory, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache *pSrcCaches)
{
    struct vkMergePipelineCaches_params params;
    NTSTATUS status;
    params.device = device;
    params.dstCache = dstCache;
    params.srcCacheCount = srcCacheCount;
    params.pSrcCaches = pSrcCaches;
    status = UNIX_CALL(vkMergePipelineCaches, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkMergeValidationCachesEXT(VkDevice device, VkValidationCacheEXT dstCache, uint32_t srcCacheCount, const VkValidationCacheEXT *pSrcCaches)
{
    struct vkMergeValidationCachesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.dstCache = dstCache;
    params.srcCacheCount = srcCacheCount;
    params.pSrcCaches = pSrcCaches;
    status = UNIX_CALL(vkMergeValidationCachesEXT, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT *pLabelInfo)
{
    struct vkQueueBeginDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pLabelInfo = pLabelInfo;
    status = UNIX_CALL(vkQueueBeginDebugUtilsLabelEXT, &params);
    assert(!status);
}

VkResult WINAPI vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo *pBindInfo, VkFence fence)
{
    struct vkQueueBindSparse_params params;
    NTSTATUS status;
    params.queue = queue;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfo = pBindInfo;
    params.fence = fence;
    status = UNIX_CALL(vkQueueBindSparse, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkQueueEndDebugUtilsLabelEXT(VkQueue queue)
{
    struct vkQueueEndDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.queue = queue;
    status = UNIX_CALL(vkQueueEndDebugUtilsLabelEXT, &params);
    assert(!status);
}

void WINAPI vkQueueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT *pLabelInfo)
{
    struct vkQueueInsertDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pLabelInfo = pLabelInfo;
    status = UNIX_CALL(vkQueueInsertDebugUtilsLabelEXT, &params);
    assert(!status);
}

VkResult WINAPI vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
    struct vkQueuePresentKHR_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pPresentInfo = pPresentInfo;
    status = UNIX_CALL(vkQueuePresentKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkQueueSetPerformanceConfigurationINTEL(VkQueue queue, VkPerformanceConfigurationINTEL configuration)
{
    struct vkQueueSetPerformanceConfigurationINTEL_params params;
    NTSTATUS status;
    params.queue = queue;
    params.configuration = configuration;
    status = UNIX_CALL(vkQueueSetPerformanceConfigurationINTEL, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
    struct vkQueueSubmit_params params;
    NTSTATUS status;
    params.queue = queue;
    params.submitCount = submitCount;
    params.pSubmits = pSubmits;
    params.fence = fence;
    status = UNIX_CALL(vkQueueSubmit, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence)
{
    struct vkQueueSubmit2_params params;
    NTSTATUS status;
    params.queue = queue;
    params.submitCount = submitCount;
    params.pSubmits = pSubmits;
    params.fence = fence;
    status = UNIX_CALL(vkQueueSubmit2, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence)
{
    struct vkQueueSubmit2KHR_params params;
    NTSTATUS status;
    params.queue = queue;
    params.submitCount = submitCount;
    params.pSubmits = pSubmits;
    params.fence = fence;
    status = UNIX_CALL(vkQueueSubmit2KHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkQueueWaitIdle(VkQueue queue)
{
    struct vkQueueWaitIdle_params params;
    NTSTATUS status;
    params.queue = queue;
    status = UNIX_CALL(vkQueueWaitIdle, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkReleasePerformanceConfigurationINTEL(VkDevice device, VkPerformanceConfigurationINTEL configuration)
{
    struct vkReleasePerformanceConfigurationINTEL_params params;
    NTSTATUS status;
    params.device = device;
    params.configuration = configuration;
    status = UNIX_CALL(vkReleasePerformanceConfigurationINTEL, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkReleaseProfilingLockKHR(VkDevice device)
{
    struct vkReleaseProfilingLockKHR_params params;
    NTSTATUS status;
    params.device = device;
    status = UNIX_CALL(vkReleaseProfilingLockKHR, &params);
    assert(!status);
}

VkResult WINAPI vkReleaseSwapchainImagesEXT(VkDevice device, const VkReleaseSwapchainImagesInfoEXT *pReleaseInfo)
{
    struct vkReleaseSwapchainImagesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pReleaseInfo = pReleaseInfo;
    status = UNIX_CALL(vkReleaseSwapchainImagesEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
{
    struct vkResetCommandBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.flags = flags;
    status = UNIX_CALL(vkResetCommandBuffer, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags)
{
    struct vkResetCommandPool_params params;
    NTSTATUS status;
    params.device = device;
    params.commandPool = commandPool;
    params.flags = flags;
    status = UNIX_CALL(vkResetCommandPool, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags)
{
    struct vkResetDescriptorPool_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorPool = descriptorPool;
    params.flags = flags;
    status = UNIX_CALL(vkResetDescriptorPool, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkResetEvent(VkDevice device, VkEvent event)
{
    struct vkResetEvent_params params;
    NTSTATUS status;
    params.device = device;
    params.event = event;
    status = UNIX_CALL(vkResetEvent, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences)
{
    struct vkResetFences_params params;
    NTSTATUS status;
    params.device = device;
    params.fenceCount = fenceCount;
    params.pFences = pFences;
    status = UNIX_CALL(vkResetFences, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    struct vkResetQueryPool_params params;
    NTSTATUS status;
    params.device = device;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    status = UNIX_CALL(vkResetQueryPool, &params);
    assert(!status);
}

void WINAPI vkResetQueryPoolEXT(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    struct vkResetQueryPoolEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    status = UNIX_CALL(vkResetQueryPoolEXT, &params);
    assert(!status);
}

VkResult WINAPI vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT *pNameInfo)
{
    struct vkSetDebugUtilsObjectNameEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pNameInfo = pNameInfo;
    status = UNIX_CALL(vkSetDebugUtilsObjectNameEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkSetDebugUtilsObjectTagEXT(VkDevice device, const VkDebugUtilsObjectTagInfoEXT *pTagInfo)
{
    struct vkSetDebugUtilsObjectTagEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pTagInfo = pTagInfo;
    status = UNIX_CALL(vkSetDebugUtilsObjectTagEXT, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkSetDeviceMemoryPriorityEXT(VkDevice device, VkDeviceMemory memory, float priority)
{
    struct vkSetDeviceMemoryPriorityEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    params.priority = priority;
    status = UNIX_CALL(vkSetDeviceMemoryPriorityEXT, &params);
    assert(!status);
}

VkResult WINAPI vkSetEvent(VkDevice device, VkEvent event)
{
    struct vkSetEvent_params params;
    NTSTATUS status;
    params.device = device;
    params.event = event;
    status = UNIX_CALL(vkSetEvent, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data)
{
    struct vkSetPrivateData_params params;
    NTSTATUS status;
    params.device = device;
    params.objectType = objectType;
    params.objectHandle = objectHandle;
    params.privateDataSlot = privateDataSlot;
    params.data = data;
    status = UNIX_CALL(vkSetPrivateData, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkSetPrivateDataEXT(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data)
{
    struct vkSetPrivateDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.objectType = objectType;
    params.objectHandle = objectHandle;
    params.privateDataSlot = privateDataSlot;
    params.data = data;
    status = UNIX_CALL(vkSetPrivateDataEXT, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo *pSignalInfo)
{
    struct vkSignalSemaphore_params params;
    NTSTATUS status;
    params.device = device;
    params.pSignalInfo = pSignalInfo;
    status = UNIX_CALL(vkSignalSemaphore, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo *pSignalInfo)
{
    struct vkSignalSemaphoreKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pSignalInfo = pSignalInfo;
    status = UNIX_CALL(vkSignalSemaphoreKHR, &params);
    assert(!status);
    return params.result;
}

void WINAPI vkSubmitDebugUtilsMessageEXT(VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
{
    struct vkSubmitDebugUtilsMessageEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.messageSeverity = messageSeverity;
    params.messageTypes = messageTypes;
    params.pCallbackData = pCallbackData;
    status = UNIX_CALL(vkSubmitDebugUtilsMessageEXT, &params);
    assert(!status);
}

void WINAPI vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags)
{
    struct vkTrimCommandPool_params params;
    NTSTATUS status;
    params.device = device;
    params.commandPool = commandPool;
    params.flags = flags;
    status = UNIX_CALL(vkTrimCommandPool, &params);
    assert(!status);
}

void WINAPI vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags)
{
    struct vkTrimCommandPoolKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.commandPool = commandPool;
    params.flags = flags;
    status = UNIX_CALL(vkTrimCommandPoolKHR, &params);
    assert(!status);
}

void WINAPI vkUninitializePerformanceApiINTEL(VkDevice device)
{
    struct vkUninitializePerformanceApiINTEL_params params;
    NTSTATUS status;
    params.device = device;
    status = UNIX_CALL(vkUninitializePerformanceApiINTEL, &params);
    assert(!status);
}

void WINAPI vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
    struct vkUnmapMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    status = UNIX_CALL(vkUnmapMemory, &params);
    assert(!status);
}

void WINAPI vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
    struct vkUpdateDescriptorSetWithTemplate_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorSet = descriptorSet;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.pData = pData;
    status = UNIX_CALL(vkUpdateDescriptorSetWithTemplate, &params);
    assert(!status);
}

void WINAPI vkUpdateDescriptorSetWithTemplateKHR(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
    struct vkUpdateDescriptorSetWithTemplateKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorSet = descriptorSet;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.pData = pData;
    status = UNIX_CALL(vkUpdateDescriptorSetWithTemplateKHR, &params);
    assert(!status);
}

void WINAPI vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet *pDescriptorCopies)
{
    struct vkUpdateDescriptorSets_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorWriteCount = descriptorWriteCount;
    params.pDescriptorWrites = pDescriptorWrites;
    params.descriptorCopyCount = descriptorCopyCount;
    params.pDescriptorCopies = pDescriptorCopies;
    status = UNIX_CALL(vkUpdateDescriptorSets, &params);
    assert(!status);
}

VkResult WINAPI vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences, VkBool32 waitAll, uint64_t timeout)
{
    struct vkWaitForFences_params params;
    NTSTATUS status;
    params.device = device;
    params.fenceCount = fenceCount;
    params.pFences = pFences;
    params.waitAll = waitAll;
    params.timeout = timeout;
    status = UNIX_CALL(vkWaitForFences, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkWaitForPresentKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t presentId, uint64_t timeout)
{
    struct vkWaitForPresentKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.presentId = presentId;
    params.timeout = timeout;
    status = UNIX_CALL(vkWaitForPresentKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo, uint64_t timeout)
{
    struct vkWaitSemaphores_params params;
    NTSTATUS status;
    params.device = device;
    params.pWaitInfo = pWaitInfo;
    params.timeout = timeout;
    status = UNIX_CALL(vkWaitSemaphores, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo, uint64_t timeout)
{
    struct vkWaitSemaphoresKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pWaitInfo = pWaitInfo;
    params.timeout = timeout;
    status = UNIX_CALL(vkWaitSemaphoresKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkWriteAccelerationStructuresPropertiesKHR(VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType, size_t dataSize, void *pData, size_t stride)
{
    struct vkWriteAccelerationStructuresPropertiesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.accelerationStructureCount = accelerationStructureCount;
    params.pAccelerationStructures = pAccelerationStructures;
    params.queryType = queryType;
    params.dataSize = dataSize;
    params.pData = pData;
    params.stride = stride;
    status = UNIX_CALL(vkWriteAccelerationStructuresPropertiesKHR, &params);
    assert(!status);
    return params.result;
}

VkResult WINAPI vkWriteMicromapsPropertiesEXT(VkDevice device, uint32_t micromapCount, const VkMicromapEXT *pMicromaps, VkQueryType queryType, size_t dataSize, void *pData, size_t stride)
{
    struct vkWriteMicromapsPropertiesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.micromapCount = micromapCount;
    params.pMicromaps = pMicromaps;
    params.queryType = queryType;
    params.dataSize = dataSize;
    params.pData = pData;
    params.stride = stride;
    status = UNIX_CALL(vkWriteMicromapsPropertiesEXT, &params);
    assert(!status);
    return params.result;
}

static const struct vulkan_func vk_device_dispatch_table[] =
{
    {"vkAcquireNextImage2KHR", vkAcquireNextImage2KHR},
    {"vkAcquireNextImageKHR", vkAcquireNextImageKHR},
    {"vkAcquirePerformanceConfigurationINTEL", vkAcquirePerformanceConfigurationINTEL},
    {"vkAcquireProfilingLockKHR", vkAcquireProfilingLockKHR},
    {"vkAllocateCommandBuffers", vkAllocateCommandBuffers},
    {"vkAllocateDescriptorSets", vkAllocateDescriptorSets},
    {"vkAllocateMemory", vkAllocateMemory},
    {"vkBeginCommandBuffer", vkBeginCommandBuffer},
    {"vkBindAccelerationStructureMemoryNV", vkBindAccelerationStructureMemoryNV},
    {"vkBindBufferMemory", vkBindBufferMemory},
    {"vkBindBufferMemory2", vkBindBufferMemory2},
    {"vkBindBufferMemory2KHR", vkBindBufferMemory2KHR},
    {"vkBindImageMemory", vkBindImageMemory},
    {"vkBindImageMemory2", vkBindImageMemory2},
    {"vkBindImageMemory2KHR", vkBindImageMemory2KHR},
    {"vkBindOpticalFlowSessionImageNV", vkBindOpticalFlowSessionImageNV},
    {"vkBuildAccelerationStructuresKHR", vkBuildAccelerationStructuresKHR},
    {"vkBuildMicromapsEXT", vkBuildMicromapsEXT},
    {"vkCmdBeginConditionalRenderingEXT", vkCmdBeginConditionalRenderingEXT},
    {"vkCmdBeginDebugUtilsLabelEXT", vkCmdBeginDebugUtilsLabelEXT},
    {"vkCmdBeginQuery", vkCmdBeginQuery},
    {"vkCmdBeginQueryIndexedEXT", vkCmdBeginQueryIndexedEXT},
    {"vkCmdBeginRenderPass", vkCmdBeginRenderPass},
    {"vkCmdBeginRenderPass2", vkCmdBeginRenderPass2},
    {"vkCmdBeginRenderPass2KHR", vkCmdBeginRenderPass2KHR},
    {"vkCmdBeginRendering", vkCmdBeginRendering},
    {"vkCmdBeginRenderingKHR", vkCmdBeginRenderingKHR},
    {"vkCmdBeginTransformFeedbackEXT", vkCmdBeginTransformFeedbackEXT},
    {"vkCmdBindDescriptorBufferEmbeddedSamplersEXT", vkCmdBindDescriptorBufferEmbeddedSamplersEXT},
    {"vkCmdBindDescriptorBuffersEXT", vkCmdBindDescriptorBuffersEXT},
    {"vkCmdBindDescriptorSets", vkCmdBindDescriptorSets},
    {"vkCmdBindIndexBuffer", vkCmdBindIndexBuffer},
    {"vkCmdBindInvocationMaskHUAWEI", vkCmdBindInvocationMaskHUAWEI},
    {"vkCmdBindPipeline", vkCmdBindPipeline},
    {"vkCmdBindPipelineShaderGroupNV", vkCmdBindPipelineShaderGroupNV},
    {"vkCmdBindShadingRateImageNV", vkCmdBindShadingRateImageNV},
    {"vkCmdBindTransformFeedbackBuffersEXT", vkCmdBindTransformFeedbackBuffersEXT},
    {"vkCmdBindVertexBuffers", vkCmdBindVertexBuffers},
    {"vkCmdBindVertexBuffers2", vkCmdBindVertexBuffers2},
    {"vkCmdBindVertexBuffers2EXT", vkCmdBindVertexBuffers2EXT},
    {"vkCmdBlitImage", vkCmdBlitImage},
    {"vkCmdBlitImage2", vkCmdBlitImage2},
    {"vkCmdBlitImage2KHR", vkCmdBlitImage2KHR},
    {"vkCmdBuildAccelerationStructureNV", vkCmdBuildAccelerationStructureNV},
    {"vkCmdBuildAccelerationStructuresIndirectKHR", vkCmdBuildAccelerationStructuresIndirectKHR},
    {"vkCmdBuildAccelerationStructuresKHR", vkCmdBuildAccelerationStructuresKHR},
    {"vkCmdBuildMicromapsEXT", vkCmdBuildMicromapsEXT},
    {"vkCmdClearAttachments", vkCmdClearAttachments},
    {"vkCmdClearColorImage", vkCmdClearColorImage},
    {"vkCmdClearDepthStencilImage", vkCmdClearDepthStencilImage},
    {"vkCmdCopyAccelerationStructureKHR", vkCmdCopyAccelerationStructureKHR},
    {"vkCmdCopyAccelerationStructureNV", vkCmdCopyAccelerationStructureNV},
    {"vkCmdCopyAccelerationStructureToMemoryKHR", vkCmdCopyAccelerationStructureToMemoryKHR},
    {"vkCmdCopyBuffer", vkCmdCopyBuffer},
    {"vkCmdCopyBuffer2", vkCmdCopyBuffer2},
    {"vkCmdCopyBuffer2KHR", vkCmdCopyBuffer2KHR},
    {"vkCmdCopyBufferToImage", vkCmdCopyBufferToImage},
    {"vkCmdCopyBufferToImage2", vkCmdCopyBufferToImage2},
    {"vkCmdCopyBufferToImage2KHR", vkCmdCopyBufferToImage2KHR},
    {"vkCmdCopyImage", vkCmdCopyImage},
    {"vkCmdCopyImage2", vkCmdCopyImage2},
    {"vkCmdCopyImage2KHR", vkCmdCopyImage2KHR},
    {"vkCmdCopyImageToBuffer", vkCmdCopyImageToBuffer},
    {"vkCmdCopyImageToBuffer2", vkCmdCopyImageToBuffer2},
    {"vkCmdCopyImageToBuffer2KHR", vkCmdCopyImageToBuffer2KHR},
    {"vkCmdCopyMemoryIndirectNV", vkCmdCopyMemoryIndirectNV},
    {"vkCmdCopyMemoryToAccelerationStructureKHR", vkCmdCopyMemoryToAccelerationStructureKHR},
    {"vkCmdCopyMemoryToImageIndirectNV", vkCmdCopyMemoryToImageIndirectNV},
    {"vkCmdCopyMemoryToMicromapEXT", vkCmdCopyMemoryToMicromapEXT},
    {"vkCmdCopyMicromapEXT", vkCmdCopyMicromapEXT},
    {"vkCmdCopyMicromapToMemoryEXT", vkCmdCopyMicromapToMemoryEXT},
    {"vkCmdCopyQueryPoolResults", vkCmdCopyQueryPoolResults},
    {"vkCmdCuLaunchKernelNVX", vkCmdCuLaunchKernelNVX},
    {"vkCmdDebugMarkerBeginEXT", vkCmdDebugMarkerBeginEXT},
    {"vkCmdDebugMarkerEndEXT", vkCmdDebugMarkerEndEXT},
    {"vkCmdDebugMarkerInsertEXT", vkCmdDebugMarkerInsertEXT},
    {"vkCmdDecompressMemoryIndirectCountNV", vkCmdDecompressMemoryIndirectCountNV},
    {"vkCmdDecompressMemoryNV", vkCmdDecompressMemoryNV},
    {"vkCmdDispatch", vkCmdDispatch},
    {"vkCmdDispatchBase", vkCmdDispatchBase},
    {"vkCmdDispatchBaseKHR", vkCmdDispatchBaseKHR},
    {"vkCmdDispatchIndirect", vkCmdDispatchIndirect},
    {"vkCmdDraw", vkCmdDraw},
    {"vkCmdDrawIndexed", vkCmdDrawIndexed},
    {"vkCmdDrawIndexedIndirect", vkCmdDrawIndexedIndirect},
    {"vkCmdDrawIndexedIndirectCount", vkCmdDrawIndexedIndirectCount},
    {"vkCmdDrawIndexedIndirectCountAMD", vkCmdDrawIndexedIndirectCountAMD},
    {"vkCmdDrawIndexedIndirectCountKHR", vkCmdDrawIndexedIndirectCountKHR},
    {"vkCmdDrawIndirect", vkCmdDrawIndirect},
    {"vkCmdDrawIndirectByteCountEXT", vkCmdDrawIndirectByteCountEXT},
    {"vkCmdDrawIndirectCount", vkCmdDrawIndirectCount},
    {"vkCmdDrawIndirectCountAMD", vkCmdDrawIndirectCountAMD},
    {"vkCmdDrawIndirectCountKHR", vkCmdDrawIndirectCountKHR},
    {"vkCmdDrawMeshTasksEXT", vkCmdDrawMeshTasksEXT},
    {"vkCmdDrawMeshTasksIndirectCountEXT", vkCmdDrawMeshTasksIndirectCountEXT},
    {"vkCmdDrawMeshTasksIndirectCountNV", vkCmdDrawMeshTasksIndirectCountNV},
    {"vkCmdDrawMeshTasksIndirectEXT", vkCmdDrawMeshTasksIndirectEXT},
    {"vkCmdDrawMeshTasksIndirectNV", vkCmdDrawMeshTasksIndirectNV},
    {"vkCmdDrawMeshTasksNV", vkCmdDrawMeshTasksNV},
    {"vkCmdDrawMultiEXT", vkCmdDrawMultiEXT},
    {"vkCmdDrawMultiIndexedEXT", vkCmdDrawMultiIndexedEXT},
    {"vkCmdEndConditionalRenderingEXT", vkCmdEndConditionalRenderingEXT},
    {"vkCmdEndDebugUtilsLabelEXT", vkCmdEndDebugUtilsLabelEXT},
    {"vkCmdEndQuery", vkCmdEndQuery},
    {"vkCmdEndQueryIndexedEXT", vkCmdEndQueryIndexedEXT},
    {"vkCmdEndRenderPass", vkCmdEndRenderPass},
    {"vkCmdEndRenderPass2", vkCmdEndRenderPass2},
    {"vkCmdEndRenderPass2KHR", vkCmdEndRenderPass2KHR},
    {"vkCmdEndRendering", vkCmdEndRendering},
    {"vkCmdEndRenderingKHR", vkCmdEndRenderingKHR},
    {"vkCmdEndTransformFeedbackEXT", vkCmdEndTransformFeedbackEXT},
    {"vkCmdExecuteCommands", vkCmdExecuteCommands},
    {"vkCmdExecuteGeneratedCommandsNV", vkCmdExecuteGeneratedCommandsNV},
    {"vkCmdFillBuffer", vkCmdFillBuffer},
    {"vkCmdInsertDebugUtilsLabelEXT", vkCmdInsertDebugUtilsLabelEXT},
    {"vkCmdNextSubpass", vkCmdNextSubpass},
    {"vkCmdNextSubpass2", vkCmdNextSubpass2},
    {"vkCmdNextSubpass2KHR", vkCmdNextSubpass2KHR},
    {"vkCmdOpticalFlowExecuteNV", vkCmdOpticalFlowExecuteNV},
    {"vkCmdPipelineBarrier", vkCmdPipelineBarrier},
    {"vkCmdPipelineBarrier2", vkCmdPipelineBarrier2},
    {"vkCmdPipelineBarrier2KHR", vkCmdPipelineBarrier2KHR},
    {"vkCmdPreprocessGeneratedCommandsNV", vkCmdPreprocessGeneratedCommandsNV},
    {"vkCmdPushConstants", vkCmdPushConstants},
    {"vkCmdPushDescriptorSetKHR", vkCmdPushDescriptorSetKHR},
    {"vkCmdPushDescriptorSetWithTemplateKHR", vkCmdPushDescriptorSetWithTemplateKHR},
    {"vkCmdResetEvent", vkCmdResetEvent},
    {"vkCmdResetEvent2", vkCmdResetEvent2},
    {"vkCmdResetEvent2KHR", vkCmdResetEvent2KHR},
    {"vkCmdResetQueryPool", vkCmdResetQueryPool},
    {"vkCmdResolveImage", vkCmdResolveImage},
    {"vkCmdResolveImage2", vkCmdResolveImage2},
    {"vkCmdResolveImage2KHR", vkCmdResolveImage2KHR},
    {"vkCmdSetAlphaToCoverageEnableEXT", vkCmdSetAlphaToCoverageEnableEXT},
    {"vkCmdSetAlphaToOneEnableEXT", vkCmdSetAlphaToOneEnableEXT},
    {"vkCmdSetBlendConstants", vkCmdSetBlendConstants},
    {"vkCmdSetCheckpointNV", vkCmdSetCheckpointNV},
    {"vkCmdSetCoarseSampleOrderNV", vkCmdSetCoarseSampleOrderNV},
    {"vkCmdSetColorBlendAdvancedEXT", vkCmdSetColorBlendAdvancedEXT},
    {"vkCmdSetColorBlendEnableEXT", vkCmdSetColorBlendEnableEXT},
    {"vkCmdSetColorBlendEquationEXT", vkCmdSetColorBlendEquationEXT},
    {"vkCmdSetColorWriteEnableEXT", vkCmdSetColorWriteEnableEXT},
    {"vkCmdSetColorWriteMaskEXT", vkCmdSetColorWriteMaskEXT},
    {"vkCmdSetConservativeRasterizationModeEXT", vkCmdSetConservativeRasterizationModeEXT},
    {"vkCmdSetCoverageModulationModeNV", vkCmdSetCoverageModulationModeNV},
    {"vkCmdSetCoverageModulationTableEnableNV", vkCmdSetCoverageModulationTableEnableNV},
    {"vkCmdSetCoverageModulationTableNV", vkCmdSetCoverageModulationTableNV},
    {"vkCmdSetCoverageReductionModeNV", vkCmdSetCoverageReductionModeNV},
    {"vkCmdSetCoverageToColorEnableNV", vkCmdSetCoverageToColorEnableNV},
    {"vkCmdSetCoverageToColorLocationNV", vkCmdSetCoverageToColorLocationNV},
    {"vkCmdSetCullMode", vkCmdSetCullMode},
    {"vkCmdSetCullModeEXT", vkCmdSetCullModeEXT},
    {"vkCmdSetDepthBias", vkCmdSetDepthBias},
    {"vkCmdSetDepthBiasEnable", vkCmdSetDepthBiasEnable},
    {"vkCmdSetDepthBiasEnableEXT", vkCmdSetDepthBiasEnableEXT},
    {"vkCmdSetDepthBounds", vkCmdSetDepthBounds},
    {"vkCmdSetDepthBoundsTestEnable", vkCmdSetDepthBoundsTestEnable},
    {"vkCmdSetDepthBoundsTestEnableEXT", vkCmdSetDepthBoundsTestEnableEXT},
    {"vkCmdSetDepthClampEnableEXT", vkCmdSetDepthClampEnableEXT},
    {"vkCmdSetDepthClipEnableEXT", vkCmdSetDepthClipEnableEXT},
    {"vkCmdSetDepthClipNegativeOneToOneEXT", vkCmdSetDepthClipNegativeOneToOneEXT},
    {"vkCmdSetDepthCompareOp", vkCmdSetDepthCompareOp},
    {"vkCmdSetDepthCompareOpEXT", vkCmdSetDepthCompareOpEXT},
    {"vkCmdSetDepthTestEnable", vkCmdSetDepthTestEnable},
    {"vkCmdSetDepthTestEnableEXT", vkCmdSetDepthTestEnableEXT},
    {"vkCmdSetDepthWriteEnable", vkCmdSetDepthWriteEnable},
    {"vkCmdSetDepthWriteEnableEXT", vkCmdSetDepthWriteEnableEXT},
    {"vkCmdSetDescriptorBufferOffsetsEXT", vkCmdSetDescriptorBufferOffsetsEXT},
    {"vkCmdSetDeviceMask", vkCmdSetDeviceMask},
    {"vkCmdSetDeviceMaskKHR", vkCmdSetDeviceMaskKHR},
    {"vkCmdSetDiscardRectangleEXT", vkCmdSetDiscardRectangleEXT},
    {"vkCmdSetEvent", vkCmdSetEvent},
    {"vkCmdSetEvent2", vkCmdSetEvent2},
    {"vkCmdSetEvent2KHR", vkCmdSetEvent2KHR},
    {"vkCmdSetExclusiveScissorNV", vkCmdSetExclusiveScissorNV},
    {"vkCmdSetExtraPrimitiveOverestimationSizeEXT", vkCmdSetExtraPrimitiveOverestimationSizeEXT},
    {"vkCmdSetFragmentShadingRateEnumNV", vkCmdSetFragmentShadingRateEnumNV},
    {"vkCmdSetFragmentShadingRateKHR", vkCmdSetFragmentShadingRateKHR},
    {"vkCmdSetFrontFace", vkCmdSetFrontFace},
    {"vkCmdSetFrontFaceEXT", vkCmdSetFrontFaceEXT},
    {"vkCmdSetLineRasterizationModeEXT", vkCmdSetLineRasterizationModeEXT},
    {"vkCmdSetLineStippleEXT", vkCmdSetLineStippleEXT},
    {"vkCmdSetLineStippleEnableEXT", vkCmdSetLineStippleEnableEXT},
    {"vkCmdSetLineWidth", vkCmdSetLineWidth},
    {"vkCmdSetLogicOpEXT", vkCmdSetLogicOpEXT},
    {"vkCmdSetLogicOpEnableEXT", vkCmdSetLogicOpEnableEXT},
    {"vkCmdSetPatchControlPointsEXT", vkCmdSetPatchControlPointsEXT},
    {"vkCmdSetPerformanceMarkerINTEL", vkCmdSetPerformanceMarkerINTEL},
    {"vkCmdSetPerformanceOverrideINTEL", vkCmdSetPerformanceOverrideINTEL},
    {"vkCmdSetPerformanceStreamMarkerINTEL", vkCmdSetPerformanceStreamMarkerINTEL},
    {"vkCmdSetPolygonModeEXT", vkCmdSetPolygonModeEXT},
    {"vkCmdSetPrimitiveRestartEnable", vkCmdSetPrimitiveRestartEnable},
    {"vkCmdSetPrimitiveRestartEnableEXT", vkCmdSetPrimitiveRestartEnableEXT},
    {"vkCmdSetPrimitiveTopology", vkCmdSetPrimitiveTopology},
    {"vkCmdSetPrimitiveTopologyEXT", vkCmdSetPrimitiveTopologyEXT},
    {"vkCmdSetProvokingVertexModeEXT", vkCmdSetProvokingVertexModeEXT},
    {"vkCmdSetRasterizationSamplesEXT", vkCmdSetRasterizationSamplesEXT},
    {"vkCmdSetRasterizationStreamEXT", vkCmdSetRasterizationStreamEXT},
    {"vkCmdSetRasterizerDiscardEnable", vkCmdSetRasterizerDiscardEnable},
    {"vkCmdSetRasterizerDiscardEnableEXT", vkCmdSetRasterizerDiscardEnableEXT},
    {"vkCmdSetRayTracingPipelineStackSizeKHR", vkCmdSetRayTracingPipelineStackSizeKHR},
    {"vkCmdSetRepresentativeFragmentTestEnableNV", vkCmdSetRepresentativeFragmentTestEnableNV},
    {"vkCmdSetSampleLocationsEXT", vkCmdSetSampleLocationsEXT},
    {"vkCmdSetSampleLocationsEnableEXT", vkCmdSetSampleLocationsEnableEXT},
    {"vkCmdSetSampleMaskEXT", vkCmdSetSampleMaskEXT},
    {"vkCmdSetScissor", vkCmdSetScissor},
    {"vkCmdSetScissorWithCount", vkCmdSetScissorWithCount},
    {"vkCmdSetScissorWithCountEXT", vkCmdSetScissorWithCountEXT},
    {"vkCmdSetShadingRateImageEnableNV", vkCmdSetShadingRateImageEnableNV},
    {"vkCmdSetStencilCompareMask", vkCmdSetStencilCompareMask},
    {"vkCmdSetStencilOp", vkCmdSetStencilOp},
    {"vkCmdSetStencilOpEXT", vkCmdSetStencilOpEXT},
    {"vkCmdSetStencilReference", vkCmdSetStencilReference},
    {"vkCmdSetStencilTestEnable", vkCmdSetStencilTestEnable},
    {"vkCmdSetStencilTestEnableEXT", vkCmdSetStencilTestEnableEXT},
    {"vkCmdSetStencilWriteMask", vkCmdSetStencilWriteMask},
    {"vkCmdSetTessellationDomainOriginEXT", vkCmdSetTessellationDomainOriginEXT},
    {"vkCmdSetVertexInputEXT", vkCmdSetVertexInputEXT},
    {"vkCmdSetViewport", vkCmdSetViewport},
    {"vkCmdSetViewportShadingRatePaletteNV", vkCmdSetViewportShadingRatePaletteNV},
    {"vkCmdSetViewportSwizzleNV", vkCmdSetViewportSwizzleNV},
    {"vkCmdSetViewportWScalingEnableNV", vkCmdSetViewportWScalingEnableNV},
    {"vkCmdSetViewportWScalingNV", vkCmdSetViewportWScalingNV},
    {"vkCmdSetViewportWithCount", vkCmdSetViewportWithCount},
    {"vkCmdSetViewportWithCountEXT", vkCmdSetViewportWithCountEXT},
    {"vkCmdSubpassShadingHUAWEI", vkCmdSubpassShadingHUAWEI},
    {"vkCmdTraceRaysIndirect2KHR", vkCmdTraceRaysIndirect2KHR},
    {"vkCmdTraceRaysIndirectKHR", vkCmdTraceRaysIndirectKHR},
    {"vkCmdTraceRaysKHR", vkCmdTraceRaysKHR},
    {"vkCmdTraceRaysNV", vkCmdTraceRaysNV},
    {"vkCmdUpdateBuffer", vkCmdUpdateBuffer},
    {"vkCmdWaitEvents", vkCmdWaitEvents},
    {"vkCmdWaitEvents2", vkCmdWaitEvents2},
    {"vkCmdWaitEvents2KHR", vkCmdWaitEvents2KHR},
    {"vkCmdWriteAccelerationStructuresPropertiesKHR", vkCmdWriteAccelerationStructuresPropertiesKHR},
    {"vkCmdWriteAccelerationStructuresPropertiesNV", vkCmdWriteAccelerationStructuresPropertiesNV},
    {"vkCmdWriteBufferMarker2AMD", vkCmdWriteBufferMarker2AMD},
    {"vkCmdWriteBufferMarkerAMD", vkCmdWriteBufferMarkerAMD},
    {"vkCmdWriteMicromapsPropertiesEXT", vkCmdWriteMicromapsPropertiesEXT},
    {"vkCmdWriteTimestamp", vkCmdWriteTimestamp},
    {"vkCmdWriteTimestamp2", vkCmdWriteTimestamp2},
    {"vkCmdWriteTimestamp2KHR", vkCmdWriteTimestamp2KHR},
    {"vkCompileDeferredNV", vkCompileDeferredNV},
    {"vkCopyAccelerationStructureKHR", vkCopyAccelerationStructureKHR},
    {"vkCopyAccelerationStructureToMemoryKHR", vkCopyAccelerationStructureToMemoryKHR},
    {"vkCopyMemoryToAccelerationStructureKHR", vkCopyMemoryToAccelerationStructureKHR},
    {"vkCopyMemoryToMicromapEXT", vkCopyMemoryToMicromapEXT},
    {"vkCopyMicromapEXT", vkCopyMicromapEXT},
    {"vkCopyMicromapToMemoryEXT", vkCopyMicromapToMemoryEXT},
    {"vkCreateAccelerationStructureKHR", vkCreateAccelerationStructureKHR},
    {"vkCreateAccelerationStructureNV", vkCreateAccelerationStructureNV},
    {"vkCreateBuffer", vkCreateBuffer},
    {"vkCreateBufferView", vkCreateBufferView},
    {"vkCreateCommandPool", vkCreateCommandPool},
    {"vkCreateComputePipelines", vkCreateComputePipelines},
    {"vkCreateCuFunctionNVX", vkCreateCuFunctionNVX},
    {"vkCreateCuModuleNVX", vkCreateCuModuleNVX},
    {"vkCreateDeferredOperationKHR", vkCreateDeferredOperationKHR},
    {"vkCreateDescriptorPool", vkCreateDescriptorPool},
    {"vkCreateDescriptorSetLayout", vkCreateDescriptorSetLayout},
    {"vkCreateDescriptorUpdateTemplate", vkCreateDescriptorUpdateTemplate},
    {"vkCreateDescriptorUpdateTemplateKHR", vkCreateDescriptorUpdateTemplateKHR},
    {"vkCreateEvent", vkCreateEvent},
    {"vkCreateFence", vkCreateFence},
    {"vkCreateFramebuffer", vkCreateFramebuffer},
    {"vkCreateGraphicsPipelines", vkCreateGraphicsPipelines},
    {"vkCreateImage", vkCreateImage},
    {"vkCreateImageView", vkCreateImageView},
    {"vkCreateIndirectCommandsLayoutNV", vkCreateIndirectCommandsLayoutNV},
    {"vkCreateMicromapEXT", vkCreateMicromapEXT},
    {"vkCreateOpticalFlowSessionNV", vkCreateOpticalFlowSessionNV},
    {"vkCreatePipelineCache", vkCreatePipelineCache},
    {"vkCreatePipelineLayout", vkCreatePipelineLayout},
    {"vkCreatePrivateDataSlot", vkCreatePrivateDataSlot},
    {"vkCreatePrivateDataSlotEXT", vkCreatePrivateDataSlotEXT},
    {"vkCreateQueryPool", vkCreateQueryPool},
    {"vkCreateRayTracingPipelinesKHR", vkCreateRayTracingPipelinesKHR},
    {"vkCreateRayTracingPipelinesNV", vkCreateRayTracingPipelinesNV},
    {"vkCreateRenderPass", vkCreateRenderPass},
    {"vkCreateRenderPass2", vkCreateRenderPass2},
    {"vkCreateRenderPass2KHR", vkCreateRenderPass2KHR},
    {"vkCreateSampler", vkCreateSampler},
    {"vkCreateSamplerYcbcrConversion", vkCreateSamplerYcbcrConversion},
    {"vkCreateSamplerYcbcrConversionKHR", vkCreateSamplerYcbcrConversionKHR},
    {"vkCreateSemaphore", vkCreateSemaphore},
    {"vkCreateShaderModule", vkCreateShaderModule},
    {"vkCreateSwapchainKHR", vkCreateSwapchainKHR},
    {"vkCreateValidationCacheEXT", vkCreateValidationCacheEXT},
    {"vkDebugMarkerSetObjectNameEXT", vkDebugMarkerSetObjectNameEXT},
    {"vkDebugMarkerSetObjectTagEXT", vkDebugMarkerSetObjectTagEXT},
    {"vkDeferredOperationJoinKHR", vkDeferredOperationJoinKHR},
    {"vkDestroyAccelerationStructureKHR", vkDestroyAccelerationStructureKHR},
    {"vkDestroyAccelerationStructureNV", vkDestroyAccelerationStructureNV},
    {"vkDestroyBuffer", vkDestroyBuffer},
    {"vkDestroyBufferView", vkDestroyBufferView},
    {"vkDestroyCommandPool", vkDestroyCommandPool},
    {"vkDestroyCuFunctionNVX", vkDestroyCuFunctionNVX},
    {"vkDestroyCuModuleNVX", vkDestroyCuModuleNVX},
    {"vkDestroyDeferredOperationKHR", vkDestroyDeferredOperationKHR},
    {"vkDestroyDescriptorPool", vkDestroyDescriptorPool},
    {"vkDestroyDescriptorSetLayout", vkDestroyDescriptorSetLayout},
    {"vkDestroyDescriptorUpdateTemplate", vkDestroyDescriptorUpdateTemplate},
    {"vkDestroyDescriptorUpdateTemplateKHR", vkDestroyDescriptorUpdateTemplateKHR},
    {"vkDestroyDevice", vkDestroyDevice},
    {"vkDestroyEvent", vkDestroyEvent},
    {"vkDestroyFence", vkDestroyFence},
    {"vkDestroyFramebuffer", vkDestroyFramebuffer},
    {"vkDestroyImage", vkDestroyImage},
    {"vkDestroyImageView", vkDestroyImageView},
    {"vkDestroyIndirectCommandsLayoutNV", vkDestroyIndirectCommandsLayoutNV},
    {"vkDestroyMicromapEXT", vkDestroyMicromapEXT},
    {"vkDestroyOpticalFlowSessionNV", vkDestroyOpticalFlowSessionNV},
    {"vkDestroyPipeline", vkDestroyPipeline},
    {"vkDestroyPipelineCache", vkDestroyPipelineCache},
    {"vkDestroyPipelineLayout", vkDestroyPipelineLayout},
    {"vkDestroyPrivateDataSlot", vkDestroyPrivateDataSlot},
    {"vkDestroyPrivateDataSlotEXT", vkDestroyPrivateDataSlotEXT},
    {"vkDestroyQueryPool", vkDestroyQueryPool},
    {"vkDestroyRenderPass", vkDestroyRenderPass},
    {"vkDestroySampler", vkDestroySampler},
    {"vkDestroySamplerYcbcrConversion", vkDestroySamplerYcbcrConversion},
    {"vkDestroySamplerYcbcrConversionKHR", vkDestroySamplerYcbcrConversionKHR},
    {"vkDestroySemaphore", vkDestroySemaphore},
    {"vkDestroyShaderModule", vkDestroyShaderModule},
    {"vkDestroySwapchainKHR", vkDestroySwapchainKHR},
    {"vkDestroyValidationCacheEXT", vkDestroyValidationCacheEXT},
    {"vkDeviceWaitIdle", vkDeviceWaitIdle},
    {"vkEndCommandBuffer", vkEndCommandBuffer},
    {"vkFlushMappedMemoryRanges", vkFlushMappedMemoryRanges},
    {"vkFreeCommandBuffers", vkFreeCommandBuffers},
    {"vkFreeDescriptorSets", vkFreeDescriptorSets},
    {"vkFreeMemory", vkFreeMemory},
    {"vkGetAccelerationStructureBuildSizesKHR", vkGetAccelerationStructureBuildSizesKHR},
    {"vkGetAccelerationStructureDeviceAddressKHR", vkGetAccelerationStructureDeviceAddressKHR},
    {"vkGetAccelerationStructureHandleNV", vkGetAccelerationStructureHandleNV},
    {"vkGetAccelerationStructureMemoryRequirementsNV", vkGetAccelerationStructureMemoryRequirementsNV},
    {"vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT", vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT},
    {"vkGetBufferDeviceAddress", vkGetBufferDeviceAddress},
    {"vkGetBufferDeviceAddressEXT", vkGetBufferDeviceAddressEXT},
    {"vkGetBufferDeviceAddressKHR", vkGetBufferDeviceAddressKHR},
    {"vkGetBufferMemoryRequirements", vkGetBufferMemoryRequirements},
    {"vkGetBufferMemoryRequirements2", vkGetBufferMemoryRequirements2},
    {"vkGetBufferMemoryRequirements2KHR", vkGetBufferMemoryRequirements2KHR},
    {"vkGetBufferOpaqueCaptureAddress", vkGetBufferOpaqueCaptureAddress},
    {"vkGetBufferOpaqueCaptureAddressKHR", vkGetBufferOpaqueCaptureAddressKHR},
    {"vkGetBufferOpaqueCaptureDescriptorDataEXT", vkGetBufferOpaqueCaptureDescriptorDataEXT},
    {"vkGetCalibratedTimestampsEXT", vkGetCalibratedTimestampsEXT},
    {"vkGetDeferredOperationMaxConcurrencyKHR", vkGetDeferredOperationMaxConcurrencyKHR},
    {"vkGetDeferredOperationResultKHR", vkGetDeferredOperationResultKHR},
    {"vkGetDescriptorEXT", vkGetDescriptorEXT},
    {"vkGetDescriptorSetHostMappingVALVE", vkGetDescriptorSetHostMappingVALVE},
    {"vkGetDescriptorSetLayoutBindingOffsetEXT", vkGetDescriptorSetLayoutBindingOffsetEXT},
    {"vkGetDescriptorSetLayoutHostMappingInfoVALVE", vkGetDescriptorSetLayoutHostMappingInfoVALVE},
    {"vkGetDescriptorSetLayoutSizeEXT", vkGetDescriptorSetLayoutSizeEXT},
    {"vkGetDescriptorSetLayoutSupport", vkGetDescriptorSetLayoutSupport},
    {"vkGetDescriptorSetLayoutSupportKHR", vkGetDescriptorSetLayoutSupportKHR},
    {"vkGetDeviceAccelerationStructureCompatibilityKHR", vkGetDeviceAccelerationStructureCompatibilityKHR},
    {"vkGetDeviceBufferMemoryRequirements", vkGetDeviceBufferMemoryRequirements},
    {"vkGetDeviceBufferMemoryRequirementsKHR", vkGetDeviceBufferMemoryRequirementsKHR},
    {"vkGetDeviceFaultInfoEXT", vkGetDeviceFaultInfoEXT},
    {"vkGetDeviceGroupPeerMemoryFeatures", vkGetDeviceGroupPeerMemoryFeatures},
    {"vkGetDeviceGroupPeerMemoryFeaturesKHR", vkGetDeviceGroupPeerMemoryFeaturesKHR},
    {"vkGetDeviceGroupPresentCapabilitiesKHR", vkGetDeviceGroupPresentCapabilitiesKHR},
    {"vkGetDeviceGroupSurfacePresentModesKHR", vkGetDeviceGroupSurfacePresentModesKHR},
    {"vkGetDeviceImageMemoryRequirements", vkGetDeviceImageMemoryRequirements},
    {"vkGetDeviceImageMemoryRequirementsKHR", vkGetDeviceImageMemoryRequirementsKHR},
    {"vkGetDeviceImageSparseMemoryRequirements", vkGetDeviceImageSparseMemoryRequirements},
    {"vkGetDeviceImageSparseMemoryRequirementsKHR", vkGetDeviceImageSparseMemoryRequirementsKHR},
    {"vkGetDeviceMemoryCommitment", vkGetDeviceMemoryCommitment},
    {"vkGetDeviceMemoryOpaqueCaptureAddress", vkGetDeviceMemoryOpaqueCaptureAddress},
    {"vkGetDeviceMemoryOpaqueCaptureAddressKHR", vkGetDeviceMemoryOpaqueCaptureAddressKHR},
    {"vkGetDeviceMicromapCompatibilityEXT", vkGetDeviceMicromapCompatibilityEXT},
    {"vkGetDeviceProcAddr", vkGetDeviceProcAddr},
    {"vkGetDeviceQueue", vkGetDeviceQueue},
    {"vkGetDeviceQueue2", vkGetDeviceQueue2},
    {"vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI", vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI},
    {"vkGetDynamicRenderingTilePropertiesQCOM", vkGetDynamicRenderingTilePropertiesQCOM},
    {"vkGetEventStatus", vkGetEventStatus},
    {"vkGetFenceStatus", vkGetFenceStatus},
    {"vkGetFramebufferTilePropertiesQCOM", vkGetFramebufferTilePropertiesQCOM},
    {"vkGetGeneratedCommandsMemoryRequirementsNV", vkGetGeneratedCommandsMemoryRequirementsNV},
    {"vkGetImageMemoryRequirements", vkGetImageMemoryRequirements},
    {"vkGetImageMemoryRequirements2", vkGetImageMemoryRequirements2},
    {"vkGetImageMemoryRequirements2KHR", vkGetImageMemoryRequirements2KHR},
    {"vkGetImageOpaqueCaptureDescriptorDataEXT", vkGetImageOpaqueCaptureDescriptorDataEXT},
    {"vkGetImageSparseMemoryRequirements", vkGetImageSparseMemoryRequirements},
    {"vkGetImageSparseMemoryRequirements2", vkGetImageSparseMemoryRequirements2},
    {"vkGetImageSparseMemoryRequirements2KHR", vkGetImageSparseMemoryRequirements2KHR},
    {"vkGetImageSubresourceLayout", vkGetImageSubresourceLayout},
    {"vkGetImageSubresourceLayout2EXT", vkGetImageSubresourceLayout2EXT},
    {"vkGetImageViewAddressNVX", vkGetImageViewAddressNVX},
    {"vkGetImageViewHandleNVX", vkGetImageViewHandleNVX},
    {"vkGetImageViewOpaqueCaptureDescriptorDataEXT", vkGetImageViewOpaqueCaptureDescriptorDataEXT},
    {"vkGetMemoryHostPointerPropertiesEXT", vkGetMemoryHostPointerPropertiesEXT},
    {"vkGetMicromapBuildSizesEXT", vkGetMicromapBuildSizesEXT},
    {"vkGetPerformanceParameterINTEL", vkGetPerformanceParameterINTEL},
    {"vkGetPipelineCacheData", vkGetPipelineCacheData},
    {"vkGetPipelineExecutableInternalRepresentationsKHR", vkGetPipelineExecutableInternalRepresentationsKHR},
    {"vkGetPipelineExecutablePropertiesKHR", vkGetPipelineExecutablePropertiesKHR},
    {"vkGetPipelineExecutableStatisticsKHR", vkGetPipelineExecutableStatisticsKHR},
    {"vkGetPipelinePropertiesEXT", vkGetPipelinePropertiesEXT},
    {"vkGetPrivateData", vkGetPrivateData},
    {"vkGetPrivateDataEXT", vkGetPrivateDataEXT},
    {"vkGetQueryPoolResults", vkGetQueryPoolResults},
    {"vkGetQueueCheckpointData2NV", vkGetQueueCheckpointData2NV},
    {"vkGetQueueCheckpointDataNV", vkGetQueueCheckpointDataNV},
    {"vkGetRayTracingCaptureReplayShaderGroupHandlesKHR", vkGetRayTracingCaptureReplayShaderGroupHandlesKHR},
    {"vkGetRayTracingShaderGroupHandlesKHR", vkGetRayTracingShaderGroupHandlesKHR},
    {"vkGetRayTracingShaderGroupHandlesNV", vkGetRayTracingShaderGroupHandlesNV},
    {"vkGetRayTracingShaderGroupStackSizeKHR", vkGetRayTracingShaderGroupStackSizeKHR},
    {"vkGetRenderAreaGranularity", vkGetRenderAreaGranularity},
    {"vkGetSamplerOpaqueCaptureDescriptorDataEXT", vkGetSamplerOpaqueCaptureDescriptorDataEXT},
    {"vkGetSemaphoreCounterValue", vkGetSemaphoreCounterValue},
    {"vkGetSemaphoreCounterValueKHR", vkGetSemaphoreCounterValueKHR},
    {"vkGetShaderInfoAMD", vkGetShaderInfoAMD},
    {"vkGetShaderModuleCreateInfoIdentifierEXT", vkGetShaderModuleCreateInfoIdentifierEXT},
    {"vkGetShaderModuleIdentifierEXT", vkGetShaderModuleIdentifierEXT},
    {"vkGetSwapchainImagesKHR", vkGetSwapchainImagesKHR},
    {"vkGetValidationCacheDataEXT", vkGetValidationCacheDataEXT},
    {"vkInitializePerformanceApiINTEL", vkInitializePerformanceApiINTEL},
    {"vkInvalidateMappedMemoryRanges", vkInvalidateMappedMemoryRanges},
    {"vkMapMemory", vkMapMemory},
    {"vkMergePipelineCaches", vkMergePipelineCaches},
    {"vkMergeValidationCachesEXT", vkMergeValidationCachesEXT},
    {"vkQueueBeginDebugUtilsLabelEXT", vkQueueBeginDebugUtilsLabelEXT},
    {"vkQueueBindSparse", vkQueueBindSparse},
    {"vkQueueEndDebugUtilsLabelEXT", vkQueueEndDebugUtilsLabelEXT},
    {"vkQueueInsertDebugUtilsLabelEXT", vkQueueInsertDebugUtilsLabelEXT},
    {"vkQueuePresentKHR", vkQueuePresentKHR},
    {"vkQueueSetPerformanceConfigurationINTEL", vkQueueSetPerformanceConfigurationINTEL},
    {"vkQueueSubmit", vkQueueSubmit},
    {"vkQueueSubmit2", vkQueueSubmit2},
    {"vkQueueSubmit2KHR", vkQueueSubmit2KHR},
    {"vkQueueWaitIdle", vkQueueWaitIdle},
    {"vkReleasePerformanceConfigurationINTEL", vkReleasePerformanceConfigurationINTEL},
    {"vkReleaseProfilingLockKHR", vkReleaseProfilingLockKHR},
    {"vkReleaseSwapchainImagesEXT", vkReleaseSwapchainImagesEXT},
    {"vkResetCommandBuffer", vkResetCommandBuffer},
    {"vkResetCommandPool", vkResetCommandPool},
    {"vkResetDescriptorPool", vkResetDescriptorPool},
    {"vkResetEvent", vkResetEvent},
    {"vkResetFences", vkResetFences},
    {"vkResetQueryPool", vkResetQueryPool},
    {"vkResetQueryPoolEXT", vkResetQueryPoolEXT},
    {"vkSetDebugUtilsObjectNameEXT", vkSetDebugUtilsObjectNameEXT},
    {"vkSetDebugUtilsObjectTagEXT", vkSetDebugUtilsObjectTagEXT},
    {"vkSetDeviceMemoryPriorityEXT", vkSetDeviceMemoryPriorityEXT},
    {"vkSetEvent", vkSetEvent},
    {"vkSetPrivateData", vkSetPrivateData},
    {"vkSetPrivateDataEXT", vkSetPrivateDataEXT},
    {"vkSignalSemaphore", vkSignalSemaphore},
    {"vkSignalSemaphoreKHR", vkSignalSemaphoreKHR},
    {"vkTrimCommandPool", vkTrimCommandPool},
    {"vkTrimCommandPoolKHR", vkTrimCommandPoolKHR},
    {"vkUninitializePerformanceApiINTEL", vkUninitializePerformanceApiINTEL},
    {"vkUnmapMemory", vkUnmapMemory},
    {"vkUpdateDescriptorSetWithTemplate", vkUpdateDescriptorSetWithTemplate},
    {"vkUpdateDescriptorSetWithTemplateKHR", vkUpdateDescriptorSetWithTemplateKHR},
    {"vkUpdateDescriptorSets", vkUpdateDescriptorSets},
    {"vkWaitForFences", vkWaitForFences},
    {"vkWaitForPresentKHR", vkWaitForPresentKHR},
    {"vkWaitSemaphores", vkWaitSemaphores},
    {"vkWaitSemaphoresKHR", vkWaitSemaphoresKHR},
    {"vkWriteAccelerationStructuresPropertiesKHR", vkWriteAccelerationStructuresPropertiesKHR},
    {"vkWriteMicromapsPropertiesEXT", vkWriteMicromapsPropertiesEXT},
};

static const struct vulkan_func vk_phys_dev_dispatch_table[] =
{
    {"vkCreateDevice", vkCreateDevice},
    {"vkEnumerateDeviceExtensionProperties", vkEnumerateDeviceExtensionProperties},
    {"vkEnumerateDeviceLayerProperties", vkEnumerateDeviceLayerProperties},
    {"vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR", vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR},
    {"vkGetPhysicalDeviceCalibrateableTimeDomainsEXT", vkGetPhysicalDeviceCalibrateableTimeDomainsEXT},
    {"vkGetPhysicalDeviceCooperativeMatrixPropertiesNV", vkGetPhysicalDeviceCooperativeMatrixPropertiesNV},
    {"vkGetPhysicalDeviceExternalBufferProperties", vkGetPhysicalDeviceExternalBufferProperties},
    {"vkGetPhysicalDeviceExternalBufferPropertiesKHR", vkGetPhysicalDeviceExternalBufferPropertiesKHR},
    {"vkGetPhysicalDeviceExternalFenceProperties", vkGetPhysicalDeviceExternalFenceProperties},
    {"vkGetPhysicalDeviceExternalFencePropertiesKHR", vkGetPhysicalDeviceExternalFencePropertiesKHR},
    {"vkGetPhysicalDeviceExternalSemaphoreProperties", vkGetPhysicalDeviceExternalSemaphoreProperties},
    {"vkGetPhysicalDeviceExternalSemaphorePropertiesKHR", vkGetPhysicalDeviceExternalSemaphorePropertiesKHR},
    {"vkGetPhysicalDeviceFeatures", vkGetPhysicalDeviceFeatures},
    {"vkGetPhysicalDeviceFeatures2", vkGetPhysicalDeviceFeatures2},
    {"vkGetPhysicalDeviceFeatures2KHR", vkGetPhysicalDeviceFeatures2KHR},
    {"vkGetPhysicalDeviceFormatProperties", vkGetPhysicalDeviceFormatProperties},
    {"vkGetPhysicalDeviceFormatProperties2", vkGetPhysicalDeviceFormatProperties2},
    {"vkGetPhysicalDeviceFormatProperties2KHR", vkGetPhysicalDeviceFormatProperties2KHR},
    {"vkGetPhysicalDeviceFragmentShadingRatesKHR", vkGetPhysicalDeviceFragmentShadingRatesKHR},
    {"vkGetPhysicalDeviceImageFormatProperties", vkGetPhysicalDeviceImageFormatProperties},
    {"vkGetPhysicalDeviceImageFormatProperties2", vkGetPhysicalDeviceImageFormatProperties2},
    {"vkGetPhysicalDeviceImageFormatProperties2KHR", vkGetPhysicalDeviceImageFormatProperties2KHR},
    {"vkGetPhysicalDeviceMemoryProperties", vkGetPhysicalDeviceMemoryProperties},
    {"vkGetPhysicalDeviceMemoryProperties2", vkGetPhysicalDeviceMemoryProperties2},
    {"vkGetPhysicalDeviceMemoryProperties2KHR", vkGetPhysicalDeviceMemoryProperties2KHR},
    {"vkGetPhysicalDeviceMultisamplePropertiesEXT", vkGetPhysicalDeviceMultisamplePropertiesEXT},
    {"vkGetPhysicalDeviceOpticalFlowImageFormatsNV", vkGetPhysicalDeviceOpticalFlowImageFormatsNV},
    {"vkGetPhysicalDevicePresentRectanglesKHR", vkGetPhysicalDevicePresentRectanglesKHR},
    {"vkGetPhysicalDeviceProperties", vkGetPhysicalDeviceProperties},
    {"vkGetPhysicalDeviceProperties2", vkGetPhysicalDeviceProperties2},
    {"vkGetPhysicalDeviceProperties2KHR", vkGetPhysicalDeviceProperties2KHR},
    {"vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR", vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR},
    {"vkGetPhysicalDeviceQueueFamilyProperties", vkGetPhysicalDeviceQueueFamilyProperties},
    {"vkGetPhysicalDeviceQueueFamilyProperties2", vkGetPhysicalDeviceQueueFamilyProperties2},
    {"vkGetPhysicalDeviceQueueFamilyProperties2KHR", vkGetPhysicalDeviceQueueFamilyProperties2KHR},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", vkGetPhysicalDeviceSparseImageFormatProperties},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2", vkGetPhysicalDeviceSparseImageFormatProperties2},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2KHR", vkGetPhysicalDeviceSparseImageFormatProperties2KHR},
    {"vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV", vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV},
    {"vkGetPhysicalDeviceSurfaceCapabilities2KHR", vkGetPhysicalDeviceSurfaceCapabilities2KHR},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vkGetPhysicalDeviceSurfaceCapabilitiesKHR},
    {"vkGetPhysicalDeviceSurfaceFormats2KHR", vkGetPhysicalDeviceSurfaceFormats2KHR},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR", vkGetPhysicalDeviceSurfaceFormatsKHR},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR", vkGetPhysicalDeviceSurfacePresentModesKHR},
    {"vkGetPhysicalDeviceSurfaceSupportKHR", vkGetPhysicalDeviceSurfaceSupportKHR},
    {"vkGetPhysicalDeviceToolProperties", vkGetPhysicalDeviceToolProperties},
    {"vkGetPhysicalDeviceToolPropertiesEXT", vkGetPhysicalDeviceToolPropertiesEXT},
    {"vkGetPhysicalDeviceWin32PresentationSupportKHR", vkGetPhysicalDeviceWin32PresentationSupportKHR},
};

static const struct vulkan_func vk_instance_dispatch_table[] =
{
    {"vkCreateDebugReportCallbackEXT", vkCreateDebugReportCallbackEXT},
    {"vkCreateDebugUtilsMessengerEXT", vkCreateDebugUtilsMessengerEXT},
    {"vkCreateWin32SurfaceKHR", vkCreateWin32SurfaceKHR},
    {"vkDebugReportMessageEXT", vkDebugReportMessageEXT},
    {"vkDestroyDebugReportCallbackEXT", vkDestroyDebugReportCallbackEXT},
    {"vkDestroyDebugUtilsMessengerEXT", vkDestroyDebugUtilsMessengerEXT},
    {"vkDestroyInstance", vkDestroyInstance},
    {"vkDestroySurfaceKHR", vkDestroySurfaceKHR},
    {"vkEnumeratePhysicalDeviceGroups", vkEnumeratePhysicalDeviceGroups},
    {"vkEnumeratePhysicalDeviceGroupsKHR", vkEnumeratePhysicalDeviceGroupsKHR},
    {"vkEnumeratePhysicalDevices", vkEnumeratePhysicalDevices},
    {"vkSubmitDebugUtilsMessageEXT", vkSubmitDebugUtilsMessageEXT},
};

void *wine_vk_get_device_proc_addr(const char *name)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(vk_device_dispatch_table); i++)
    {
        if (strcmp(vk_device_dispatch_table[i].name, name) == 0)
        {
            TRACE("Found name=%s in device table\n", debugstr_a(name));
            return vk_device_dispatch_table[i].func;
        }
    }
    return NULL;
}

void *wine_vk_get_phys_dev_proc_addr(const char *name)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(vk_phys_dev_dispatch_table); i++)
    {
        if (strcmp(vk_phys_dev_dispatch_table[i].name, name) == 0)
        {
            TRACE("Found name=%s in physical device table\n", debugstr_a(name));
            return vk_phys_dev_dispatch_table[i].func;
        }
    }
    return NULL;
}

void *wine_vk_get_instance_proc_addr(const char *name)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(vk_instance_dispatch_table); i++)
    {
        if (strcmp(vk_instance_dispatch_table[i].name, name) == 0)
        {
            TRACE("Found name=%s in instance table\n", debugstr_a(name));
            return vk_instance_dispatch_table[i].func;
        }
    }
    return NULL;
}
