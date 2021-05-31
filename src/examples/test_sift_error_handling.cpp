#include <vulkansift/vulkansift.h>

#include <iostream>
#include <vector>

// Define the error callback function used to throw exceptions
void vulkansift_error_callback(vksift_Result result)
{
  std::cout << "vulkansift_error_callback() called !" << std::endl;
  if (result == VKSIFT_VULKAN_ERROR)
  {
    throw std::runtime_error{"Vulkan related failure detected in VulkanSift functions. vksift_Instance must be destroyed."};
  }
  else if (result == VKSIFT_INVALID_INPUT_ERROR)
  {
    throw std::invalid_argument{"Invalid argument detected in VulkanSift functions. vksift_Instance can still be used."};
  }
}

int main()
{
  int NB_BUFF = 5;

  vksift_setLogLevel(VKSIFT_LOG_INFO);

  // Load the Vulkan API (should never be called more than once per program)
  if (vksift_loadVulkan() != VKSIFT_SUCCESS)
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  // Create a vksift instance using the default configuration
  vksift_Config config = vksift_getDefaultConfig();
  // Configure our callback function so it will be called when something goes wrong.
  config.on_error_callback_function = vulkansift_error_callback;
  config.sift_buffer_count = NB_BUFF;

  vksift_Instance vksift_instance = NULL;
  if (vksift_createInstance(&vksift_instance, &config) != VKSIFT_SUCCESS)
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  std::cout << NB_BUFF << " SIFT GPU buffers reserved." << std::endl;

  try
  {

    for (int i = 0; i < NB_BUFF * 2; i++)
    {

      std::cout << "Trying to access buffer " << i << std::endl;
      // Calling this with any buffer_idx argument >=NB_BUFF will cause an invalid argument error,
      // our error callback function will be called and throw the std::invalid_argument exception.
      vksift_getFeaturesNumber(vksift_instance, i);
      std::cout << "Result valid." << std::endl;
    }
  }
  catch (std::invalid_argument e)
  {
    std::cout << "std::invalid_argument exception catched: " << e.what() << std::endl;
  }
  catch (std::runtime_error e)
  {
    std::cout << "std::runtime_error exception catched: " << e.what() << std::endl;
  }

  // Release vksift instance and API
  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}