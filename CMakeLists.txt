cmake_minimum_required(VERSION 3.14)

project(VisualOptimization VERSION 1.0.0 LANGUAGES CXX)

# BML+ SDK：提供 BML 目标 (BMLPlus.lib)
find_package(BML CONFIG REQUIRED)
# Virtools SDK：提供 CK2 / VxMath 目标与 CKAll.h 头文件
find_package(VirtoolsSDK CONFIG REQUIRED)

function(add_visual_mod MODNAME)
  add_library(${MODNAME} SHARED main.cpp)
  set_target_properties(${MODNAME} PROPERTIES PREFIX ""
    OUTPUT_NAME "${MODNAME}"
    SUFFIX ".bmodp"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_options(${MODNAME} PRIVATE /utf-8)
  endif()
  target_compile_definitions(${MODNAME} PRIVATE USING_BML_PLUS _CRT_SECURE_NO_WARNINGS ${ARGN})
  target_link_libraries(${MODNAME} PRIVATE BML CK2 VxMath)
endfunction()

# 写死版：视距 1e9、preload 开、光照优化开
add_visual_mod(VisualOptimization)

# 可配置版：三个距离输入栏 + preload/光照优化开关
add_visual_mod(VisualOptimizationConfigurable CONFIG_MODE)
