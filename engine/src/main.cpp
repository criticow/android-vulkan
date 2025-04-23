#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef __ANDROID__
#define VK_USE_PLATFORM_ANDROID_KHR
#include "native_app_glue/android_native_app_glue.h"
#include <android/imagedecoder.h>
#include <android/asset_manager.h>
struct GLFWwindow;
#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
struct android_app;
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>

#include <chrono>

#include "logger.hpp"

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
  }
};

const std::vector<Vertex> vertices = {
  {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
  {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
  {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
  {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
  0, 1, 2, 2, 3, 0
};

struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

std::vector<const char*> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

std::vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

const int MAX_FRAMES_IN_FLIGHT = 2;

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

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
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
  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;
  std::vector<VkImageView> swapChainImageViews;
  VkRenderPass renderPass;
  VkDescriptorSetLayout descriptorSetLayout;
  VkPipelineLayout pipelineLayout;
  VkPipeline graphicsPipeline;
  std::vector<VkFramebuffer> swapChainFramebuffers;
  VkImage textureImage;
  VkDeviceMemory textureImageMemory;
  VkCommandPool commandPool;
  std::vector<VkCommandBuffer> commandBuffers;
  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  VkBuffer vertexBuffer;
  VkDeviceMemory vertexBufferMemory;
  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;
  std::vector<VkBuffer> uniformBuffers;
  std::vector<VkDeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;
  VkDescriptorPool descriptorPool;
  std::vector<VkDescriptorSet> descriptorSets;
  VkImageView textureImageView;
  VkSampler textureSampler;
};

VulkanConfig vulkanConfig = {};
bool running = true;
bool focused = false;
bool isBackendReady = false;
bool framebufferResized = false;
uint32_t currentFrame = 0;

GLFWwindow *glfwWindow = nullptr;
android_app *androidApp = nullptr;

#ifndef __ANDROID__
void framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
  framebufferResized = true;
}
#endif

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
    // TODO: Check if VK_EXT_debug_utils is available before setting the pNext
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

bool checkDeviceExtensionsSupport(VkPhysicalDevice device)
{
  uint32_t extensionsCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, nullptr);

  std::vector<VkExtensionProperties> extensionProperties(extensionsCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, extensionProperties.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

  for(auto extension : extensionProperties)
  {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, vulkanConfig.surface, &details.capabilities);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, vulkanConfig.surface, &formatCount, nullptr);

  if(formatCount != 0)
  {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, vulkanConfig.surface, &formatCount, details.formats.data());
  }

  uint32_t presentCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, vulkanConfig.surface, &presentCount, nullptr);

  if(presentCount != 0)
  {
    details.presentModes.resize(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, vulkanConfig.surface, &presentCount, details.presentModes.data());
  }


  if(details.capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
  {
    details.capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }

  return details;
}

bool isDeviceSuitable(VkPhysicalDevice device)
{
  QueueFamilyIndices indices = findQueueFamilies(device);
  bool extensionsSupported = checkDeviceExtensionsSupport(device);

  bool swapChainAdequate = false;
  if(extensionsSupported)
  {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
  }

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

  return indices.graphicsFamily.has_value() && indices.presentFamily.has_value() && swapChainAdequate && supportedFeatures.samplerAnisotropy;
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
  deviceFeatures.samplerAnisotropy = VK_TRUE;

  if(vkCreateDevice(vulkanConfig.physicalDevice, &deviceCreateInfo, nullptr, &vulkanConfig.device) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create logical device");
  }

  vkGetDeviceQueue(vulkanConfig.device, indices.graphicsFamily.value(), 0, &vulkanConfig.graphicsQueue);
  vkGetDeviceQueue(vulkanConfig.device, indices.presentFamily.value(), 0, &vulkanConfig.presentQueue);
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
  for (const auto& availableFormat : availableFormats)
  {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
  for (const auto& availablePresentMode : availablePresentModes)
  {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }
  else
  {
    int width, height;
    #ifdef __ANDROID__
    width = static_cast<int>(ANativeWindow_getWidth(androidApp->window));
    height = static_cast<int>(ANativeWindow_getHeight(androidApp->window));
    #else
    glfwGetFramebufferSize(glfwWindow, &width, &height);
    #endif

    VkExtent2D actualExtent = {
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

void createSwapChain()
{
  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vulkanConfig.physicalDevice);

  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

  // Make sure not to exceed number of images while adding +1 to minImageCount
  if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
  {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
  swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCreateInfo.pNext = nullptr;
  // swapchainCreateInfo.flags;
  swapchainCreateInfo.surface = vulkanConfig.surface;
  swapchainCreateInfo.minImageCount = imageCount; // The implementation will either create the swapchain with at least that many images, or it will fail to create the swapchain.
  swapchainCreateInfo.imageFormat = surfaceFormat.format;
  swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapchainCreateInfo.imageExtent = extent;
  swapchainCreateInfo.imageArrayLayers = 1; // For non-stereoscopic-3D applications, this value is 1.
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // specifies that the image can be used to create a VkImageView suitable for use as a color or resolve attachment in a VkFramebuffer.

  QueueFamilyIndices indices = findQueueFamilies(vulkanConfig.physicalDevice);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  // If the families are different
  if (indices.graphicsFamily != indices.presentFamily)
  {
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchainCreateInfo.queueFamilyIndexCount = 2; // Number of queue families accessing the the images of the swapchain
    swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices; // pointer to the array of families
  }
  else
  {
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0; // Optional
    swapchainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
  }

  swapchainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;

  auto compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(swapChainSupport.capabilities.supportedCompositeAlpha);

  swapchainCreateInfo.compositeAlpha = compositeAlpha;
  swapchainCreateInfo.presentMode = presentMode;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

  if(vkCreateSwapchainKHR(vulkanConfig.device, &swapchainCreateInfo, nullptr, &vulkanConfig.swapChain) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create swapchain");
  }

  vkGetSwapchainImagesKHR(vulkanConfig.device, vulkanConfig.swapChain, &imageCount, nullptr);
  vulkanConfig.swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(vulkanConfig.device, vulkanConfig.swapChain, &imageCount, vulkanConfig.swapChainImages.data());

  vulkanConfig.swapChainImageFormat = surfaceFormat.format;
  vulkanConfig.swapChainExtent = extent;
}

VkImageView createImageView(VkImage image, VkFormat format) {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView;
  if (vkCreateImageView(vulkanConfig.device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
    LOG_DEBUG("failed to create image view!");
  }

  return imageView;
}

void createImageViews()
{
  vulkanConfig.swapChainImageViews.resize(vulkanConfig.swapChainImages.size());
  for(size_t i = 0; i < vulkanConfig.swapChainImages.size(); i++)
  {
    vulkanConfig.swapChainImageViews[i] = createImageView(vulkanConfig.swapChainImages[i], vulkanConfig.swapChainImageFormat);
  }
}

std::vector<char> readFile(const std::string &filename)
{
  #ifdef __ANDROID__
  AAssetManager *am = androidApp->activity->assetManager;
  AAsset *asset = AAssetManager_open(am, filename.c_str(), AASSET_MODE_STREAMING);

  if(!asset)
  {
    LOG_DEBUG("Could not open file {}", filename);
  }

  size_t size = static_cast<size_t>(AAsset_getLength(asset));
  std::vector<char> buffer(size);

  AAsset_read(asset, buffer.data(), size);
  AAsset_close(asset);

  #else
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if(!file.is_open())
  {
    LOG_DEBUG("Could not open file {}", filename);
  }

  size_t size = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(size);

  file.seekg(0);
  file.read(buffer.data(), size);

  file.close();
  #endif

  return buffer;
}

VkShaderModule createShaderModule(const std::vector<char> &code)
{
  VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleCreateInfo.pNext = nullptr;
  // shaderModuleCreateInfo.flags;
  shaderModuleCreateInfo.codeSize = code.size();
  shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shaderModule;
  if(vkCreateShaderModule(vulkanConfig.device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create shader module");
  }

  return shaderModule;
}

void createGraphicsPipeline()
{
  std::vector<char> vertShaderCode = readFile("shaders/vert.spv");
  std::vector<char> fragShaderCode = readFile("shaders/frag.spv");

  VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

  VkPipelineShaderStageCreateInfo vertStageCreateInfo = {};
  vertStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStageCreateInfo.pNext = nullptr;
  // vertStageCreateInfo.flags;
  vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStageCreateInfo.module = vertShaderModule;
  vertStageCreateInfo.pName = "main";
  // vertStageCreateInfo.pSpecializationInfo;

  VkPipelineShaderStageCreateInfo fragStageCreateInfo = {};
  fragStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragStageCreateInfo.pNext = nullptr;
  fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStageCreateInfo.module = fragShaderModule;
  fragStageCreateInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageCreateInfo, fragStageCreateInfo};

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &vulkanConfig.descriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  if(vkCreatePipelineLayout(vulkanConfig.device, &pipelineLayoutInfo, nullptr, &vulkanConfig.pipelineLayout) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create pipelineLayout");
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;

  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = nullptr; // Optional
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = vulkanConfig.pipelineLayout;
  pipelineInfo.renderPass = vulkanConfig.renderPass;
  pipelineInfo.subpass = 0;

  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
  pipelineInfo.basePipelineIndex = -1; // Optional

  if (vkCreateGraphicsPipelines(vulkanConfig.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vulkanConfig.graphicsPipeline) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create graphics pipeline");
  }

  vkDestroyShaderModule(vulkanConfig.device, fragShaderModule, nullptr);
  vkDestroyShaderModule(vulkanConfig.device, vertShaderModule, nullptr);
}

void createRenderPass()
{
  VkAttachmentDescription colorAttachment{};
  // colorAttachment.flags;
  colorAttachment.format = vulkanConfig.swapChainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Images to be presented in the swap chain

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;

  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if(vkCreateRenderPass(vulkanConfig.device, &renderPassInfo, nullptr, &vulkanConfig.renderPass) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create render pass");
  }
}

void createFramebuffers()
{
  vulkanConfig.swapChainFramebuffers.resize(vulkanConfig.swapChainImageViews.size());

  for (size_t i = 0; i < vulkanConfig.swapChainImageViews.size(); i++)
  {
    VkImageView attachments[] = {
      vulkanConfig.swapChainImageViews[i]
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = vulkanConfig.renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = vulkanConfig.swapChainExtent.width;
    framebufferInfo.height = vulkanConfig.swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(vulkanConfig.device, &framebufferInfo, nullptr, &vulkanConfig.swapChainFramebuffers[i]) != VK_SUCCESS)
    {
      LOG_DEBUG("Failed to create framebuffer");
    }
  }
}

void createCommandPool()
{
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(vulkanConfig.physicalDevice);

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

  if (vkCreateCommandPool(vulkanConfig.device, &poolInfo, nullptr, &vulkanConfig.commandPool) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create command pool");
  }
}

void createCommandBuffer()
{
  vulkanConfig.commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = vulkanConfig.commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = static_cast<uint32_t>(vulkanConfig.commandBuffers.size());

  if (vkAllocateCommandBuffers(vulkanConfig.device, &allocInfo, vulkanConfig.commandBuffers.data()) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to create command buffer");
  }
}

void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0; // Optional
  beginInfo.pInheritanceInfo = nullptr; // Optional

  if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to begin recording the command buffer");
  }

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = vulkanConfig.renderPass;
  renderPassInfo.framebuffer = vulkanConfig.swapChainFramebuffers[imageIndex];

  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = vulkanConfig.swapChainExtent;

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanConfig.graphicsPipeline);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(vulkanConfig.swapChainExtent.width);
  viewport.height = static_cast<float>(vulkanConfig.swapChainExtent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = vulkanConfig.swapChainExtent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  VkBuffer vertexBuffers[] = {vulkanConfig.vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer, vulkanConfig.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanConfig.pipelineLayout, 0, 1, &vulkanConfig.descriptorSets[currentFrame], 0, nullptr);
  vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
  {
    LOG_DEBUG("Failed to end recording the command buffer");
  }
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(vulkanConfig.physicalDevice, &memProperties);

  uint32_t memoryType = std::numeric_limits<uint32_t>::max();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      memoryType = i;
    }
  }

  if(memoryType == std::numeric_limits<uint32_t>::max())
  {
    LOG_DEBUG("Failed to find suitable memory type");
  }

  return memoryType;
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(vulkanConfig.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
    LOG_DEBUG("failed to create buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(vulkanConfig.device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(vulkanConfig.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
    LOG_DEBUG("failed to allocate buffer memory!");
  }

  vkBindBufferMemory(vulkanConfig.device, buffer, bufferMemory, 0);
}

VkCommandBuffer beginSingleTimeCommands() {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = vulkanConfig.commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(vulkanConfig.device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(vulkanConfig.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(vulkanConfig.graphicsQueue);

  vkFreeCommandBuffers(vulkanConfig.device, vulkanConfig.commandPool, 1, &commandBuffer);
}

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;

  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    LOG_DEBUG("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(
    commandBuffer,
    sourceStage, destinationStage,
    0,
    0, nullptr,
    0, nullptr,
    1, &barrier
  );

  endSingleTimeCommands(commandBuffer);
}

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
    width,
    height,
    1
  };

  vkCmdCopyBufferToImage(
    commandBuffer,
    buffer,
    image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region
  );

  endSingleTimeCommands(commandBuffer);
}

void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(vulkanConfig.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
    LOG_DEBUG("failed to create image!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(vulkanConfig.device, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(vulkanConfig.device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
    LOG_DEBUG("failed to allocate image memory!");
  }

  vkBindImageMemory(vulkanConfig.device, image, imageMemory, 0);
}

void createTextureImage()
{
  const char *filename = "textures/texture.jpg";
  int texWidth, texHeight, texChannels;
  unsigned char *pixels = nullptr;
  
  #ifdef __ANDROID__
  AAssetManager *am = androidApp->activity->assetManager;
  AAsset *image = AAssetManager_open(am, filename, AASSET_MODE_BUFFER);
  AImageDecoder *androidDecoder = nullptr;

  int result = AImageDecoder_createFromAAsset(image, &androidDecoder);

  if(result != ANDROID_IMAGE_DECODER_SUCCESS)
  {
    LOG_DEBUG("Failed to create image decoder");
  }

  AImageDecoder_setAndroidBitmapFormat(androidDecoder, ANDROID_BITMAP_FORMAT_RGBA_8888);

  const AImageDecoderHeaderInfo *imageHeader = AImageDecoder_getHeaderInfo(androidDecoder);

  texWidth = static_cast<int>(AImageDecoderHeaderInfo_getWidth(imageHeader));
  texHeight = static_cast<int>(AImageDecoderHeaderInfo_getHeight(imageHeader));
  size_t stride = AImageDecoder_getMinimumStride(androidDecoder);

  size_t bufferSize = texHeight * stride;
  pixels = new unsigned char[bufferSize];

  auto decodeResult = AImageDecoder_decodeImage(androidDecoder, pixels, stride, bufferSize);
  if(decodeResult != ANDROID_IMAGE_DECODER_SUCCESS)
  {
    LOG_DEBUG("Failed to decode image");
  }

  #else
  pixels = stbi_load(filename, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

  if (!pixels) {
    LOG_DEBUG("Failed to load texture image");
  }
  #endif

  VkDeviceSize imageSize = texWidth * texHeight * 4;

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(vulkanConfig.device, stagingBufferMemory, 0, imageSize, 0, &data);
      memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(vulkanConfig.device, stagingBufferMemory);

  #ifdef __ANDROID__
  delete[] pixels;
  #else
  stbi_image_free(pixels);
  #endif

  createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanConfig.textureImage, vulkanConfig.textureImageMemory);

  transitionImageLayout(vulkanConfig.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(stagingBuffer, vulkanConfig.textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
  transitionImageLayout(vulkanConfig.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(vulkanConfig.device, stagingBuffer, nullptr);
  vkFreeMemory(vulkanConfig.device, stagingBufferMemory, nullptr);
}

void createTextureImageView()
{
  vulkanConfig.textureImageView = createImageView(vulkanConfig.textureImage, VK_FORMAT_R8G8B8A8_SRGB);
}

void createTextureSampler()
{
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;

  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(vulkanConfig.physicalDevice, &properties);

  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;

  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;

  if (vkCreateSampler(vulkanConfig.device, &samplerInfo, nullptr, &vulkanConfig.textureSampler) != VK_SUCCESS) {
    LOG_DEBUG("failed to create texture sampler!");
  }
}

void createSyncObjects()
{
  vulkanConfig.imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  vulkanConfig.renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  vulkanConfig.inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    if(
      vkCreateSemaphore(vulkanConfig.device, &semaphoreInfo, nullptr, &vulkanConfig.imageAvailableSemaphores[i]) != VK_SUCCESS ||
      vkCreateSemaphore(vulkanConfig.device, &semaphoreInfo, nullptr, &vulkanConfig.renderFinishedSemaphores[i]) != VK_SUCCESS ||
      vkCreateFence(vulkanConfig.device, &fenceInfo, nullptr, &vulkanConfig.inFlightFences[i]) != VK_SUCCESS
    )
    {
      LOG_DEBUG("Failed to create sync objects");
    }
  }
}

void cleanUpSwapChain()
{
  for (auto framebuffer : vulkanConfig.swapChainFramebuffers)
  {
    vkDestroyFramebuffer(vulkanConfig.device, framebuffer, nullptr);
  }

  for (auto imageView : vulkanConfig.swapChainImageViews)
  {
    vkDestroyImageView(vulkanConfig.device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(vulkanConfig.device, vulkanConfig.swapChain, nullptr);
}

void recreateSwapChain()
{
  #ifndef __ANDROID__
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(glfwWindow, &width, &height);
  while(width == 0 || height == 0)
  {
    glfwGetFramebufferSize(glfwWindow, &width, &height);
    glfwWaitEvents();
  }
  #endif

  vkDeviceWaitIdle(vulkanConfig.device);
  cleanUpSwapChain();

  createSwapChain();
  createImageViews();
  createFramebuffers();
}

void updateUniformBuffer(uint32_t currentFrame)
{
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

  UniformBufferObject ubo{};
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(glm::radians(45.0f), vulkanConfig.swapChainExtent.width / (float) vulkanConfig.swapChainExtent.height, 0.1f, 10.0f);
  ubo.proj[1][1] *= -1;

  memcpy(vulkanConfig.uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void drawFrame()
{
  vkWaitForFences(vulkanConfig.device, 1, &vulkanConfig.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(vulkanConfig.device, vulkanConfig.swapChain, UINT64_MAX, vulkanConfig.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapChain();
      return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOG_DEBUG("failed to acquire swap chain image!");
  }

  vkResetFences(vulkanConfig.device, 1, &vulkanConfig.inFlightFences[currentFrame]);

  vkResetCommandBuffer(vulkanConfig.commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
  recordCommandBuffer(vulkanConfig.commandBuffers[currentFrame], imageIndex);

  updateUniformBuffer(currentFrame);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {vulkanConfig.imageAvailableSemaphores[currentFrame]};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &vulkanConfig.commandBuffers[currentFrame];

  VkSemaphore signalSemaphores[] = {vulkanConfig.renderFinishedSemaphores[currentFrame]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(vulkanConfig.graphicsQueue, 1, &submitInfo, vulkanConfig.inFlightFences[currentFrame]) != VK_SUCCESS) {
    LOG_DEBUG("failed to submit draw command buffer!");
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {vulkanConfig.swapChain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(vulkanConfig.presentQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
      framebufferResized = false;
      recreateSwapChain();
  } else if (result != VK_SUCCESS) {
    LOG_DEBUG("failed to present swap chain image!");
  }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}


void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  endSingleTimeCommands(commandBuffer);
}


void createVertexBuffer()
{
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(vulkanConfig.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t) bufferSize);
  vkUnmapMemory(vulkanConfig.device, stagingBufferMemory);

  createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanConfig.vertexBuffer, vulkanConfig.vertexBufferMemory);

  copyBuffer(stagingBuffer, vulkanConfig.vertexBuffer, bufferSize);

  vkDestroyBuffer(vulkanConfig.device, stagingBuffer, nullptr);
  vkFreeMemory(vulkanConfig.device, stagingBufferMemory, nullptr);
}

void createIndexBuffer()
{
  VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(vulkanConfig.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t) bufferSize);
  vkUnmapMemory(vulkanConfig.device, stagingBufferMemory);

  createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanConfig.indexBuffer, vulkanConfig.indexBufferMemory);

  copyBuffer(stagingBuffer, vulkanConfig.indexBuffer, bufferSize);

  vkDestroyBuffer(vulkanConfig.device, stagingBuffer, nullptr);
  vkFreeMemory(vulkanConfig.device, stagingBufferMemory, nullptr);
}

void createDescriptorSetLayout()
{
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 1;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = nullptr;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(vulkanConfig.device, &layoutInfo, nullptr, &vulkanConfig.descriptorSetLayout) != VK_SUCCESS)
  {
    LOG_DEBUG("failed to create descriptor set layout!");
  }
}

void createUniformBuffers()
{
  VkDeviceSize bufferSize = sizeof(UniformBufferObject);

  vulkanConfig.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  vulkanConfig.uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
  vulkanConfig.uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vulkanConfig.uniformBuffers[i], vulkanConfig.uniformBuffersMemory[i]);

    vkMapMemory(vulkanConfig.device, vulkanConfig.uniformBuffersMemory[i], 0, bufferSize, 0, &vulkanConfig.uniformBuffersMapped[i]);
  }
}

void createDescriptorPool()
{
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  if (vkCreateDescriptorPool(vulkanConfig.device, &poolInfo, nullptr, &vulkanConfig.descriptorPool) != VK_SUCCESS) {
    LOG_DEBUG("failed to create descriptor pool!");
  }
}

void createDescriptorSets()
{
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, vulkanConfig.descriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = vulkanConfig.descriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  vulkanConfig.descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

  if (vkAllocateDescriptorSets(vulkanConfig.device, &allocInfo, vulkanConfig.descriptorSets.data()) != VK_SUCCESS) {
    LOG_DEBUG("failed to allocate descriptor sets!");
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = vulkanConfig.uniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = vulkanConfig.textureImageView;
    imageInfo.sampler = vulkanConfig.textureSampler;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = vulkanConfig.descriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = vulkanConfig.descriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(vulkanConfig.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
  }
}

void cleanUp()
{
  LOG_DEBUG("Cleaning up");

  cleanUpSwapChain();

  vkDestroySampler(vulkanConfig.device, vulkanConfig.textureSampler, nullptr);
  vkDestroyImageView(vulkanConfig.device, vulkanConfig.textureImageView, nullptr);

  vkDestroyImage(vulkanConfig.device, vulkanConfig.textureImage, nullptr);
  vkFreeMemory(vulkanConfig.device, vulkanConfig.textureImageMemory, nullptr);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroyBuffer(vulkanConfig.device, vulkanConfig.uniformBuffers[i], nullptr);
    vkFreeMemory(vulkanConfig.device, vulkanConfig.uniformBuffersMemory[i], nullptr);
  }

  vkDestroyDescriptorPool(vulkanConfig.device, vulkanConfig.descriptorPool, nullptr);

  vkDestroyDescriptorSetLayout(vulkanConfig.device, vulkanConfig.descriptorSetLayout, nullptr);

  vkDestroyBuffer(vulkanConfig.device, vulkanConfig.indexBuffer, nullptr);
  vkFreeMemory(vulkanConfig.device, vulkanConfig.indexBufferMemory, nullptr);

  vkDestroyBuffer(vulkanConfig.device, vulkanConfig.vertexBuffer, nullptr);
  vkFreeMemory(vulkanConfig.device, vulkanConfig.vertexBufferMemory, nullptr);

  for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkDestroySemaphore(vulkanConfig.device, vulkanConfig.imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(vulkanConfig.device, vulkanConfig.renderFinishedSemaphores[i], nullptr);
    vkDestroyFence(vulkanConfig.device, vulkanConfig.inFlightFences[i], nullptr);
  }

  vkDestroyCommandPool(vulkanConfig.device, vulkanConfig.commandPool, nullptr);

  vkDestroyRenderPass(vulkanConfig.device, vulkanConfig.renderPass, nullptr);

  vkDestroyPipeline(vulkanConfig.device, vulkanConfig.graphicsPipeline, nullptr);
  vkDestroyPipelineLayout(vulkanConfig.device, vulkanConfig.pipelineLayout, nullptr);

  if (enableValidationLayers)
  {
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
  createSwapChain();
  createImageViews();
  createRenderPass();
  createDescriptorSetLayout();
  createGraphicsPipeline();
  createFramebuffers();
  createCommandPool();
  createTextureImage();
  createTextureImageView();
  createTextureSampler();
  createVertexBuffer();
  createIndexBuffer();
  createUniformBuffers();
  createDescriptorPool();
  createDescriptorSets();
  createCommandBuffer();
  createSyncObjects();

  isBackendReady = true;
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
  focused = true;
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
  glfwSetFramebufferSizeCallback(glfwWindow, framebufferResizeCallback);
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

    if(focused && isBackendReady)
    {
      drawFrame();
    }
  }

  vkDeviceWaitIdle(vulkanConfig.device);

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