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

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}