#include <vulkan/vulkan.h>

#ifdef __ANDROID__
#include "native_app_glue/android_native_app_glue.h"
#else
#include <GLFW/glfw3.h>
#endif

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include "logger.hpp"

std::vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
bool enableValidationLayers = false;
#else
bool enableValidationLayers = true;
#endif

VkInstance instance;
VkDebugUtilsMessengerEXT debugMessenger;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  LOG_DEBUG("Validation Layer: {}", pCallbackData->pMessage);
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
  if (!enableValidationLayers) return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  populateDebugMessengerCreateInfo(createInfo);

  if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
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

void cleanUp()
{
  if (enableValidationLayers) {
    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
  }

  vkDestroyInstance(instance, nullptr);
}

#ifdef __ANDROID__
void android_main(struct android_app *app)
{
  createInstance(&instance);
  setupDebugMessenger();
  cleanUp();
}
#else
int main()
{
  createInstance(&instance);
  setupDebugMessenger();
  cleanUp();
}
#endif