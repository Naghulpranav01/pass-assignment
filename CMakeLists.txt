cmake_minimum_required(VERSION 3.16)
project(UnifiedEngineeringWorkbench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warning flags
if(MSVC)
    add_compile_options(/W4 /O2 /permissive-)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -O3 -march=x86-64)
    # Enable static runtime linking for zero external DLL dependency on Windows
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

# Include directories
include_directories(src/backend/include)

# Core library
set(CORE_SOURCES
    src/backend/src/stl_parser.cpp
    src/backend/src/ansys_csv_parser.cpp
    src/backend/src/fea_1d_solver.cpp
    src/backend/src/multicore_batch.cpp
    src/backend/src/embedded_adapters.cpp
    src/backend/src/backprop.cpp
)

add_library(workbench_core STATIC ${CORE_SOURCES})

# Winsock library on Windows
if(WIN32)
    target_link_libraries(workbench_core PUBLIC ws2_32)
endif()

# 1. Main Standalone Launcher (Launch.exe in root)
add_executable(Launch
    src/backend/src/launcher.cpp
    src/backend/src/http_server.cpp
)
target_link_libraries(Launch PRIVATE workbench_core)
set_target_properties(Launch PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# 2. Server Executable
add_executable(workbench_server
    src/backend/src/main_server.cpp
    src/backend/src/http_server.cpp
)
target_link_libraries(workbench_server PRIVATE workbench_core)
set_target_properties(workbench_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# 3. Unit & Numerical Tests Executable
enable_testing()
add_executable(run_tests
    tests/test_main.cpp
)
target_link_libraries(run_tests PRIVATE workbench_core)
set_target_properties(run_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_test(NAME AllUnitAndNumericalTests COMMAND run_tests)
