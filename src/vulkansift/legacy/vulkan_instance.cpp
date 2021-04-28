#include "vulkansift/vulkan_instance.h"

#include "vulkansift/utils/logger.h"
#include <set>
#include <string>

static char LOG_TAG[] = "VulkanInstance";

#ifdef VK_USE_PLATFORM_ANDROID_KHR
bool VulkanInstance::init(ANativeWindow *window, const int window_width, const int window_height, bool use_validation_layers, bool use_debug_extensions)
{
  m_android_native_window = window;
#else
bool VulkanInstance::init(const int window_width, const int window_height, bool use_validation_layers, bool use_debug_extensions)
{
#endif

  m_window_width = window_width;
  m_window_height = window_height;
  m_validation_layer_enabled = use_validation_layers;
  m_debug_ext_enabled = use_debug_extensions;

#ifndef VK_USE_PLATFORM_ANDROID_KHR
  // If were not running android, we need to create our own window
  // We use GLFW for that

  // Try to load GLFW
  if (!loadGLFW())
  {
    logError(LOG_TAG, "Failed to load GLFW at runtime.");
    terminate();
    return false;
  }
  // Init GLFW window
  if (!DL_glfwInit())
  {
    logError(LOG_TAG, "Failed to init GLFW.");
    terminate();
    return false;
  }
  // Create window object
  DL_glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  DL_glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  m_glfw_window = DL_glfwCreateWindow(m_window_width, m_window_height, "VulkanApplication", nullptr, nullptr);
#endif

  // Load Vulkan runtime
  if (!loadVulkan())
  {
    logError(LOG_TAG, "Failed to load Vulkan at runtime.");
    terminate();
    return false;
  }

  if (!createInstance())
  {
    terminate();
    return false;
  }

  if (!createSurface())
  {
    terminate();
    return false;
  }

  if (!findPhysicalDevice())
  {
    terminate();
    return false;
  }

  if (!createLogicalDevice())
  {
    terminate();
    return false;
  }

  if (!createSwapchain())
  {
    terminate();
    return false;
  }

  if (!createSwaphainImageViews())
  {
    terminate();
    return false;
  }

  if (!createRenderPass())
  {
    terminate();
    return false;
  }

  if (!createFramebuffers())
  {
    terminate();
    return false;
  }

  logInfo(LOG_TAG, "VulkanInstance successfully initialized");
  return true;
}

bool VulkanInstance::createInstance()
{
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pEngineName = "";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pApplicationName = "VulkanApplication";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo instance_create_info{};
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pApplicationInfo = &app_info;
  if (m_validation_layer_enabled)
  {
    instance_create_info.enabledLayerCount = 1;
    instance_create_info.ppEnabledLayerNames = m_validation_layers.data();
  }
  else
  {
    instance_create_info.enabledLayerCount = 0;
  }

  ///////////////////////////////////////////////////
  /*uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> extensions{extensionCount};
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
  logInfo(LOG_TAG, "% available extensions:", extensionCount);

  for (const auto &extension : extensions)
  {
    logInfo(LOG_TAG, "\t %s", extension.extensionName);
  }*/
  ///////////////////////////////////////////////////

  // Get list of extensions required by window provider
  std::vector<const char *> instance_extensions{m_fixed_instance_extensions.begin(), m_fixed_instance_extensions.end()};
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  instance_extensions.push_back("VK_KHR_surface");
  instance_extensions.push_back("VK_KHR_android_surface");
#else
  uint32_t glfw_extension_count = 0;
  const char **glfw_extensions;
  glfw_extensions = DL_glfwGetRequiredInstanceExtensions(&glfw_extension_count);
  logInfo(LOG_TAG, "Number of extension required by GLFW: %d", glfw_extension_count);
  for (uint32_t i = 0; i < glfw_extension_count; i++)
  {
    instance_extensions.push_back(glfw_extensions[i]);
    logInfo(LOG_TAG, "\t - %s", glfw_extensions[i]);
  }
#endif
  // Add debug extensions if requested by the user
  if (m_debug_ext_enabled)
  {
    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  instance_create_info.enabledExtensionCount = instance_extensions.size();
  instance_create_info.ppEnabledExtensionNames = instance_extensions.data();

  if (vkCreateInstance(&instance_create_info, nullptr, &m_instance) != VK_SUCCESS)
  {
    logInfo(LOG_TAG, "vkCreateInstance failure");
    return false;
  }
  return true;
}

bool VulkanInstance::createSurface()
{
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  VkAndroidSurfaceCreateInfoKHR surface_create_info{};
  surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  surface_create_info.window = m_android_native_window;
  if (vkCreateAndroidSurfaceKHR(m_instance, &surface_create_info, nullptr, &m_surface) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create surface from Android native window.");
    return false;
  }
#else
  if (DL_glfwCreateWindowSurface(m_instance, m_glfw_window, nullptr, &m_surface) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create surface from GLFW window.");
    return false;
  }
#endif
  return true;
}

bool VulkanInstance::findPhysicalDevice()
{
  uint32_t nb_devices = 0;
  vkEnumeratePhysicalDevices(m_instance, &nb_devices, nullptr);
  logInfo(LOG_TAG, "nb devices: %d", nb_devices);
  if (nb_devices == 0)
  {
    logError(LOG_TAG, "No GPU with Vulkan support found.");
    return false;
  }
  std::vector<VkPhysicalDevice> devices{nb_devices};
  vkEnumeratePhysicalDevices(m_instance, &nb_devices, devices.data());

  for (const auto &device : devices)
  {
    if (isPhysicalDeviceValid(device))
    {
      m_physical_device = device;
      break;
    }
  }

  if (m_physical_device == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "Could not find a suitable GPU.");
    return false;
  }

  // Print out informations about selected device
  VkPhysicalDeviceProperties device_properties;
  vkGetPhysicalDeviceProperties(m_physical_device, &device_properties);
  logInfo(LOG_TAG, "Using GPU: %s [ID=%d]", device_properties.deviceName, device_properties.deviceID);
  logInfo(LOG_TAG, "Period: %f", device_properties.limits.timestampPeriod);
  logInfo(LOG_TAG, "Ts: %d", device_properties.limits.timestampComputeAndGraphics);

  // Choose queue family indices
  // For now we try to find a single queue family that allows to do graphic, present, compute and transfer
  // Other queues could be used for async compute or transfer
  bool general_family_found = false;
  bool async_compute_queue_family_found = false;
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> queue_families{queue_family_count};
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_families.data());
  int i = 0;
  for (const auto &queue_family : queue_families)
  {
    VkBool32 support_present = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, m_surface, &support_present);
    if (!general_family_found && (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT && support_present && queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT))
    {
      // Queue with general capabilities found
      m_general_queue_family_index = i;
      general_family_found = true;
      logInfo(LOG_TAG, "Graphics/Compute/Present queue family (%d): %d queue(s)", m_general_queue_family_index, queue_family.queueCount);
    }

    if (!async_compute_queue_family_found && (!(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)))
    {
      // Queue with async compute capabilities found
      m_async_compute_available = true;
      m_async_compute_queue_family_index = i;
      async_compute_queue_family_found = true;
      logInfo(LOG_TAG, "Async compute queue family (%d): %d queue(s)", m_async_compute_queue_family_index, queue_family.queueCount);
    }

    i++;
  }

  logInfo(LOG_TAG, "%d", queue_families[m_general_queue_family_index].timestampValidBits);
  m_ts_query_supported = device_properties.limits.timestampComputeAndGraphics;
  m_ts_query_period = device_properties.limits.timestampPeriod;
  uint32_t nb_valid_bits_timestamp = queue_families[m_general_queue_family_index].timestampValidBits;
  if (nb_valid_bits_timestamp == 0u)
  {
    m_ts_query_supported = false;
  }
  else
  {
    m_ts_bitmask = 0u;
    for (uint32_t i = 0; i < nb_valid_bits_timestamp; i++)
    {
      m_ts_bitmask |= 1 << i;
    }
  }

  m_physical_device_props = device_properties;

  return true;
}

bool VulkanInstance::isPhysicalDeviceValid(const VkPhysicalDevice &device)
{
  bool is_valid_GPU = false;
  bool has_swapchain_extensions = false;
  bool has_valid_queues = false;
  bool has_valid_swapchain = false;
  {
    // Check if the device is a valid GPU
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    logInfo(LOG_TAG, "device_properties.deviceType %d", device_properties.deviceType);
    is_valid_GPU =
        (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
  }
  {
    // Check if device supports swapchains with extension VK_KHR_SWAPCHAIN_EXTENSION_NAME
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions{extension_count};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());
    std::set<std::string> required_device_extensions{m_device_extensions.begin(), m_device_extensions.end()};
    for (const auto &extension : available_extensions)
    {
      logInfo(LOG_TAG, "ext: %s", extension.extensionName);
      required_device_extensions.erase(extension.extensionName);
    }
    /*for (auto extension : required_device_extensions)
    {
      logInfo(LOG_TAG, "remaining: %s", extension.c_str());
    }*/
    if (required_device_extensions.empty())
    {
      has_swapchain_extensions = true;
    }
  }

  {
    // Check if the device queue families support graphics, present, compute and transfer commands
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families{queue_family_count};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
    bool graphics_ok = false;
    bool present_ok = false;
    bool compute_ok = false;
    int i = 0;
    for (const auto &queue_family : queue_families)
    {
      VkBool32 support_present = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &support_present);
      if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
        graphics_ok = true;
      }
      if (support_present)
      {
        present_ok = true;
      }
      if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)
      {
        compute_ok = true;
      }
      if (graphics_ok && present_ok && compute_ok)
      {
        break;
      }
      i++;
    }

    i = 0;
    for (const auto &queue_family : queue_families)
    {
      VkBool32 support_present = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &support_present);
      bool gqueue = queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
      bool tqueue = queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT;
      bool cqueue = queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT;
      logError(LOG_TAG, "Queue %d --> Present %d Graphics %d Compute %d Transfer %d", i, support_present, gqueue, cqueue, tqueue);
      i++;
    }

    if (graphics_ok && present_ok && compute_ok)
    {
      has_valid_queues = true;
    }
  }
  {
    // Check if the swapchain has at least one format and one presentation mode
    uint32_t swapchain_nb_format, swapchain_nb_present_mode;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &swapchain_nb_format, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &swapchain_nb_present_mode, nullptr);
    has_valid_swapchain = swapchain_nb_format != 0 && swapchain_nb_present_mode != 0;
  }
  return is_valid_GPU && has_swapchain_extensions && has_valid_queues && has_valid_swapchain;
}

bool VulkanInstance::createLogicalDevice()
{
  std::vector<VkDeviceQueueCreateInfo> queue_create_infos{};

  float queue_priority = 1.f;
  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = m_general_queue_family_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;
  queue_create_infos.push_back(queue_create_info);

  if (m_async_compute_available)
  {
    queue_priority = 0.5f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = m_async_compute_queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);
  }

  // No device features
  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = queue_create_infos.data();
  createInfo.queueCreateInfoCount = queue_create_infos.size();
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.ppEnabledExtensionNames = m_device_extensions.data();
  createInfo.enabledExtensionCount = m_device_extensions.size();

  if (m_validation_layer_enabled)
  {
    createInfo.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
    createInfo.ppEnabledLayerNames = m_validation_layers.data();
  }
  else
  {
    createInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(m_physical_device, &createInfo, nullptr, &m_device) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create logical device");
    return false;
  }

  vkGetDeviceQueue(m_device, m_general_queue_family_index, 0, &m_general_queue);
  if (m_async_compute_available)
  {
    vkGetDeviceQueue(m_device, m_async_compute_queue_family_index, 0, &m_async_compute_queue);
  }

  // Load debug functions if needed
  if (m_debug_ext_enabled)
  {
    vkDebugMarkerSetObjectTagEXT = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(m_device, "vkDebugMarkerSetObjectTagEXT");
    vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(m_device, "vkDebugMarkerSetObjectNameEXT");
    vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(m_device, "vkCmdDebugMarkerBeginEXT");
    vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(m_device, "vkCmdDebugMarkerEndEXT");
    vkCmdDebugMarkerInsertEXT = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(m_device, "vkCmdDebugMarkerInsertEXT");
    if (vkDebugMarkerSetObjectTagEXT != nullptr && vkDebugMarkerSetObjectNameEXT != nullptr && vkCmdDebugMarkerBeginEXT != nullptr &&
        vkCmdDebugMarkerEndEXT != nullptr && vkCmdDebugMarkerInsertEXT != nullptr)
    {
      logInfo(LOG_TAG, "Marker functions successfully loaded");
      m_debug_funcs_available = true;
    }
    else
    {
      logError(LOG_TAG, "Failed to load marker related debug functions");
      m_debug_funcs_available = false;
    }
  }

  return true;
}

bool VulkanInstance::createSwapchain()
{

  // Store swapchain informations (present modes, formats, etc)
  // Get swapchain capabilities
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &m_swapchain_capabilities);
  // Get swapchain formats
  uint32_t nb_format;
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &nb_format, nullptr);
  if (nb_format != 0)
  {
    m_swapchain_available_formats.resize(nb_format);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &nb_format, m_swapchain_available_formats.data());
  }
  // Get swapchain present modes
  uint32_t nb_present_mode;
  vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &nb_present_mode, nullptr);
  if (nb_present_mode != 0)
  {
    m_swapchain_available_present_modes.resize(nb_present_mode);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &nb_present_mode, m_swapchain_available_present_modes.data());
  }

  // Choose swapchain format
  VkSurfaceFormatKHR surface_format = m_swapchain_available_formats[0];
  for (auto format_khr : m_swapchain_available_formats)
  {
    if (format_khr.format == VK_FORMAT_R8G8B8A8_SRGB && format_khr.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      surface_format = format_khr;
      break;
    }
  }
  // Choose swapchain present mode
  VkPresentModeKHR swapchain_present_mode = m_swapchain_available_present_modes[0];
  for (auto present_mode : m_swapchain_available_present_modes)
  {
    if (present_mode == VK_PRESENT_MODE_FIFO_KHR)
    {
      swapchain_present_mode = present_mode;
      break;
    }
  }

  uint32_t nb_image_min = m_swapchain_capabilities.minImageCount + 1;
  if (m_swapchain_capabilities.maxImageCount > 0 && nb_image_min > m_swapchain_capabilities.maxImageCount)
  {
    nb_image_min = m_swapchain_capabilities.maxImageCount;
  }

  VkExtent2D swapchain_extent;
  if (m_swapchain_capabilities.currentExtent.width != UINT32_MAX && m_swapchain_capabilities.currentExtent.height != UINT32_MAX)
  {
    swapchain_extent = m_swapchain_capabilities.currentExtent;
  }
  else
  {
    swapchain_extent = {static_cast<uint32_t>(m_window_width), static_cast<uint32_t>(m_window_height)};

    swapchain_extent.width =
        std::max(m_swapchain_capabilities.minImageExtent.width, std::min(m_swapchain_capabilities.maxImageExtent.width, swapchain_extent.width));
    swapchain_extent.height =
        std::max(m_swapchain_capabilities.minImageExtent.height, std::min(m_swapchain_capabilities.maxImageExtent.height, swapchain_extent.height));
  }

  // Create swapchain
  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = m_surface;

  create_info.minImageCount = nb_image_min;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.presentMode = swapchain_present_mode;
  create_info.clipped = VK_TRUE;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.preTransform = m_swapchain_capabilities.currentTransform;
  create_info.oldSwapchain = VK_NULL_HANDLE;
  create_info.imageExtent = swapchain_extent;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.imageArrayLayers = 1;
  create_info.queueFamilyIndexCount = 0;
  create_info.pQueueFamilyIndices = nullptr;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create swapchain");
    return false;
  }

  m_swapchain_format = surface_format.format;
  m_swapchain_extent = swapchain_extent;

  // Get number of swapchain image
  if (vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_nb_swapchain_image, nullptr) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to get the number of swapchain image.");
    return false;
  }
  logInfo(LOG_TAG, "Nb swapchain image: %d", m_nb_swapchain_image);
  m_swapchain_images.resize(m_nb_swapchain_image);
  if (vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_nb_swapchain_image, m_swapchain_images.data()) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to get swapchain images.");
    return false;
  }

  return true;
}

bool VulkanInstance::createSwaphainImageViews()
{
  m_swapchain_image_views.resize(m_nb_swapchain_image);
  for (size_t i = 0; i < m_nb_swapchain_image; i++)
  {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = m_swapchain_images[i];
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = m_swapchain_format;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_device, &create_info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to get image views.");
      return false;
    }
  }
  return true;
}

bool VulkanInstance::createRenderPass()
{
  VkAttachmentDescription color_attachment{.format = m_swapchain_format,
                                           .samples = VK_SAMPLE_COUNT_1_BIT,
                                           .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                           .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                           .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                           .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                           .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

  VkAttachmentReference color_attachment_reference{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpass{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &color_attachment_reference};

  VkSubpassDependency dependency{.srcSubpass = VK_SUBPASS_EXTERNAL,
                                 .dstSubpass = 0,
                                 .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 .srcAccessMask = 0,
                                 .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};

  VkRenderPassCreateInfo render_pass_create_info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                                 .attachmentCount = 1,
                                                 .pAttachments = &color_attachment,
                                                 .subpassCount = 1,
                                                 .pSubpasses = &subpass,

                                                 .dependencyCount = 1,
                                                 .pDependencies = &dependency};

  if (vkCreateRenderPass(m_device, &render_pass_create_info, nullptr, &m_render_pass) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create render pass");
    return false;
  }
  return true;
}

bool VulkanInstance::createFramebuffers()
{
  m_swapchain_framebuffers.resize(m_nb_swapchain_image);
  for (size_t i = 0; i < m_nb_swapchain_image; i++)
  {
    VkImageView attachments[] = {m_swapchain_image_views[i]};
    VkFramebufferCreateInfo framebuffer_info{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                             .renderPass = m_render_pass,
                                             .attachmentCount = 1,
                                             .pAttachments = attachments,
                                             .width = m_swapchain_extent.width,
                                             .height = m_swapchain_extent.height,
                                             .layers = 1};
    if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swapchain_framebuffers[i]) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create framebuffers.");
      return false;
    }
  }
  return true;
}

void VulkanInstance::cleanupSwapchain()
{
  // Destroy framebuffers
  for (auto framebuffer : m_swapchain_framebuffers)
  {
    vkDestroyFramebuffer(m_device, framebuffer, nullptr);
  }
  m_swapchain_framebuffers.clear();

  // Destroy render pass
  if (m_render_pass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(m_device, m_render_pass, nullptr);
    m_render_pass = VK_NULL_HANDLE;
  }

  // Destroy image views
  for (auto image_view : m_swapchain_image_views)
  {
    vkDestroyImageView(m_device, image_view, nullptr);
  }
  m_swapchain_image_views.clear();

  // Destroy swapchain
  if (m_swapchain != VK_NULL_HANDLE)
  {
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
  }
}

void VulkanInstance::cleanupVulkanEntities()
{
  // Cleanup swapchain
  cleanupSwapchain();

  // Destroy logical device
  if (m_device != VK_NULL_HANDLE)
  {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }
  // Destroy surface
  if (m_surface != VK_NULL_HANDLE)
  {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
  }
  // Destroy instance
  if (m_instance != VK_NULL_HANDLE)
  {
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
  }
}

bool VulkanInstance::shouldStop()
{
#ifndef VK_USE_PLATFORM_ANDROID_KHR
  DL_glfwPollEvents();
  return DL_glfwWindowShouldClose(m_glfw_window);
#else
  return false;
#endif
}

bool VulkanInstance::resetSwapchain()
{
  // Destroy framebuffers
  for (auto framebuffer : m_swapchain_framebuffers)
  {
    vkDestroyFramebuffer(m_device, framebuffer, nullptr);
  }
  m_swapchain_framebuffers.clear();

  // Destroy render pass
  vkDestroyRenderPass(m_device, m_render_pass, nullptr);

  // Destroy swapchain image views
  for (auto imageView : m_swapchain_image_views)
  {
    vkDestroyImageView(m_device, imageView, nullptr);
  }
  m_swapchain_image_views.clear();

  // Destroy swapchain
  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

  // Recreate swapchain
  if (!createSwapchain())
  {
    return false;
  }

  if (!createSwaphainImageViews())
  {
    return false;
  }

  if (!createRenderPass())
  {
    return false;
  }

  if (!createFramebuffers())
  {
    return false;
  }

  return true;
}

void VulkanInstance::terminate()
{
  // Destroy Vulkan objects
  cleanupVulkanEntities();

#ifndef VK_USE_PLATFORM_ANDROID_KHR
  // Destroy GLFW window and terminate
  if (m_glfw_window != nullptr)
  {
    DL_glfwDestroyWindow(m_glfw_window);
    m_glfw_window = nullptr;
  }
  DL_glfwTerminate();
  // Unload GLFW
  unloadGLFW();
#endif

  // Unload Vulkan
  unloadVulkan();
  logInfo(LOG_TAG, "VulkanInstance destroyed");
}