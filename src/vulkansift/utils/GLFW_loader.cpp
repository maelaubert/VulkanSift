#include "vulkansift/utils/GLFW_loader.h"
#include "vulkansift/utils/logger.h"
#include <dlfcn.h>

static char LOG_TAG[] = "GLFWLoader";

PFN_glfwInit DL_glfwInit;
PFN_glfwWindowHint DL_glfwWindowHint;
PFN_glfwCreateWindow DL_glfwCreateWindow;
PFN_glfwCreateWindowSurface DL_glfwCreateWindowSurface;
PFN_glfwGetRequiredInstanceExtensions DL_glfwGetRequiredInstanceExtensions;
PFN_glfwDestroyWindow DL_glfwDestroyWindow;
PFN_glfwTerminate DL_glfwTerminate;
PFN_glfwPollEvents DL_glfwPollEvents;
PFN_glfwWindowShouldClose DL_glfwWindowShouldClose;

static void *glfw_lib_handle = nullptr;

bool loadGLFWSymbol(void *lib_handle, void **symbol_target, const char *symbol_name)
{
  dlerror(); // Clear error code
  bool res = true;
  *symbol_target = dlsym(lib_handle, symbol_name);
  const char *dlsym_error = dlerror();
  if (dlsym_error)
  {
    logError(LOG_TAG, "Failed to load GLFW symbol (%s)", dlsym_error);
    res = false;
  }
  return res;
}

bool loadGLFW()
{
  // Open GLFW dynamic library
  glfw_lib_handle = dlopen("libglfw.so", RTLD_NOW);
  if (glfw_lib_handle == nullptr)
  {
    logError(LOG_TAG, "Failed to open libglfw.so");
    return false;
  }

  // Load GLFW required symbols
  bool symbol_loading_ok = true;
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwInit, "glfwInit");
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwWindowHint, "glfwWindowHint");
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwCreateWindow, "glfwCreateWindow");
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwCreateWindowSurface, "glfwCreateWindowSurface");
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwGetRequiredInstanceExtensions, "glfwGetRequiredInstanceExtensions");
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwDestroyWindow, "glfwDestroyWindow");
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwTerminate, "glfwTerminate");
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwPollEvents, "glfwPollEvents");
  symbol_loading_ok &= loadGLFWSymbol(glfw_lib_handle, (void **)&DL_glfwWindowShouldClose, "glfwWindowShouldClose");

  // If any error occured when loading symbols
  if (!symbol_loading_ok)
  {
    dlclose(glfw_lib_handle);
    glfw_lib_handle = nullptr;
    return false;
  }
  return true;
}

void unloadGLFW()
{
  if (glfw_lib_handle != nullptr)
  {
    dlclose(glfw_lib_handle);
  }
}