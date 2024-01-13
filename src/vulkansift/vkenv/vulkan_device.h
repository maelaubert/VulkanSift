#ifndef VKENV_DEVICE_H
#define VKENV_DEVICE_H

#ifdef VK_NO_PROTOTYPES
#include "volk/volk.h"
#else
#include <vulkan/vulkan.h>
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct vkenv_Device_T
{
  // Devices and properties
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceProperties physical_device_props;
  VkPhysicalDeviceMemoryProperties physical_device_memory_props;
  // General purpose queue
  VkQueue *general_queues;
  uint32_t general_queue_cnt;
  uint32_t general_queues_family_idx;
  // Asynchronous compute queue
  bool async_compute_available;
  VkQueue *async_compute_queues;
  uint32_t async_compute_queue_cnt;
  uint32_t async_compute_queues_family_idx;
  // Asynchronous transfer queue
  bool async_transfer_available;
  VkQueue *async_transfer_queues;
  uint32_t async_transfer_queue_cnt;
  uint32_t async_transfer_queues_family_idx;
} * vkenv_Device;

typedef struct
{
  // Vulkan application info
  const char *application_name;
  uint32_t application_version;
  const char *engine_name;
  uint32_t engine_version;
  uint32_t vulkan_api_version;
  // Vulkan instance requirements
  uint32_t validation_layer_count;
  const char **validation_layers;
  uint32_t instance_extension_count;
  const char **instance_extensions;
} vkenv_InstanceConfig;

typedef struct
{
  // Vulkan device requirements
  uint32_t device_extension_count;
  const char **device_extensions;
  // Queue configuration
  // If the device cannot provide the number of general queue the device creation will fail
  uint32_t nb_general_queues;
  // If the device cannot provide the number of async queues they will be tagged as unavailable
  uint32_t nb_async_compute_queues;
  uint32_t nb_async_transfer_queues;

  // GPU selection config
  // If target_device_idx<0, the GPU with best performances is chosen automatically
  int32_t target_device_idx;
} vkenv_DeviceConfig;

// Create a Vulkan instance and load the Vulkan API. There should never be more than one instance per process since the Vulkan API
// functions are instance dependencies and are accessed globally.
// Calling vkenv_createInstance() is mandatory for any Vulkan application using vkenv.
// Returns true if the API is successfully loaded and the instance created
// Returns false if the API functions can't be loaded, if the instance creation failed or if an instance is already present.
bool vkenv_createInstance(vkenv_InstanceConfig *config_ptr);

// Only used to provide instance access to window system interfaces (WSI) and to load extension functions
VkInstance vkenv_getInstance();

// Destroy the Vulkan instance and unload the Vulkan API.
void vkenv_destroyInstance();

// Allocate and setup a Vulkan device structure according to the provided configuration, this basically:
//  - get access to the GPU mentionned in the user configuration (or automatically select the most appropriate)
//  - retrieve the GPU relevent information (device properties, device memory properties and queues info)
//  - create a Vulkan logical device for the GPU and provide access to the GPU queues
// Return true on success, false on failure.
bool vkenv_createDevice(vkenv_Device *device_ptr, vkenv_DeviceConfig *config_ptr);
// Destroy Vulkan entities created during vkenv_createDevice() and free any allocated memory
void vkenv_destroyDevice(vkenv_Device *device_ptr);

// If "devices" is NULL, fill "nb_devices" with the number of available physical device
// If "devices" is not null, "nb_devices" devices info are copied into "devices"
void vkenv_getPhysicalDevicesProperties(uint32_t *nb_devices, VkPhysicalDeviceProperties *devices);

#endif // VKENV_DEVICE_H