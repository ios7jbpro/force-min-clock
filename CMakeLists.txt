cmake_minimum_required(VERSION 3.15)
project(force-min-clock LANGUAGES C)

set(CMAKE_C_STANDARD 11)

add_executable(force-min-clock WIN32
    src/main.c
    src/resource.rc
)

target_compile_definitions(force-min-clock PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_options(force-min-clock PRIVATE -municode -mwindows)

target_link_libraries(force-min-clock PRIVATE
    shell32
    comctl32
    advapi32
)
