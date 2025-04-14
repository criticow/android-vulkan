cmake_minimum_required(VERSION 4.0.0)

project(android-vulkan)

set(CMAKE_CXX_STANDARD 20)

# GLFW Variables
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Don't build GLFW Examples")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Don't build GLFW Tests")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Don't build GLFW Docs")
set(GLFW_INSTALL OFF CACHE BOOL "Don't install GLFW")

set(SOURCES src/main.cpp)
set(LINK_LIBS Vulkan::Vulkan)
set(INCLUDE_DIRS deps/glm)

find_package(Vulkan REQUIRED)

if(ANDROID)
  list(APPEND LINK_LIBS android log)
  list(APPEND SOURCES $ENV{NDK_HOME}/sources/android/native_app_glue/android_native_app_glue.c)
  list(APPEND INCLUDE_DIRS $ENV{NDK_HOME}/sources/android)

  add_library(${PROJECT_NAME} SHARED ${SOURCES})
else()
  list(APPEND LINK_LIBS glfw)

  add_subdirectory(deps/glfw)
  add_executable(${PROJECT_NAME} ${SOURCES})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${LINK_LIBS})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRS})