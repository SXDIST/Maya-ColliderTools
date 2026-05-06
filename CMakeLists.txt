cmake_minimum_required(VERSION 3.13)

project(ColliderTools VERSION 0.1.0)

if(DEFINED ENV{DEVKIT_LOCATION})
    set(MAYA_DEVKIT_LOCATION $ENV{DEVKIT_LOCATION})
else()
    set(MAYA_DEVKIT_LOCATION "C:/maya_devkits/2027/devkitBase")
endif()

include(${MAYA_DEVKIT_LOCATION}/cmake/pluginEntry.cmake)

set(PROJECT_NAME ColliderTools)

set(SOURCE_FILES
    src/ColliderCreateCmd.cpp
    src/ColliderGeometry.cpp
    src/ColliderFit.cpp
    src/ColliderPrimitiveManip.cpp
    src/ColliderPrimitiveNode.cpp
    src/ColliderToolsPlugin.cpp
    src/ColliderHullNode.cpp
    src/ColliderHull.cpp
    src/ColliderHullCore.cpp
)

set(LIBRARIES
    OpenMaya
    OpenMayaUI
    Foundation
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

build_plugin()
