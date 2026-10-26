cmake_minimum_required(VERSION 3.21)

project(BossSense VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------- #
#                     CommonLibSSE-NG (ng)                          #
# ---------------------------------------------------------------- #
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK OFF CACHE BOOL "" FORCE)

if(DEFINED ENV{COMMONLIB_SSE_FOLDER})
    set(COMMONLIB_SSE_FOLDER "$ENV{COMMONLIB_SSE_FOLDER}")
elseif(EXISTS "E:/CommonLibSSE-NG-alandtse/CMakeLists.txt")
    # The shared alandtse/CommonLibSSE-NG (ng) checkout used by this machine.
    set(COMMONLIB_SSE_FOLDER "E:/CommonLibSSE-NG-alandtse")
else()
    set(COMMONLIB_SSE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng")
endif()

if(NOT EXISTS "${COMMONLIB_SSE_FOLDER}/CMakeLists.txt")
    message(FATAL_ERROR "CommonLibSSE-NG not found at '${COMMONLIB_SSE_FOLDER}'")
endif()

add_subdirectory("${COMMONLIB_SSE_FOLDER}" "${CMAKE_CURRENT_BINARY_DIR}/commonlibsse")
include("${COMMONLIB_SSE_FOLDER}/cmake/CommonLibSSE.cmake")

# ---------------------------------------------------------------- #
#                             Sources                              #
# ---------------------------------------------------------------- #
set(SOURCES
    src/main.cpp
    src/Config.cpp
    src/PluginScanner.cpp
    src/Detector.cpp
    src/TrueHUD.cpp
    src/BossManager.cpp
    src/Localization.cpp
    src/UI.cpp
)

add_library(bosssense_miniz STATIC
    vendor/miniz/miniz.c
    vendor/miniz/miniz_tdef.c
    vendor/miniz/miniz_tinfl.c
)
target_include_directories(bosssense_miniz PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniz")

add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "Boss Sense"
    SOURCES ${SOURCES}
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_link_libraries(${PROJECT_NAME} PRIVATE bosssense_miniz)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniz"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /utf-8
        /Zc:__cplusplus
        /Zc:preprocessor
        /permissive-
        /EHsc
        /MP
        /bigobj
        /W4
        /wd4100 /wd4189 /wd4244 /wd4245 /wd4267 /wd4324 /wd4458 /wd4459
        /Zi
    )
    # /DEBUG turns off /OPT:REF,/OPT:ICF; put the optimizations back.
    target_link_options(${PROJECT_NAME} PRIVATE /DEBUG /OPT:REF /OPT:ICF)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    OUTPUT_NAME "BossSense"
)
