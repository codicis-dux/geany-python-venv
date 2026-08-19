cmake_minimum_required(VERSION 3.16)

project(GeanyPlugin
    VERSION 0.1
    LANGUAGES C
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GEANY REQUIRED geany)

add_library(geany_plugin MODULE
    src/plugin.c
)

target_include_directories(geany_plugin PRIVATE
    ${GEANY_INCLUDE_DIRS}
)

target_link_directories(geany_plugin PRIVATE
    ${GEANY_LIBRARY_DIRS}
)

target_link_libraries(geany_plugin PRIVATE
    ${GEANY_LIBRARIES}
)

target_compile_options(geany_plugin PRIVATE
    ${GEANY_CFLAGS_OTHER}
)

set_target_properties(geany_plugin PROPERTIES
    PREFIX ""
)
