#include "vulkansift/viz/vulkan_viewer.h"

#include "vulkansift/utils/logger.h"

#include <array>
#include <cstdlib>
#include <cstring>

#include <thread>

static char LOG_TAG[] = "VulkanViewer";
#ifdef VK_USE_PLATFORM_ANDROID_KHR
bool VulkanAppliVulkanViewercation::init(VulkanInstance *vulkan_instance, AAssetManager *asset_manager, const int image_width, const int image_height)
#else
bool VulkanViewer::init(VulkanInstance *vulkan_instance, const int image_width, const int image_height)
#endif
{
  m_image_width = image_width;
  m_image_height = image_height;

  m_vulkan_instance = vulkan_instance;

// Create VulkanInstance (handles instance, physical and logical device, surface create, swapchain, renderpass and framebuffers)
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  m_asset_manager = asset_manager;
  VulkanUtils::Shader::setupAndroidAssetsFileAccess(m_asset_manager);
#endif

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get Vulkan entities and information from VulkanInstance
  m_device = m_vulkan_instance->getVkDevice();
  if (m_device == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanInstance returned a NULL logical device.");
    return false;
  }
  m_physical_device = m_vulkan_instance->getVkPhysicalDevice();
  if (m_physical_device == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanInstance returned a NULL physical device.");
    return false;
  }
  m_queue_family_index = m_vulkan_instance->getGraphicsQueueFamilyIndex();
  m_queue = m_vulkan_instance->getGraphicsQueue();
  if (m_queue == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanInstance returned a NULL queue.");
    terminate();
    return false;
  }
  m_render_pass = m_vulkan_instance->getRenderPass();
  if (m_render_pass == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanInstance returned a NULL render pass.");
    return false;
  }
  m_nb_swapchain_image = m_vulkan_instance->getNbSwapchainImage();
  m_swapchain_extent = m_vulkan_instance->getSwapchainExtent();
  m_swapchain_framebuffers = m_vulkan_instance->getSwapchainFramebuffers();
  m_swapchain = m_vulkan_instance->getSwapchain();
  if (m_swapchain == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanInstance returned a NULL swapchain.");
    return false;
  }

  // Init own members

  // Create graphics entities
  if (!createGraphics())
  {
    terminate();
    return false;
  }

  return true;
}

bool VulkanViewer::createGraphics()
{
  if (!createGraphicsCommandPool())
    return false;

  // Create texture image
  if (!createImages())
    return false;
  if (!createSamplers())
    return false;

  if (m_vulkan_instance->isTimestampQuerySupported())
  {
    if (!setupQueries())
      return false;
  }

  // Create buffers
  if (!createGraphicsBuffers())
    return false;

  if (!createGraphicsDescriptors())
    return false;

  if (!createGraphicsPipeline())
    return false;

  if (!createGraphicsCommandBuffers())
    return false;

  if (!createGraphicsSyncObjects())
    return false;

  return true;
}

bool VulkanViewer::createGraphicsCommandPool()
{
  VkCommandPoolCreateInfo pool_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = 0, .queueFamilyIndex = m_queue_family_index};
  if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_graphics_command_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create command pool");
    return false;
  }
  return true;
}

bool VulkanViewer::createImages()
{
  // Create staging buffer to upload new images at execution time
  VkDeviceSize image_size = sizeof(uint8_t) * m_image_width * m_image_height;
  if (!m_input_img_staging_buffer.create(m_device, m_physical_device, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
  {
    logError(LOG_TAG, "Failed to create intput image staging buffer");
    return false;
  }

  if (!m_input_image.create(m_device, m_physical_device, m_image_width, m_image_height, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
  {
    logError(LOG_TAG, "Failed to create intput image");
    return false;
  }

  // Setup initial layout for images
  bool cmd_res = VulkanUtils::submitCommandsAndWait(m_device, m_queue, m_graphics_command_pool, [&](VkCommandBuffer command_buf) {
    VkImageMemoryBarrier image_barrier = m_input_image.getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkCmdPipelineBarrier(command_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &image_barrier);
    // m_input_image.registerBarrier(command_buf, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    // VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  });
  if (!cmd_res)
  {
    logError(LOG_TAG, "Failed to apply barriers to images after creation");
    return false;
  }

  return true;
}

bool VulkanViewer::createSamplers()
{
  VkSamplerCreateInfo sampler_create_info{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                          .magFilter = VK_FILTER_LINEAR,
                                          .minFilter = VK_FILTER_LINEAR,
                                          .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                          .mipLodBias = 0.0f,
                                          .anisotropyEnable = VK_FALSE,
                                          .maxAnisotropy = 1.0f,
                                          .compareEnable = VK_FALSE,
                                          .compareOp = VK_COMPARE_OP_ALWAYS,
                                          .minLod = 0.0f,
                                          .maxLod = 0.0f,
                                          .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                                          .unnormalizedCoordinates = VK_FALSE};
  if (vkCreateSampler(m_device, &sampler_create_info, nullptr, &m_texture_sampler) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create texture sampler");
    return false;
  }
  return true;
}

bool VulkanViewer::createGraphicsBuffers()
{
  VkDeviceSize buffer_size = sizeof(float) * m_graphics_vertices.size() + sizeof(uint32_t) * m_graphics_indices.size();
  VulkanUtils::Buffer staging_buffer;
  // Create staging buffer
  if (!staging_buffer.create(m_device, m_physical_device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
  {
    logError(LOG_TAG, "Failed to create staging buffer for graphics data (vertices and indexes) upload");
    return false;
  }

  // Create graphics buffer
  if (!m_graphics_buffer.create(m_device, m_physical_device, buffer_size,
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
  {
    logError(LOG_TAG, "Failed to setup graphics buffer");
    return false;
  }

  // Map and copy vertices and index data into the staging buffer
  void *staging_buff_data;
  if (vkMapMemory(m_device, staging_buffer.getBufferMemory(), 0, buffer_size, 0, &staging_buff_data) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to map staging buffer when setting up graphics buffer");
    return false;
  }
  float *vertices_loc_buff = (float *)staging_buff_data;
  memcpy(vertices_loc_buff, m_graphics_vertices.data(), (size_t)m_graphics_vertices.size() * sizeof(m_graphics_vertices[0]));
  uint32_t *indices_loc_buff = (uint32_t *)(vertices_loc_buff + m_graphics_vertices.size());
  memcpy(indices_loc_buff, m_graphics_indices.data(), (size_t)m_graphics_indices.size() * sizeof(m_graphics_indices[0]));
  vkUnmapMemory(m_device, staging_buffer.getBufferMemory());

  // Copy the stating buffer content into the graphics buffer with a copy command
  VulkanUtils::submitCommandsAndWait(m_device, m_queue, m_graphics_command_pool, [&](VkCommandBuffer cmd_buff) {
    VkBufferCopy copy_region{.srcOffset = 0, .dstOffset = 0, .size = buffer_size};
    vkCmdCopyBuffer(cmd_buff, staging_buffer.getBuffer(), m_graphics_buffer.getBuffer(), 1, &copy_region);
  });

  // Destroy staging buffer and free its memory
  staging_buffer.destroy(m_device);

  return true;
}

bool VulkanViewer::createGraphicsDescriptors()
{
  // Create descriptor set layout, this layout is specified in the graphics pipeline
  VkDescriptorSetLayoutBinding sampler_layout_binding{.binding = 0,
                                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      .descriptorCount = 1,
                                                      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      .pImmutableSamplers = nullptr};

  std::array<VkDescriptorSetLayoutBinding, 1> bindings{sampler_layout_binding};

  VkDescriptorSetLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = bindings.size(), .pBindings = bindings.data()};

  if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_graphics_desc_set_layout) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create graphics descriptor set layout");
    return false;
  }

  // Create descriptor pool to allocate descriptor sets
  std::array<VkDescriptorPoolSize, 1> pool_sizes{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}}};
  VkDescriptorPoolCreateInfo descriptor_pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = pool_sizes.size(), .pPoolSizes = pool_sizes.data()};
  if (vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_graphics_desc_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create graphics descriptor pool");
    return false;
  }

  // Create descriptor sets that can be bound in command buffer
  VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                         .descriptorPool = m_graphics_desc_pool,
                                         .descriptorSetCount = 1,
                                         .pSetLayouts = &m_graphics_desc_set_layout};

  if (vkAllocateDescriptorSets(m_device, &alloc_info, &m_graphics_desc_set) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate graphics descriptor sets");
    return false;
  }

  VkDescriptorImageInfo image_info{
      .sampler = m_texture_sampler, .imageView = m_input_image.getImageView(), .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

  VkWriteDescriptorSet descriptor_write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                        .dstSet = m_graphics_desc_set,
                                        .dstBinding = 0,
                                        .dstArrayElement = 0,
                                        .descriptorCount = 1,
                                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        .pImageInfo = &image_info,
                                        .pBufferInfo = nullptr,
                                        .pTexelBufferView = nullptr};
  vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);

  return true;
}

bool VulkanViewer::createGraphicsPipeline()
{
  VkShaderModule vert_shader_module;
  VkShaderModule frag_shader_module;

  if (!VulkanUtils::Shader::createShaderModule(m_device, "shaders/shader.vert.spv", &vert_shader_module))
  {
    return false;
  }
  if (!VulkanUtils::Shader::createShaderModule(m_device, "shaders/shader.frag.spv", &frag_shader_module))
  {
    return false;
  }

  VkPipelineShaderStageCreateInfo vert_shader_stage_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_shader_module, .pName = "main"};

  VkPipelineShaderStageCreateInfo frag_shader_stage_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_shader_module, .pName = "main"};

  VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

  // Setup bindings for vertex buffer input (vertex coordinate and color)
  VkVertexInputBindingDescription vert_binding_desc{.binding = 0, .stride = sizeof(float) * 7, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputAttributeDescription vert_attributes_desc[]{
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0},
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(float) * 2},
      {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(float) * 5}};

  VkPipelineVertexInputStateCreateInfo vertex_input_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                         .vertexBindingDescriptionCount = 1,
                                                         .pVertexBindingDescriptions = &vert_binding_desc,
                                                         .vertexAttributeDescriptionCount = 3,
                                                         .pVertexAttributeDescriptions = vert_attributes_desc};

  VkPipelineInputAssemblyStateCreateInfo input_assembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                        .primitiveRestartEnable = VK_FALSE};

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_swapchain_extent.width;
  viewport.height = (float)m_swapchain_extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_swapchain_extent;
  VkPipelineViewportStateCreateInfo viewport_state{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                   .viewportCount = 1,
                                                   .pViewports = &viewport,
                                                   .scissorCount = 1,
                                                   .pScissors = &scissor};

  VkPipelineRasterizationStateCreateInfo rasterizer{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                    .depthClampEnable = VK_FALSE,
                                                    .rasterizerDiscardEnable = VK_FALSE,
                                                    .polygonMode = VK_POLYGON_MODE_FILL,
                                                    .cullMode = VK_CULL_MODE_BACK_BIT,
                                                    .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                                    .depthBiasEnable = VK_FALSE,
                                                    .depthBiasConstantFactor = 0.0f,
                                                    .depthBiasClamp = 0.0f,
                                                    .depthBiasSlopeFactor = 0.0f,
                                                    .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo multisampling{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                     .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                                                     .sampleShadingEnable = VK_FALSE,
                                                     .minSampleShading = 1.0f,
                                                     .pSampleMask = nullptr,
                                                     .alphaToCoverageEnable = VK_FALSE,
                                                     .alphaToOneEnable = VK_FALSE};

  VkPipelineColorBlendAttachmentState color_blend_attachment{.blendEnable = VK_FALSE,
                                                             .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                             .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                                                             .colorBlendOp = VK_BLEND_OP_ADD,
                                                             .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                             .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                                                             .alphaBlendOp = VK_BLEND_OP_ADD,
                                                             .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo color_blending{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                     .logicOpEnable = VK_FALSE,
                                                     .logicOp = VK_LOGIC_OP_COPY,
                                                     .attachmentCount = 1,
                                                     .pAttachments = &color_blend_attachment,
                                                     .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}};

  // To update with descriptor layout if needed
  VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &m_graphics_desc_set_layout};

  if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_graphics_pipeline_layout) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create graphics pipeline layout");
    return false;
  }

  VkGraphicsPipelineCreateInfo pipeline_info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                             .stageCount = 2,
                                             .pStages = shader_stages,
                                             .pVertexInputState = &vertex_input_info,
                                             .pInputAssemblyState = &input_assembly,
                                             .pViewportState = &viewport_state,
                                             .pRasterizationState = &rasterizer,
                                             .pMultisampleState = &multisampling,
                                             .pDepthStencilState = nullptr,
                                             .pColorBlendState = &color_blending,
                                             .pDynamicState = nullptr,
                                             .layout = m_graphics_pipeline_layout,
                                             .renderPass = m_render_pass,
                                             .subpass = 0,
                                             .basePipelineHandle = VK_NULL_HANDLE,
                                             .basePipelineIndex = -1};

  if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create graphics pipeline");
    return false;
  }

  vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
  vkDestroyShaderModule(m_device, frag_shader_module, nullptr);

  return true;
}

bool VulkanViewer::createGraphicsCommandBuffers()
{

  m_graphics_command_buffers.resize(m_nb_swapchain_image);
  VkCommandBufferAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                         .commandPool = m_graphics_command_pool,
                                         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                         .commandBufferCount = (uint32_t)m_nb_swapchain_image};

  if (vkAllocateCommandBuffers(m_device, &alloc_info, m_graphics_command_buffers.data()) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate graphics command buffers");
    return false;
  }

  for (size_t i = 0; i < m_nb_swapchain_image; i++)
  {
    VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0, .pInheritanceInfo = nullptr};

    if (vkBeginCommandBuffer(m_graphics_command_buffers[i], &begin_info) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to begin recording graphics command buffer");
      return false;
    }

    if (m_vulkan_instance->isTimestampQuerySupported())
    {
      vkCmdResetQueryPool(m_graphics_command_buffers[i], m_ts_query_pool, 0, 2);
      // Write graphics start timestamp
      vkCmdWriteTimestamp(m_graphics_command_buffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_ts_query_pool, 0);
    }

    // Image acquisition commands (transfer from pixels in staging buffer to device image)
    {
      VkImageMemoryBarrier image_barrier =
          m_input_image.getImageMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      vkCmdPipelineBarrier(m_graphics_command_buffers[i], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                           1, &image_barrier);
      // m_input_image.registerBarrier(m_graphics_command_buffers[i], VK_ACCESS_TRANSFER_WRITE_BIT,
      // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT);
    }

    VkBufferImageCopy buffer_image_region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .imageOffset = {.x = 0, .y = 0, .z = 0},
        .imageExtent = {.width = m_image_width, .height = m_image_height, .depth = 1}};
    vkCmdCopyBufferToImage(m_graphics_command_buffers[i], m_input_img_staging_buffer.getBuffer(), m_input_image.getImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_region);
    {
      VkImageMemoryBarrier image_barrier =
          m_input_image.getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      vkCmdPipelineBarrier(m_graphics_command_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                           1, &image_barrier);
      // m_input_image.registerBarrier(m_graphics_command_buffers[i], VK_ACCESS_SHADER_READ_BIT,
      // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    // Start render
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo render_pass_info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                           .renderPass = m_render_pass,
                                           .framebuffer = m_swapchain_framebuffers[i],
                                           .renderArea = {.offset = {0, 0}, .extent = m_swapchain_extent},
                                           .clearValueCount = 1,
                                           .pClearValues = &clear_color};

    vkCmdBeginRenderPass(m_graphics_command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(m_graphics_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    VkBuffer vertex_buffers[] = {m_graphics_buffer.getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(m_graphics_command_buffers[i], 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(m_graphics_command_buffers[i], m_graphics_buffer.getBuffer(), sizeof(float) * m_graphics_vertices.size(), VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(m_graphics_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline_layout, 0, 1, &m_graphics_desc_set, 0,
                            nullptr);
    vkCmdDrawIndexed(m_graphics_command_buffers[i], m_graphics_indices.size(), 1, 0, 0, 0);
    // vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

    vkCmdEndRenderPass(m_graphics_command_buffers[i]);

    if (m_vulkan_instance->isTimestampQuerySupported())
    {
      // Write graphics end timestamp
      vkCmdWriteTimestamp(m_graphics_command_buffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_ts_query_pool, 1);
    }

    if (vkEndCommandBuffer(m_graphics_command_buffers[i]) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to record graphics command buffer");
      return false;
    }
  }
  return true;
}

bool VulkanViewer::setupQueries()
{
  VkQueryPoolCreateInfo query_pool_info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2};

  if (vkCreateQueryPool(m_device, &query_pool_info, nullptr, &m_ts_query_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create timestamp query pool");
    return false;
  }

  return true;
}

bool VulkanViewer::createGraphicsSyncObjects()
{
  // Create semaphores
  VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_graphics_image_available_semaphore) != VK_SUCCESS ||
      vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_graphics_render_finished_semaphore) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create graphics semaphores");
    return false;
  }

  // Create fences
  VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
  if (vkCreateFence(m_device, &fence_info, nullptr, &m_frame_rendering_fence) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create graphics fence");
    return false;
  }

  return true;
}

bool VulkanViewer::execOnce(uint8_t *pixel_buffer, float *gpu_time_ms)
{
  // Wait if current frame is used
  vkWaitForFences(m_device, 1, &m_frame_rendering_fence, VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_frame_rendering_fence);

  if (m_vulkan_instance->isTimestampQuerySupported())
  {
    // Get previous execution graphics timestamps
    uint64_t timestamps[2] = {0u, 0u};
    vkGetQueryPoolResults(m_device, m_ts_query_pool, 0, 2, sizeof(uint64_t) * 2, timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    uint64_t ts_start = timestamps[0] & m_vulkan_instance->getTimestampBitMask();
    uint64_t ts_end = timestamps[1] & m_vulkan_instance->getTimestampBitMask();
    *gpu_time_ms = (float(ts_end - ts_start) * m_vulkan_instance->getTimestampQueryPeriodNs()) / 1000000.f;
  }
  else
  {
    *gpu_time_ms = -1.f;
  }

  // Copy new camera image pixels to the staging buffer
  void *staging_buf_data = nullptr;
  if (vkMapMemory(m_device, m_input_img_staging_buffer.getBufferMemory(), 0, m_image_width * m_image_height * sizeof(uint8_t), 0, &staging_buf_data) !=
      VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to map staging buffer to upload new image pixels");
    return false;
  }
  memcpy(staging_buf_data, pixel_buffer, m_image_width * m_image_height * sizeof(uint8_t));
  vkUnmapMemory(m_device, m_input_img_staging_buffer.getBufferMemory());

  // Acquire image
  uint32_t image_idx;
  VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_graphics_image_available_semaphore, VK_NULL_HANDLE, &image_idx);
  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // If VK_ERROR_OUT_OF_DATE_KHR, the surface is no longer compatible with the swapchain, nothing can be displayed so the swapchain must be recreated
    logInfo(LOG_TAG, "Must recreate swapchain after error code %d on vkAcquireNextImageKHR", result);
    if (!recreateSwapchain())
    {
      logError(LOG_TAG, "Failed to recreate swapchain after VK_ERROR_OUT_OF_DATE_KHR");
      return false;
    }
  }
  // Accept VK_SUBOPTIMAL_KHR because presentation will still be visible (but swapchain should be recreated after presentation)
  else if (result != VK_SUBOPTIMAL_KHR && result != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to acquire acquire next swapchain image before drawing");
    return false;
  }

  // Draw
  VkSemaphore wait_semaphores[] = {m_graphics_image_available_semaphore};
  VkSemaphore signal_semaphores[] = {m_graphics_render_finished_semaphore};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .waitSemaphoreCount = 1,
                           .pWaitSemaphores = wait_semaphores,
                           .pWaitDstStageMask = waitStages,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &m_graphics_command_buffers[image_idx],
                           .signalSemaphoreCount = 1,
                           .pSignalSemaphores = signal_semaphores};

  if (vkQueueSubmit(m_queue, 1, &submit_info, m_frame_rendering_fence) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to submit draw command buffer");
    return false;
  }

  // Present
  VkSwapchainKHR swapchains[] = {m_swapchain};
  VkPresentInfoKHR present_info{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .waitSemaphoreCount = 1,
                                .pWaitSemaphores = signal_semaphores,
                                .swapchainCount = 1,
                                .pSwapchains = swapchains,
                                .pImageIndices = &image_idx,
                                .pResults = nullptr};
  result = vkQueuePresentKHR(m_queue, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
  {
    logInfo(LOG_TAG, "Must recreate swapchain after error code %d on vkQueuePresentKHR", result);
    if (!recreateSwapchain())
    {
      logError(LOG_TAG, "Failed to recreate swapchain after VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR");
      return false;
    }
  }
  else if (result != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to present swapchain image");
    return false;
  }

  return true;
}

bool VulkanViewer::recreateSwapchain()
{
  vkDeviceWaitIdle(m_device);

  // Destroy swapchain dependencies (graphics pipeline, command buffers)
  vkFreeCommandBuffers(m_device, m_graphics_command_pool, static_cast<uint32_t>(m_graphics_command_buffers.size()), m_graphics_command_buffers.data());
  vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_graphics_pipeline_layout, nullptr);
  ////////////////////////////////////////////////

  // Reset swapchain
  if (!m_vulkan_instance->resetSwapchain())
  {
    logError(LOG_TAG, "Device manager failed to reset swapchain");
    return false;
  }

  // Update entities from VulkanInstance
  m_render_pass = m_vulkan_instance->getRenderPass();
  if (m_render_pass == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanInstance returned a NULL render pass.");
    return false;
  }
  m_nb_swapchain_image = m_vulkan_instance->getNbSwapchainImage();
  m_swapchain_extent = m_vulkan_instance->getSwapchainExtent();
  m_swapchain_framebuffers = m_vulkan_instance->getSwapchainFramebuffers();
  m_swapchain = m_vulkan_instance->getSwapchain();
  if (m_swapchain == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanInstance returned a NULL swapchain.");
    return false;
  }

  // Create swapchain dependencies
  if (!createGraphicsPipeline())
    return false;

  if (!createGraphicsCommandBuffers())
    return false;

  return true;
}

void VulkanViewer::cleanupGraphics()
{
  // Free fence
  VK_NULL_SAFE_DELETE(m_frame_rendering_fence, vkDestroyFence(m_device, m_frame_rendering_fence, nullptr));
  // Free semaphores
  VK_NULL_SAFE_DELETE(m_graphics_image_available_semaphore, vkDestroySemaphore(m_device, m_graphics_image_available_semaphore, nullptr));
  VK_NULL_SAFE_DELETE(m_graphics_render_finished_semaphore, vkDestroySemaphore(m_device, m_graphics_render_finished_semaphore, nullptr));
  // Free command buffers
  vkFreeCommandBuffers(m_device, m_graphics_command_pool, static_cast<uint32_t>(m_graphics_command_buffers.size()), m_graphics_command_buffers.data());
  // Destroy pipeline
  VK_NULL_SAFE_DELETE(m_graphics_pipeline, vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr));
  VK_NULL_SAFE_DELETE(m_graphics_pipeline_layout, vkDestroyPipelineLayout(m_device, m_graphics_pipeline_layout, nullptr));
  // Destroy graphics descriptors
  VK_NULL_SAFE_DELETE(m_graphics_desc_pool, vkDestroyDescriptorPool(m_device, m_graphics_desc_pool, nullptr));
  m_graphics_desc_set = VK_NULL_HANDLE;
  VK_NULL_SAFE_DELETE(m_graphics_desc_set_layout, vkDestroyDescriptorSetLayout(m_device, m_graphics_desc_set_layout, nullptr));
  ////////////////////////////////////////////////
  // Destroy image sampler
  VK_NULL_SAFE_DELETE(m_texture_sampler, vkDestroySampler(m_device, m_texture_sampler, nullptr));
  // Destroy input image
  m_input_image.destroy(m_device);
  // Destroy input image stating buffer and memory
  m_input_img_staging_buffer.destroy(m_device);
  // Destroy graphics buffer
  m_graphics_buffer.destroy(m_device);
  // Destroy graphics command pool
  VK_NULL_SAFE_DELETE(m_graphics_command_pool, vkDestroyCommandPool(m_device, m_graphics_command_pool, nullptr));
  // Destroy queries
  if (m_vulkan_instance->isTimestampQuerySupported())
  {
    VK_NULL_SAFE_DELETE(m_ts_query_pool, vkDestroyQueryPool(m_device, m_ts_query_pool, nullptr));
  }
}

void VulkanViewer::terminate()
{
  vkDeviceWaitIdle(m_device);

  cleanupGraphics();

  // Reset to VK_NULL_HANDLE object obtained from VulkanInstance
  m_vulkan_instance = nullptr;
  m_physical_device = VK_NULL_HANDLE;
  m_device = VK_NULL_HANDLE;
  m_queue = VK_NULL_HANDLE;
  m_render_pass = VK_NULL_HANDLE;
  m_swapchain_framebuffers.clear();
  m_swapchain = VK_NULL_HANDLE;
}