cmake_minimum_required(VERSION 3.25)

project(RuntimeRegistry
        VERSION 1.0.0
        DESCRIPTION "Runtime Registry: canonical runtime/service/capability discovery and authority for heterogeneous AI infrastructure"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RUNTIMEREGISTRY_BUILD_TESTS "Build Runtime Registry tests" ON)
option(RUNTIMEREGISTRY_BUILD_EXAMPLES "Build Runtime Registry examples" ON)
option(RUNTIMEREGISTRY_BUILD_BENCHMARKS "Build Runtime Registry benchmarks" ON)
option(RUNTIMEREGISTRY_ENABLE_SYNTHETIC_DISTRIBUTED "Build synthetic distributed scenarios" ON)
option(RUNTIMEREGISTRY_ENABLE_CUDA_PROOF "Build the real CUDA capability proof (requires CUDA toolkit)" OFF)
option(RUNTIMEREGISTRY_BUILD_CLI "Build the runtime-registry command line utility" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /WX /permissive- /utf-8 /EHsc /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

set(RUNTIMEREGISTRY_SOURCES
  src/version.cpp
  src/enums.cpp
  src/provenance.cpp
  src/capability.cpp
  src/sha256.cpp
  src/registry.cpp
  src/index.cpp
  src/selection.cpp
  src/lease.cpp
  src/persistence.cpp
  src/protocol.cpp
  src/tcp.cpp
  src/authority.cpp
  src/explanation.cpp
  src/accounting.cpp
)

add_library(runtimeregistry STATIC ${RUNTIMEREGISTRY_SOURCES})
add_library(RuntimeRegistry::runtimeregistry ALIAS runtimeregistry)

target_include_directories(runtimeregistry
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

set_target_properties(runtimeregistry PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR})

if(WIN32)
  target_link_libraries(runtimeregistry PUBLIC ws2_32)
endif()

if(RUNTIMEREGISTRY_BUILD_CLI)
  add_executable(runtime-registry apps/cli.cpp)
  target_link_libraries(runtime-registry PRIVATE RuntimeRegistry::runtimeregistry)
endif()

if(WIN32 AND RUNTIMEREGISTRY_ENABLE_SYNTHETIC_DISTRIBUTED)
  add_executable(runtime-registry-coordinator apps/coordinator.cpp)
  target_link_libraries(runtime-registry-coordinator PRIVATE RuntimeRegistry::runtimeregistry)
  add_executable(runtime-registry-worker apps/worker.cpp)
  target_link_libraries(runtime-registry-worker PRIVATE RuntimeRegistry::runtimeregistry)
endif()

if(RUNTIMEREGISTRY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(RUNTIMEREGISTRY_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(RUNTIMEREGISTRY_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS runtimeregistry
        EXPORT RuntimeRegistryTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT RuntimeRegistryTargets
        FILE RuntimeRegistryTargets.cmake
        NAMESPACE RuntimeRegistry::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RuntimeRegistry)

configure_package_config_file(
  cmake/RuntimeRegistryConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/RuntimeRegistryConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RuntimeRegistry)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/RuntimeRegistryConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/RuntimeRegistryConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/RuntimeRegistryConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RuntimeRegistry)

set(CPACK_PACKAGE_NAME "RuntimeRegistry")
set(CPACK_PACKAGE_VENDOR "Summon Software Labs")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP;TGZ")
include(CPack)
