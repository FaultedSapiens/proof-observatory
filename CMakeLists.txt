cmake_minimum_required(VERSION 3.20)

project(proof-observatory
    VERSION 0.2.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(yaml-cpp CONFIG REQUIRED)

add_executable(proof-observatory
    src/main.cpp
    src/artifact_stage.cpp
    src/inspector.cpp
    src/process.cpp
    src/formalization_parser.cpp
    src/lean_indexer.cpp
    src/snapshot.cpp
    src/verification.cpp
    src/simulation.cpp
)

target_include_directories(proof-observatory
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(proof-observatory
    PRIVATE
        yaml-cpp::yaml-cpp
)

enable_testing()

add_executable(formalization_parser_test
    tests/formalization_parser_test.cpp
    src/formalization_parser.cpp
)

target_include_directories(formalization_parser_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(formalization_parser_test
    PRIVATE
        yaml-cpp::yaml-cpp
)

add_test(NAME formalization_parser_test COMMAND formalization_parser_test)

add_executable(process_runner_test
    tests/process_runner_test.cpp
    src/process.cpp
)

# This target runs alongside the application in the solution. Give its MSVC
# compiler PDB a private path so stale/contended target PDBs cannot break builds.
set_target_properties(process_runner_test PROPERTIES
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/process-runner-pdb"
)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/process-runner-pdb")
target_compile_options(process_runner_test PRIVATE
    "$<$<CONFIG:Debug>:/Fd${CMAKE_CURRENT_BINARY_DIR}/process-runner-pdb/process_runner_test.pdb>"
)

target_include_directories(process_runner_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME process_runner_test COMMAND process_runner_test)

add_executable(simulation_test tests/simulation_test.cpp src/simulation.cpp)
target_include_directories(simulation_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME simulation_test COMMAND simulation_test)

add_executable(lean_indexer_test
    tests/lean_indexer_test.cpp
    src/lean_indexer.cpp
    src/artifact_stage.cpp
    src/process.cpp
)
target_include_directories(lean_indexer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME lean_indexer_test COMMAND lean_indexer_test)
