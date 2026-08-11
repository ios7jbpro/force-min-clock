cmake_minimum_required(VERSION 3.15)
project(force-min-clock LANGUAGES C)

set(CMAKE_C_STANDARD 11)

add_executable(force-min-clock WIN32
    src/main.c
    src/resource.rc
)

target_compile_definitions(force-min-clock PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

target_link_libraries(force-min-clock PRIVATE
    shell32
    comctl32
    advapi32
)

# NVAPI - expects NVAPI_DIR environment variable or -DNVAPI_DIR=path
if(DEFINED ENV{NVAPI_DIR})
    set(NVAPI_DIR "$ENV{NVAPI_DIR}")
endif()

if(NVAPI_DIR)
    message(STATUS "NVAPI found at: ${NVAPI_DIR}")
    target_include_directories(force-min-clock PRIVATE "${NVAPI_DIR}/include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_directories(force-min-clock PRIVATE "${NVAPI_DIR}/amd64")
        target_link_libraries(force-min-clock PRIVATE nvapi64)
    else()
        target_link_directories(force-min-clock PRIVATE "${NVAPI_DIR}/x86")
        target_link_libraries(force-min-clock PRIVATE nvapi)
    endif()
    target_compile_definitions(force-min-clock PRIVATE HAS_NVAPI)
else()
    message(WARNING "NVAPI_DIR not set. NVIDIA clock control will not be available. "
        "Download NVAPI SDK and set NVAPI_DIR.")
endif()
