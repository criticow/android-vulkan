#!/bin/bash

$VULKAN_SDK/bin/glslc.exe data/shaders/shader.vert -o data/shaders/vert.spv
$VULKAN_SDK/bin/glslc.exe data/shaders/shader.frag -o data/shaders/frag.spv