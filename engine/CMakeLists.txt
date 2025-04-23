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
set(INCLUDE_DIRS deps/glm deps/stb)

find_package(Vulkan REQUIRED)

if(ANDROID)
  list(APPEND LINK_LIBS android log jnigraphics)
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

if(ANDROID)
  add_custom_target(copy_data
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different 
    "${CMAKE_CURRENT_SOURCE_DIR}/data/"
    "${CMAKE_CURRENT_SOURCE_DIR}/../android_vulkan/app/src/main/assets"
    COMMENT "Copying data"
  )
else()
  add_custom_target(copy_data
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different 
    "${CMAKE_CURRENT_SOURCE_DIR}/data/"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying data"
  )
endif()

add_dependencies(${PROJECT_NAME} copy_data)