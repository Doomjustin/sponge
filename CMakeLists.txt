cmake_minimum_required(VERSION 3.28)

project(sponge
    VERSION 0.0.1
    LANGUAGES C CXX
)

include(CTest)
include(CMakeDependentOption)

cmake_dependent_option(${PROJECT_NAME}_BUILD_TESTS
    "Enable ${PROJECT_NAME} project tests targets" ON
    "BUILD_TESTING" OFF
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(ConfigSafeGuards)
include(AddModule)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)

if (${PROJECT_NAME}_BUILD_TESTS)
   find_package(Catch2 CONFIG REQUIRED)
endif()

add_subdirectory(src)
