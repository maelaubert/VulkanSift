#ifndef GLFW_LOADER_H
#define GLFW_LOADER_H

#include <GLFW/glfw3.h>
#include <cstdint>

#define VK_NO_PROTOTYPES 1
#include <vulkan/vulkan.h>

typedef int (*PFN_glfwInit)(void);
typedef void (*PFN_glfwWindowHint)(int hint, int value);
typedef GLFWwindow *(*PFN_glfwCreateWindow)(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share);
typedef VkResult (*PFN_glfwCreateWindowSurface)(VkInstance instance, GLFWwindow *window, const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface);
typedef const char **(*PFN_glfwGetRequiredInstanceExtensions)(uint32_t *count);
typedef void (*PFN_glfwDestroyWindow)(GLFWwindow *window);
typedef void (*PFN_glfwTerminate)(void);
typedef void (*PFN_glfwPollEvents)(void);
typedef int (*PFN_glfwWindowShouldClose)(GLFWwindow *window);

extern PFN_glfwInit DL_glfwInit;
extern PFN_glfwWindowHint DL_glfwWindowHint;
extern PFN_glfwCreateWindow DL_glfwCreateWindow;
extern PFN_glfwCreateWindowSurface DL_glfwCreateWindowSurface;
extern PFN_glfwGetRequiredInstanceExtensions DL_glfwGetRequiredInstanceExtensions;
extern PFN_glfwDestroyWindow DL_glfwDestroyWindow;
extern PFN_glfwTerminate DL_glfwTerminate;
extern PFN_glfwPollEvents DL_glfwPollEvents;
extern PFN_glfwWindowShouldClose DL_glfwWindowShouldClose;

bool loadGLFW();
void unloadGLFW();

#endif // GLFW_LOADER_H