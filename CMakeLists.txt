cmake_minimum_required(VERSION 3.16)

project(Python_venv
    VERSION 0.5
    LANGUAGES C
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GEANY REQUIRED geany)

add_library(python_venv MODULE
    src/plugin.c
)

target_include_directories(python_venv PRIVATE
    ${GEANY_INCLUDE_DIRS}
)

target_link_directories(python_venv PRIVATE
    ${GEANY_LIBRARY_DIRS}
)

target_link_libraries(python_venv PRIVATE
    ${GEANY_LIBRARIES}
)

target_compile_options(python_venv PRIVATE
    ${GEANY_CFLAGS_OTHER}
)

set_target_properties(python_venv PROPERTIES
    PREFIX ""
)
