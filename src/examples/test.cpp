extern "C"
{
#include "vulkansift/vulkansift.h"
}

#include <cstring>
#include <iostream>

int main()
{
  vksift_setLogLevel(VKSIFT_LOG_INFO);

  if (!vksift_loadVulkan())
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  vksift_Config config = vksift_Config_Default;
  vksift_Instance vksift_instance = NULL;
  if (!vksift_createInstance(&vksift_instance, &config))
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  while (vksift_presentDebugFrame(vksift_instance))
  {
  }

  std::cout << "Input 640 480" << std::endl;
  vksift_detectFeatures(vksift_instance, NULL, 640, 480, 0);

  std::cout << "Input 1920 1080" << std::endl;
  vksift_detectFeatures(vksift_instance, NULL, 1920, 1080, 0);

  std::cout << "Input 1918 1079" << std::endl;
  vksift_detectFeatures(vksift_instance, NULL, 1918, 1079, 0);

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}