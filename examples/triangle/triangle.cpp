/*
* Vulkan Example - Basic indexed triangle rendering
*
* Note:
*	This is a "pedal to the metal" example to show off how to get Vulkan up an displaying something
*	Contrary to the other examples, this one won't make use of helper functions or initializers
*	Except in a few cases (swap chain setup e.g.)
*
* Copyright (C) 2016-2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fstream>
#include <vector>
#include <exception>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>

#include "vikApplicationVks.hpp"
#include "vikShader.hpp"

// Set to "true" to use staging buffers for uploading vertex and index data to device local memory
// See "prepareVertices" for details on what's staging and on why to use it
#define USE_STAGING true

class Triangle : public vik::ApplicationVks
{
public:
  // Vertex layout used in this example
  struct Vertex {
    float position[3];
    float color[3];
  };

  // Vertex buffer and attributes
  struct {
    VkDeviceMemory memory;															// Handle to the device memory for this buffer
    VkBuffer buffer;																// Handle to the Vulkan buffer object that the memory is bound to
  } vertices;

  // Index buffer
  struct {
    VkDeviceMemory memory;
    VkBuffer buffer;
    uint32_t count;
  } indices;

  // Uniform buffer block object
  struct {
    VkDeviceMemory memory;
    VkBuffer buffer;
    VkDescriptorBufferInfo descriptor;
  } uniformBufferVS;

  // For simplicity we use the same uniform block layout as in the shader:
  //
  //	layout(set = 0, binding = 0) uniform UBO
  //	{
  //		mat4 projectionMatrix;
  //		mat4 modelMatrix;
  //		mat4 viewMatrix;
  //	} ubo;
  //
  // This way we can just memcopy the ubo data to the ubo
  // Note: You should use data types that align with the GPU in order to avoid manual padding (vec4, mat4)
  struct {
    glm::mat4 projectionMatrix;
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
  } uboVS;

  // The pipeline layout is used by a pipline to access the descriptor sets
  // It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
  // A pipeline layout can be shared among multiple pipelines as long as their interfaces match
  VkPipelineLayout pipelineLayout;

  // Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
  // While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
  // So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
  // Even though this adds a new dimension of planing ahead, it's a great opportunity for performance optimizations by the driver
  VkPipeline pipeline;

  // The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
  // Like the pipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
  VkDescriptorSetLayout descriptorSetLayout;

  // The descriptor set stores the resources bound to the binding points in a shader
  // It connects the binding points of the different shaders with the buffers and images used for those bindings
  VkDescriptorSet descriptorSet;


  // Synchronization primitives
  // Synchronization is an important concept of Vulkan that OpenGL mostly hid away. Getting this right is crucial to using Vulkan.

  // Semaphores
  // Used to coordinate operations within the graphics queue and ensure correct command ordering
  VkSemaphore presentCompleteSemaphore;
  VkSemaphore renderCompleteSemaphore;

  // Fences
  // Used to check the completion of queue operations (e.g. command buffer execution)
  std::vector<VkFence> waitFences;

  Triangle() : ApplicationVks() {
    zoom = -2.5f;
    title = "Vulkan Example - Basic indexed triangle";
    // Values not set here are initialized in the base class constructor
  }

  ~Triangle() {
    // Clean up used Vulkan resources
    // Note: Inherited destructor cleans up resources stored in base class
    vkDestroyPipeline(renderer->device, pipeline, nullptr);

    vkDestroyPipelineLayout(renderer->device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(renderer->device, descriptorSetLayout, nullptr);

    vkDestroyBuffer(renderer->device, vertices.buffer, nullptr);
    vkFreeMemory(renderer->device, vertices.memory, nullptr);

    vkDestroyBuffer(renderer->device, indices.buffer, nullptr);
    vkFreeMemory(renderer->device, indices.memory, nullptr);

    vkDestroyBuffer(renderer->device, uniformBufferVS.buffer, nullptr);
    vkFreeMemory(renderer->device, uniformBufferVS.memory, nullptr);

    vkDestroySemaphore(renderer->device, presentCompleteSemaphore, nullptr);
    vkDestroySemaphore(renderer->device, renderCompleteSemaphore, nullptr);

    for (auto& fence : waitFences)
    {
      vkDestroyFence(renderer->device, fence, nullptr);
    }
  }

  // This function is used to request a device memory type that supports all the property flags we request (e.g. device local, host visibile)
  // Upon success it will return the index of the memory type that fits our requestes memory properties
  // This is necessary as implementations can offer an arbitrary number of memory types with different
  // memory properties.
  // You can check http://vulkan.gpuinfo.org/ for details on different memory configurations
  uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) {
    // Iterate over all memory types available for the device used in this example
    for (uint32_t i = 0; i < renderer->deviceMemoryProperties.memoryTypeCount; i++)
    {
      if ((typeBits & 1) == 1)
      {
        if ((renderer->deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
          return i;
        }
      }
      typeBits >>= 1;
    }

    throw "Could not find a suitable memory type!";
  }

  // Create the Vulkan synchronization primitives used in this example
  void prepareSynchronizationPrimitives() {
    // Semaphores (Used for correct command ordering)
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;

    // Semaphore used to ensures that image presentation is complete before starting to submit again
    vik_log_check(vkCreateSemaphore(renderer->device, &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore));

    // Semaphore used to ensures that all commands submitted have been finished before submitting the image to the queue
    vik_log_check(vkCreateSemaphore(renderer->device, &semaphoreCreateInfo, nullptr, &renderCompleteSemaphore));

    // Fences (Used to check draw command buffer completion)
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Create in signaled state so we don't wait on first render of each command buffer
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    waitFences.resize(renderer->drawCmdBuffers.size());
    for (auto& fence : waitFences)
    {
      vik_log_check(vkCreateFence(renderer->device, &fenceCreateInfo, nullptr, &fence));
    }
  }

  // Get a new command buffer from the command pool
  // If begin is true, the command buffer is also started so we can start adding commands
  VkCommandBuffer getCommandBuffer(bool begin) {
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = renderer->cmdPool;
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    vik_log_check(vkAllocateCommandBuffers(renderer->device, &cmdBufAllocateInfo, &cmdBuffer));

    // If requested, also start the new command buffer
    if (begin)
    {
      VkCommandBufferBeginInfo cmdBufInfo = vik::initializers::commandBufferBeginInfo();
      vik_log_check(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
    }

    return cmdBuffer;
  }

  // End the command buffer and submit it to the queue
  // Uses a fence to ensure command buffer has finished executing before deleting it
  void flushCommandBuffer(VkCommandBuffer commandBuffer) {
    assert(commandBuffer != VK_NULL_HANDLE);

    vik_log_check(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    VkFence fence;
    vik_log_check(vkCreateFence(renderer->device, &fenceCreateInfo, nullptr, &fence));

    // Submit to the queue
    vik_log_check(vkQueueSubmit(renderer->queue, 1, &submitInfo, fence));
    // Wait for the fence to signal that command buffer has finished executing
    vik_log_check(vkWaitForFences(renderer->device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

    vkDestroyFence(renderer->device, fence, nullptr);
    vkFreeCommandBuffers(renderer->device, renderer->cmdPool, 1, &commandBuffer);
  }

  // Build separate command buffers for every framebuffer image
  // Unlike in OpenGL all rendering commands are recorded once into command buffers that are then resubmitted to the queue
  // This allows to generate work upfront and from multiple threads, one of the biggest advantages of Vulkan
  void buildCommandBuffers() {
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufInfo.pNext = nullptr;

    // Set clear values for all framebuffer attachments with loadOp set to clear
    // We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = nullptr;
    renderPassBeginInfo.renderPass = renderer->render_pass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = renderer->width;
    renderPassBeginInfo.renderArea.extent.height = renderer->height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    vik_log_d("we will process %ld draw buffers", renderer->drawCmdBuffers.size());

    for (int32_t i = 0; i < renderer->drawCmdBuffers.size(); ++i) {
      // Set target frame buffer
      renderPassBeginInfo.framebuffer = renderer->frame_buffers[i];

      vik_log_check(vkBeginCommandBuffer(renderer->drawCmdBuffers[i], &cmdBufInfo));

      // Start the first sub pass specified in our default render pass setup by the base class
      // This will clear the color and depth attachment
      vkCmdBeginRenderPass(renderer->drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      // Update dynamic viewport state
      VkViewport viewport = {};
      viewport.height = (float) renderer->height;
      viewport.width = (float) renderer->width;
      viewport.minDepth = (float) 0.0f;
      viewport.maxDepth = (float) 1.0f;
      vkCmdSetViewport(renderer->drawCmdBuffers[i], 0, 1, &viewport);

      // Update dynamic scissor state
      VkRect2D scissor = {};
      scissor.extent.width = renderer->width;
      scissor.extent.height = renderer->height;
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      vkCmdSetScissor(renderer->drawCmdBuffers[i], 0, 1, &scissor);

      // Bind descriptor sets describing shader binding points
      vkCmdBindDescriptorSets(renderer->drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

      // Bind the rendering pipeline
      // The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
      vkCmdBindPipeline(renderer->drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

      // Bind triangle vertex buffer (contains position and colors)
      VkDeviceSize offsets[1] = { 0 };
      vkCmdBindVertexBuffers(renderer->drawCmdBuffers[i], 0, 1, &vertices.buffer, offsets);

      // Bind triangle index buffer
      vkCmdBindIndexBuffer(renderer->drawCmdBuffers[i], indices.buffer, 0, VK_INDEX_TYPE_UINT32);

      // Draw indexed triangle
      vkCmdDrawIndexed(renderer->drawCmdBuffers[i], indices.count, 1, 0, 0, 1);

      vkCmdEndRenderPass(renderer->drawCmdBuffers[i]);

      // Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
      // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

      vik_log_check(vkEndCommandBuffer(renderer->drawCmdBuffers[i]));
    }
    vik_log_d("buildCommandBuffers size: %ld", renderer->drawCmdBuffers.size());
  }

  void draw() {
    // Get next image in the swap chain (back/front buffer)
    vik::SwapChainVK *sc = (vik::SwapChainVK*) renderer->swap_chain;
    vik_log_check(sc->acquire_next_image(presentCompleteSemaphore, &renderer->currentBuffer));

    // Use a fence to wait until the command buffer has finished execution before using it again
    vik_log_check(vkWaitForFences(renderer->device, 1, &waitFences[renderer->currentBuffer], VK_TRUE, UINT64_MAX));
    vik_log_check(vkResetFences(renderer->device, 1, &waitFences[renderer->currentBuffer]));

    // Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // The submit info structure specifices a command buffer queue submission batch
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
    submitInfo.pWaitSemaphores = &presentCompleteSemaphore;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
    submitInfo.waitSemaphoreCount = 1;												// One wait semaphore
    submitInfo.pSignalSemaphores = &renderCompleteSemaphore;						// Semaphore(s) to be signaled when command buffers have completed
    submitInfo.signalSemaphoreCount = 1;											// One signal semaphore
    submitInfo.pCommandBuffers = &renderer->drawCmdBuffers[renderer->currentBuffer];					// Command buffers(s) to execute in this batch (submission)
    submitInfo.commandBufferCount = 1;												// One command buffer

    // Submit to the graphics queue passing a wait fence
    vik_log_check(vkQueueSubmit(renderer->queue, 1, &submitInfo, waitFences[renderer->currentBuffer]));

    // Present the current buffer to the swap chain
    // Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
    // This ensures that the image is not presented to the windowing system until all commands have been submitted
    vik_log_check(sc->present(renderer->queue, renderer->currentBuffer, renderCompleteSemaphore));
  }

  // Prepare vertex and index buffers for an indexed triangle
  // Also uploads them to device local memory using staging and initializes vertex input and attribute binding to match the vertex shader
  void prepareVertices(bool useStagingBuffers) {
    // A note on memory management in Vulkan in general:
    //	This is a very complex topic and while it's fine for an example application to to small individual memory allocations that is not
    //	what should be done a real-world application, where you should allocate large chunkgs of memory at once isntead.

    // Setup vertices
    std::vector<Vertex> vertexBuffer =
    {
      { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
      { { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
      { {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
    };
    uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

    // Setup indices
    std::vector<uint32_t> indexBuffer = { 0, 1, 2 };
    indices.count = static_cast<uint32_t>(indexBuffer.size());
    uint32_t indexBufferSize = indices.count * sizeof(uint32_t);

    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memReqs;

    void *data;

    if (useStagingBuffers) {
      // Static data like vertex and index buffer should be stored on the device memory
      // for optimal (and fastest) access by the GPU
      //
      // To achieve this we use so-called "staging buffers" :
      // - Create a buffer that's visible to the host (and can be mapped)
      // - Copy the data to this buffer
      // - Create another buffer that's local on the device (VRAM) with the same size
      // - Copy the data from the host to the device using a command buffer
      // - Delete the host visible (staging) buffer
      // - Use the device local buffers for rendering

      struct StagingBuffer {
        VkDeviceMemory memory;
        VkBuffer buffer;
      };

      struct {
        StagingBuffer vertices;
        StagingBuffer indices;
      } stagingBuffers;

      // Vertex buffer
      VkBufferCreateInfo vertexBufferInfo = {};
      vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      vertexBufferInfo.size = vertexBufferSize;
      // Buffer is used as the copy source
      vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      // Create a host-visible buffer to copy the vertex data to (staging buffer)
      vik_log_check(vkCreateBuffer(renderer->device, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, stagingBuffers.vertices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      // Request a host visible memory type that can be used to copy our data do
      // Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &stagingBuffers.vertices.memory));
      // Map and copy
      vik_log_check(vkMapMemory(renderer->device, stagingBuffers.vertices.memory, 0, memAlloc.allocationSize, 0, &data));
      memcpy(data, vertexBuffer.data(), vertexBufferSize);
      vkUnmapMemory(renderer->device, stagingBuffers.vertices.memory);
      vik_log_check(vkBindBufferMemory(renderer->device, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0));

      // Create a device local buffer to which the (host local) vertex data will be copied and which will be used for rendering
      vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      vik_log_check(vkCreateBuffer(renderer->device, &vertexBufferInfo, nullptr, &vertices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, vertices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &vertices.memory));
      vik_log_check(vkBindBufferMemory(renderer->device, vertices.buffer, vertices.memory, 0));

      // Index buffer
      VkBufferCreateInfo indexbufferInfo = {};
      indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      indexbufferInfo.size = indexBufferSize;
      indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      // Copy index data to a buffer visible to the host (staging buffer)
      vik_log_check(vkCreateBuffer(renderer->device, &indexbufferInfo, nullptr, &stagingBuffers.indices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, stagingBuffers.indices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &stagingBuffers.indices.memory));
      vik_log_check(vkMapMemory(renderer->device, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data));
      memcpy(data, indexBuffer.data(), indexBufferSize);
      vkUnmapMemory(renderer->device, stagingBuffers.indices.memory);
      vik_log_check(vkBindBufferMemory(renderer->device, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0));

      // Create destination buffer with device only visibility
      indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      vik_log_check(vkCreateBuffer(renderer->device, &indexbufferInfo, nullptr, &indices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, indices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &indices.memory));
      vik_log_check(vkBindBufferMemory(renderer->device, indices.buffer, indices.memory, 0));

      VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
      cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      cmdBufferBeginInfo.pNext = nullptr;

      // Buffer copies have to be submitted to a queue, so we need a command buffer for them
      // Note: Some devices offer a dedicated transfer queue (with only the transfer bit set) that may be faster when doing lots of copies
      VkCommandBuffer copyCmd = getCommandBuffer(true);

      // Put buffer region copies into command buffer
      VkBufferCopy copyRegion = {};

      // Vertex buffer
      copyRegion.size = vertexBufferSize;
      vkCmdCopyBuffer(copyCmd, stagingBuffers.vertices.buffer, vertices.buffer, 1, &copyRegion);
      // Index buffer
      copyRegion.size = indexBufferSize;
      vkCmdCopyBuffer(copyCmd, stagingBuffers.indices.buffer, indices.buffer,	1, &copyRegion);

      // Flushing the command buffer will also submit it to the queue and uses a fence to ensure that all commands have been executed before returning
      flushCommandBuffer(copyCmd);

      // Destroy staging buffers
      // Note: Staging buffer must not be deleted before the copies have been submitted and executed
      vkDestroyBuffer(renderer->device, stagingBuffers.vertices.buffer, nullptr);
      vkFreeMemory(renderer->device, stagingBuffers.vertices.memory, nullptr);
      vkDestroyBuffer(renderer->device, stagingBuffers.indices.buffer, nullptr);
      vkFreeMemory(renderer->device, stagingBuffers.indices.memory, nullptr);
    } else {
      // Don't use staging
      // Create host-visible buffers only and use these for rendering. This is not advised and will usually result in lower rendering performance

      // Vertex buffer
      VkBufferCreateInfo vertexBufferInfo = {};
      vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      vertexBufferInfo.size = vertexBufferSize;
      vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

      // Copy vertex data to a buffer visible to the host
      vik_log_check(vkCreateBuffer(renderer->device, &vertexBufferInfo, nullptr, &vertices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, vertices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT is host visible memory, and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT makes sure writes are directly visible
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &vertices.memory));
      vik_log_check(vkMapMemory(renderer->device, vertices.memory, 0, memAlloc.allocationSize, 0, &data));
      memcpy(data, vertexBuffer.data(), vertexBufferSize);
      vkUnmapMemory(renderer->device, vertices.memory);
      vik_log_check(vkBindBufferMemory(renderer->device, vertices.buffer, vertices.memory, 0));

      // Index buffer
      VkBufferCreateInfo indexbufferInfo = {};
      indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      indexbufferInfo.size = indexBufferSize;
      indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

      // Copy index data to a buffer visible to the host
      vik_log_check(vkCreateBuffer(renderer->device, &indexbufferInfo, nullptr, &indices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, indices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &indices.memory));
      vik_log_check(vkMapMemory(renderer->device, indices.memory, 0, indexBufferSize, 0, &data));
      memcpy(data, indexBuffer.data(), indexBufferSize);
      vkUnmapMemory(renderer->device, indices.memory);
      vik_log_check(vkBindBufferMemory(renderer->device, indices.buffer, indices.memory, 0));
    }
  }

  void setupDescriptorPool() {
    // We need to tell the API the number of max. requested descriptors per type
    VkDescriptorPoolSize typeCounts[1];
    // This example only uses one descriptor type (uniform buffer) and only requests one descriptor of this type
    typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    typeCounts[0].descriptorCount = 1;
    // For additional types you need to add new entries in the type count list
    // E.g. for two combined image samplers :
    // typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // typeCounts[1].descriptorCount = 2;

    // Create the global descriptor pool
    // All descriptors used in this example are allocated from this pool
    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = nullptr;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = typeCounts;
    // Set the max. number of descriptor sets that can be requested from this pool (requesting beyond this limit will result in an error)
    descriptorPoolInfo.maxSets = 1;

    vik_log_check(vkCreateDescriptorPool(renderer->device, &descriptorPoolInfo, nullptr, &renderer->descriptorPool));
  }

  void setupDescriptorSetLayout() {
    // Setup layout of descriptors used in this example
    // Basically connects the different shader stages to descriptors for binding uniform buffers, image samplers, etc.
    // So every shader binding should map to one descriptor set layout binding

    // Binding 0: Uniform buffer (Vertex shader)
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = 1;
    descriptorLayout.pBindings = &layoutBinding;

    vik_log_check(vkCreateDescriptorSetLayout(renderer->device, &descriptorLayout, nullptr, &descriptorSetLayout));

    // Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
    // In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

    vik_log_check(vkCreatePipelineLayout(renderer->device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
  }

  void setupDescriptorSet() {
    // Allocate a new descriptor set from the global descriptor pool
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = renderer->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    vik_log_check(vkAllocateDescriptorSets(renderer->device, &allocInfo, &descriptorSet));

    // Update the descriptor set determining the shader binding points
    // For every binding point used in a shader there needs to be one
    // descriptor set matching that binding point

    VkWriteDescriptorSet writeDescriptorSet = {};

    // Binding 0 : Uniform buffer
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &uniformBufferVS.descriptor;
    // Binds this uniform buffer to binding point 0
    writeDescriptorSet.dstBinding = 0;

    vkUpdateDescriptorSets(renderer->device, 1, &writeDescriptorSet, 0, nullptr);
  }

  void preparePipelines() {
    // Create the graphics pipeline used in this example
    // Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
    // A pipeline is then stored and hashed on the GPU making pipeline changes very fast
    // Note: There are still a few dynamic states that are not directly part of the pipeline (but the info that they are used is)

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    // The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
    pipelineCreateInfo.layout = pipelineLayout;
    // Renderpass this pipeline is attached to
    pipelineCreateInfo.renderPass = renderer->render_pass;

    // Construct the differnent states making up the pipeline

    // Input assembly state describes how primitives are assembled
    // This pipeline will assemble vertex data as a triangle lists (though we only use one triangle)
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;

    // Color blend state describes how blend factors are calculated (if used)
    // We need one blend attachment state per color attachment (even if blending is not used
    VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
    blendAttachmentState[0].colorWriteMask = 0xf;
    blendAttachmentState[0].blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState;

    // Viewport state sets the number of viewports and scissor used in this pipeline
    // Note: This is actually overriden by the dynamic states (see below)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Enable dynamic states
    // Most states are baked into the pipeline, but there are still a few dynamic states that can be changed within a command buffer
    // To be able to change these we need do specify which dynamic states will be changed using this pipeline. Their actual states are set later on in the command buffer.
    // For this example we will set the viewport and scissor using dynamic states
    std::vector<VkDynamicState> dynamicStateEnables;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

    // Depth and stencil state containing depth and stencil compare and test operations
    // We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = depthStencilState.back;

    // Multi sampling state
    // This example does not make use fo multi sampling (for anti-aliasing), the state must still be set and passed to the pipeline
    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.pSampleMask = nullptr;

    // Vertex input descriptions
    // Specifies the vertex input parameters for a pipeline

    // Vertex input binding
    // This example uses a single vertex input binding at binding point 0 (see vkCmdBindVertexBuffers)
    VkVertexInputBindingDescription vertexInputBinding = {};
    vertexInputBinding.binding = 0;
    vertexInputBinding.stride = sizeof(Vertex);
    vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Inpute attribute bindings describe shader attribute locations and memory layouts
    std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributs;
    // These match the following shader layout (see triangle.vert):
    //	layout (location = 0) in vec3 inPos;
    //	layout (location = 1) in vec3 inColor;
    // Attribute location 0: Position
    vertexInputAttributs[0].binding = 0;
    vertexInputAttributs[0].location = 0;
    // Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
    vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[0].offset = offsetof(Vertex, position);
    // Attribute location 1: Color
    vertexInputAttributs[1].binding = 0;
    vertexInputAttributs[1].location = 1;
    // Color attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
    vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[1].offset = offsetof(Vertex, color);

    // Vertex input state used for pipeline creation
    VkPipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputState.vertexAttributeDescriptionCount = 2;
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs.data();

    // Shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
    shaderStages[0] = vik::Shader::load(renderer->device, "triangle/triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = vik::Shader::load(renderer->device, "triangle/triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    // Set pipeline shader stage info
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();

    // Assign the pipeline states to the pipeline creation info structure
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.renderPass = renderer->render_pass;
    pipelineCreateInfo.pDynamicState = &dynamicState;

    // Create rendering pipeline using the specified states
    vik_log_check(vkCreateGraphicsPipelines(renderer->device, renderer->pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

    // Shader modules are no longer needed once the graphics pipeline has been created
    vkDestroyShaderModule(renderer->device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(renderer->device, shaderStages[1].module, nullptr);
  }

  void prepareUniformBuffers() {
    // Prepare and initialize a uniform buffer block containing shader uniforms
    // Single uniforms like in OpenGL are no longer present in Vulkan. All Shader uniforms are passed via uniform buffer blocks
    VkMemoryRequirements memReqs;

    // Vertex shader uniform buffer block
    VkBufferCreateInfo bufferInfo = {};
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.allocationSize = 0;
    allocInfo.memoryTypeIndex = 0;

    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(uboVS);
    // This buffer will be used as a uniform buffer
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    // Create a new buffer
    vik_log_check(vkCreateBuffer(renderer->device, &bufferInfo, nullptr, &uniformBufferVS.buffer));
    // Get memory requirements including size, alignment and memory type
    vkGetBufferMemoryRequirements(renderer->device, uniformBufferVS.buffer, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    // Get the memory type index that supports host visibile memory access
    // Most implementations offer multiple memory types and selecting the correct one to allocate memory from is crucial
    // We also want the buffer to be host coherent so we don't have to flush (or sync after every update.
    // Note: This may affect performance so you might not want to do this in a real world application that updates buffers on a regular base
    allocInfo.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    // Allocate memory for the uniform buffer
    vik_log_check(vkAllocateMemory(renderer->device, &allocInfo, nullptr, &(uniformBufferVS.memory)));
    // Bind memory to buffer
    vik_log_check(vkBindBufferMemory(renderer->device, uniformBufferVS.buffer, uniformBufferVS.memory, 0));

    // Store information in the uniform's descriptor that is used by the descriptor set
    uniformBufferVS.descriptor.buffer = uniformBufferVS.buffer;
    uniformBufferVS.descriptor.offset = 0;
    uniformBufferVS.descriptor.range = sizeof(uboVS);

    updateUniformBuffers();
  }

  void updateUniformBuffers() {
    // Update matrices
    uboVS.projectionMatrix = glm::perspective(glm::radians(60.0f), (float)renderer->width / (float)renderer->height, 0.1f, 256.0f);

    uboVS.viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

    uboVS.modelMatrix = glm::mat4();
    uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    // Map uniform buffer and update it
    uint8_t *pData;
    vik_log_check(vkMapMemory(renderer->device, uniformBufferVS.memory, 0, sizeof(uboVS), 0, (void **)&pData));
    memcpy(pData, &uboVS, sizeof(uboVS));
    // Unmap after data has been copied
    // Note: Since we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU
    vkUnmapMemory(renderer->device, uniformBufferVS.memory);
  }

  void prepare() {
    ApplicationVks::prepare();
    prepareSynchronizationPrimitives();
    prepareVertices(USE_STAGING);
    prepareUniformBuffers();
    setupDescriptorSetLayout();
    preparePipelines();
    setupDescriptorPool();
    setupDescriptorSet();
    buildCommandBuffers();
    prepared = true;
  }

  virtual void render() {
    if (!prepared)
      return;
    draw();
  }

  virtual void viewChanged() {
    updateUniformBuffers();
  }
};

int main(int argc, char *argv[]) {
  Triangle *app = new Triangle();
  app->parse_arguments(argc, argv);
  app->prepare();
  app->loop();
  delete(app);

  return 0;
}
