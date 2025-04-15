#ifdef __ANDROID__
#define VK_USE_PLATFORM_ANDROID_KHR
#include "native_app_glue/android_native_app_glue.h"
struct GLFWwindow;
#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
struct android_app;
#endif

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>
#include <optional>
#include <set>

#include "logger.hpp"

std::vector<const char*> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

std::vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
bool enableValidationLayers = false;
#else
bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices
{
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;
};

struct VulkanConfig
{
  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device;
  VkQueue graphicsQueue;
  VkQueue presentQueue;
  VkSurfaceKHR surface;
};

VulkanConfig vulkanConfig = {};
bool running = true;
bool focused = false;

GLFWwindow *glfwWindow = nullptr;
android_app *androidApp = nullptr;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  LOG_DEBUG("VL: {}", pCallbackData->pMessage);
  return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

void setupDebugMessenger()
{
  LOG_DEBUG("Creating debug messenger");
  if (!enableValidationLayers) return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  populateDebugMessengerCreateInfo(createInfo);

  if (CreateDebugUtilsMessengerEXT(vulkanConfig.instance, &createInfo, nullptr, &vulkanConfig.debugMessenger) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create debug messenger");
  }
}

bool checkValidationLayerSupport()
{
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> layersAvailable(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, layersAvailable.data());

  for(auto layer : validationLayers)
  {
    bool layerFound = false;

    for(auto layerAvailable : layersAvailable)
    {
      if(std::string(layer) == std::string(layerAvailable.layerName))
      {
        layerFound = true;
        break;
      }
    }

    if(!layerFound)
    {
      return false;
    }
  }

  return true;
}

void createInstance(VkInstance *instance)
{
  LOG_DEBUG("Creating vulkan instance");
  VkApplicationInfo applicationInfo = {};
  applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pNext = nullptr;
  applicationInfo.pApplicationName = "android-vulkan";
  applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  applicationInfo.pEngineName = "android-vulkan engine";
  applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  applicationInfo.apiVersion = VK_API_VERSION_1_3;

  uint32_t enabledExtensionCount;
  vkEnumerateInstanceExtensionProperties(nullptr, &enabledExtensionCount, nullptr);
  std::vector<VkExtensionProperties> extensions(enabledExtensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &enabledExtensionCount, extensions.data());

  std::vector<char *> enabledExtensionNames;
  enabledExtensionNames.reserve(enabledExtensionCount);

  LOG_DEBUG("Vulkan Extension Count: {}", enabledExtensionCount);

  for(auto &ext : extensions)
  {
    enabledExtensionNames.push_back(ext.extensionName);
    LOG_DEBUG("Vulkan Extension Enabled: {}", ext.extensionName);
  }

  if(enableValidationLayers && !checkValidationLayerSupport())
  {
    LOG_DEBUG("Validation layers not available");
  }

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = nullptr;
  // instanceCreateInfo.flags;
  instanceCreateInfo.pApplicationInfo = &applicationInfo;
  instanceCreateInfo.enabledLayerCount = 0;

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {}; // should be outside, because of scope
  if(enableValidationLayers)
  {
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    populateDebugMessengerCreateInfo(debugCreateInfo);
    instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
  }

  instanceCreateInfo.enabledExtensionCount = enabledExtensionCount;
  instanceCreateInfo.ppEnabledExtensionNames = enabledExtensionNames.data();

  VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, instance);

  if(result != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create vulkan instance");
  }
}

void createSurface()
{
  LOG_DEBUG("Creating vulkan surface");

  #ifdef __ANDROID__
  VkAndroidSurfaceCreateInfoKHR createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = nullptr;
  // createInfo.flags;
  createInfo.window = androidApp->window;

  if(vkCreateAndroidSurfaceKHR(vulkanConfig.instance, &createInfo, nullptr, &vulkanConfig.surface) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create android vulkan surface");
  }
  #else
  if(glfwCreateWindowSurface(vulkanConfig.instance, glfwWindow, nullptr, &vulkanConfig.surface) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create desktop vulkan surface");
  }
  #endif
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
{
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

  for(int i = 0; i < queueFamilyProperties.size(); i++)
  {
    if(indices.graphicsFamily.has_value() && indices.presentFamily.has_value())
    {
      break;
    }

    VkQueueFamilyProperties queueFamily = queueFamilyProperties[i];

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vulkanConfig.surface, &presentSupport);

    if(presentSupport)
    {
      indices.presentFamily = i;
    }

    if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      indices.graphicsFamily = i;
    }
  }

  return indices;
}

bool checkDeviceExtensionsSupporte(VkPhysicalDevice device)
{
  uint32_t extensionsCount;
  vkEnumerateDeviceExtensionProperties(vulkanConfig.physicalDevice, nullptr, &extensionsCount, nullptr);

  std::vector<VkExtensionProperties> extensionProperties(extensionsCount);
  vkEnumerateDeviceExtensionProperties(vulkanConfig.physicalDevice, nullptr, &extensionsCount, extensionProperties.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

  for(auto extension : extensionProperties)
  {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

bool isDeviceSuitable(VkPhysicalDevice device)
{
  QueueFamilyIndices indices = findQueueFamilies(device);
  bool extensionsSupported = checkDeviceExtensionsSupporte(device);
  return indices.graphicsFamily.has_value() && indices.presentFamily.has_value();
}

void pickPhysicalDevice()
{
  uint32_t physicalDeviceCount;
  vkEnumeratePhysicalDevices(vulkanConfig.instance, &physicalDeviceCount, nullptr);
  std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
  vkEnumeratePhysicalDevices(vulkanConfig.instance, &physicalDeviceCount, physicalDevices.data());

  LOG_DEBUG("Found {} devices with vulkan support", physicalDeviceCount);

  for(auto &device : physicalDevices)
  {
    if(isDeviceSuitable(device))
    {
      vulkanConfig.physicalDevice = device;
      break;
    }
  }

  if(vulkanConfig.physicalDevice == VK_NULL_HANDLE)
  {
    LOG_DEBUG("Failed to find a suitable device");
  }
}

void createLogicalDevice()
{
  QueueFamilyIndices indices = findQueueFamilies(vulkanConfig.physicalDevice);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for(auto familyIndex : uniqueFamilies)
  {
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = nullptr;
    // queueCreateInfo.flags;
    queueCreateInfo.queueFamilyIndex = familyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }


  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.pNext = nullptr;
  // deviceCreateInfo.flags;
  deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
  // deviceCreateInfo.enabledLayerCount;
  // deviceCreateInfo.ppEnabledLayerNames;
  deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

  VkPhysicalDeviceFeatures deviceFeatures = {};
  deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

  if(vkCreateDevice(vulkanConfig.physicalDevice, &deviceCreateInfo, nullptr, &vulkanConfig.device) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create logical device");
  }

  vkGetDeviceQueue(vulkanConfig.device, indices.graphicsFamily.value(), 0, &vulkanConfig.graphicsQueue);
  vkGetDeviceQueue(vulkanConfig.device, indices.presentFamily.value(), 0, &vulkanConfig.presentQueue);
}

void cleanUp()
{
  if (enableValidationLayers) {
    DestroyDebugUtilsMessengerEXT(vulkanConfig.instance, vulkanConfig.debugMessenger, nullptr);
  }

  vkDestroyDevice(vulkanConfig.device, nullptr);
  vkDestroySurfaceKHR(vulkanConfig.instance, vulkanConfig.surface, nullptr);
  vkDestroyInstance(vulkanConfig.instance, nullptr);

  #ifndef __ANDROID__
  glfwDestroyWindow(glfwWindow);
  glfwTerminate();
  #endif
}

void initVulkan()
{
  LOG_DEBUG("Initializing Vulkan");
  createInstance(&vulkanConfig.instance);
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
}

void pollEvents()
{
  #ifdef __ANDROID__
  int events = 0;
  struct android_poll_source *source;
  int timeout = 0;

  while(ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0)
  {
    if(source) source->process(androidApp, source);
  }
  #else
  glfwPollEvents();
  running = !glfwWindowShouldClose(glfwWindow);
  #endif
}

void centerGLFWWindow(GLFWwindow *window)
{
  #ifndef __ANDROID__
  // Get monitors
  int monitorCount = 0;
  GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);
  // Get monitor position
  int monitorX = 0;
  int monitorY = 0;
  glfwGetMonitorPos(monitors[1], &monitorX, &monitorY);
  // Get monitor dimensions
  const GLFWvidmode *videoMode = glfwGetVideoMode(monitors[1]);
  // Get window dimensions
  int windowWidth = 0;
  int windowHeight = 0;
  glfwGetWindowSize(window, &windowWidth, &windowHeight);
  // Get frame dimensions
  int frameTop = 0;
  int frameLeft = 0;
  int frameRight = 0;
  int frameBottom = 0;
  glfwGetWindowFrameSize(window, &frameLeft, &frameTop, &frameRight, &frameBottom);
  // Calculate center position
  int centerX = monitorX + videoMode->width * 0.5f - windowWidth * 0.5f;
  int centerY = monitorY + frameTop + videoMode->height * 0.5f - windowHeight * 0.5f;
  // Center window
  glfwSetWindowPos(window, centerX, centerY);
  #endif
}

void initPlatform(android_app *app, int32_t cmd)
{
  #ifdef __ANDROID__
  switch(cmd)
  {
    case APP_CMD_INIT_WINDOW:
    LOG_DEBUG("Initializing Android platform");
    LOG_DEBUG("APP_CMD_INIT_WINDOW");
    if(androidApp->window)
    {
      running = true;
      initVulkan();
    }
    break;
    case APP_CMD_TERM_WINDOW:
    LOG_DEBUG("APP_CMD_TERM_WINDOW");
    running = false;
    break;
    case APP_CMD_GAINED_FOCUS:
    LOG_DEBUG("APP_CMD_GAINED_FOCUS");
    focused = true;
    break;
    case APP_CMD_LOST_FOCUS:
    LOG_DEBUG("APP_CMD_LOST_FOCUS");
    focused = false;
    break;
    case APP_CMD_DESTROY:
    LOG_DEBUG("APP_CMD_DESTROY");
    running = false;
    break;
  }
  #else
  LOG_DEBUG("Initializing Desktop platform");
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  glfwWindow = glfwCreateWindow(1280, 720, "Vulkan", nullptr, nullptr);
  centerGLFWWindow(glfwWindow);
  initVulkan();
  #endif
}

void run()
{
  #ifdef __ANDROID__
  androidApp->onAppCmd = initPlatform;
  #else
  initPlatform(nullptr, 0);
  #endif

  while(running)
  {
    pollEvents();
  }

  cleanUp();
}

#ifdef __ANDROID__
void android_main(struct android_app *app)
{
  androidApp = app;
  run();
}
#else
int main()
{
  run();
}
#endif