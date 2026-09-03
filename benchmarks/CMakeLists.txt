add_executable(benchmark_core benchmark_core.cpp)
target_link_libraries(benchmark_core PRIVATE RuntimeRegistry::runtimeregistry)
target_include_directories(benchmark_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/examples)
