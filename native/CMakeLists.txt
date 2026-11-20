cmake_minimum_required(VERSION 3.16)
project(marila_native LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(marila-native main.cpp renderers.cpp scene_model.cpp)
target_compile_options(marila-native PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS marila-native RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_test(
    NAME marila_smoke
    COMMAND ${CMAKE_COMMAND}
        -DMARILA_EXECUTABLE=$<TARGET_FILE:marila-native>
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/smoke.cmake
)
