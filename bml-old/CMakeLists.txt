cmake_minimum_required(VERSION 3.14)

# ============================================================
# VisualOptimization - 原版 BML (.bmod) 构建配置
# 使用 BML v0.3.40 SDK（最早兼容版本）编译，声明 BML 0.3.40，
# 兼容 BML v0.3.40 ~ v0.3.43（接口经验证完全一致）。
# 产物: bml-old/build/Release/VisualOptimization.bmod
#       bml-old/build/Release/VisualOptimizationConfigurable.bmod
# ============================================================

project(VisualOptimizationBML VERSION 1.0.0 LANGUAGES CXX)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../SDK/BML-dev-0340")
    set(BML_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../SDK/BML-dev-0340")
else()
    set(BML_SDK "E:/ViewDistanceEditor/SDK/BML-dev-0340")
endif()

function(add_visual_bml MODNAME)
    add_library(${MODNAME} SHARED ../main.cpp)
    set_target_properties(${MODNAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${MODNAME}"
        SUFFIX ".bmod"
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON)
    target_include_directories(${MODNAME} PRIVATE "${BML_SDK}/include")
    target_link_libraries(${MODNAME} PRIVATE "${BML_SDK}/lib/Release/BML.lib")
    if(MSVC)
        target_compile_options(${MODNAME} PRIVATE /utf-8)
    endif()
    target_compile_definitions(${MODNAME} PRIVATE _CRT_SECURE_NO_WARNINGS BML_ORIENTATION_REF ${ARGN})
endfunction()

add_visual_bml(VisualOptimization)
add_visual_bml(VisualOptimizationConfigurable CONFIG_MODE)
