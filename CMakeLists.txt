# =============================================================================
# agent - Built on Position-Independent Runtime
# =============================================================================

cmake_minimum_required(VERSION 3.20)
include(${CMAKE_SOURCE_DIR}/runtime/cmake/Toolchain.cmake)
project(agent LANGUAGES CXX)
include(${CMAKE_SOURCE_DIR}/runtime/cmake/Common.cmake)
include(${PIR_ROOT_DIR}/cmake/Target.cmake)
