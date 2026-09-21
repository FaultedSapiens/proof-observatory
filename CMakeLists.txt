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
    src/inspector.cpp
    src/process.cpp
    src/formalization_parser.cpp
    src/lean_indexer.cpp
    src/snapshot.cpp
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