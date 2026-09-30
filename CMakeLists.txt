cmake_minimum_required(VERSION 3.21)
project(dimmest LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# static CRT: one exe, nothing else to install
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(dimmest WIN32
    src/main.cpp
    src/TrayApp.cpp
    src/Magnifier.cpp
    src/ColorEffect.cpp
    src/Settings.cpp
    res/dimmest.rc
    res/dimmest.manifest)

target_include_directories(dimmest PRIVATE src)
target_compile_definitions(dimmest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(dimmest PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8>)
target_link_libraries(dimmest PRIVATE magnification comctl32 comdlg32 wtsapi32 shell32 ole32 advapi32)
