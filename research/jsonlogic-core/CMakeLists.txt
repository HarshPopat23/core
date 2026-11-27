cmake_minimum_required(VERSION 3.24)
project(core_jsonlogic_facilities LANGUAGES CXX)

# Research probes only: no JsonLogic engine, public API, or default build changes.
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." core EXCLUDE_FROM_ALL)
add_executable(core_jsonlogic_facilities probes.cc)
target_compile_features(core_jsonlogic_facilities PRIVATE cxx_std_23)
set_target_properties(core_jsonlogic_facilities PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(core_jsonlogic_facilities PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(core_jsonlogic_facilities PRIVATE
  sourcemeta::core::json sourcemeta::core::jsonpointer
  sourcemeta::core::jsonpath sourcemeta::core::memory
  sourcemeta::core::numeric sourcemeta::core::unicode
  sourcemeta::core::regex sourcemeta::core::time
  sourcemeta::core::test sourcemeta::core::benchmark
  ${SOURCEMETA_CORE_ALLOCATOR_TARGET})

enable_testing()
add_test(NAME research.jsonlogic.facilities COMMAND core_jsonlogic_facilities)
set_tests_properties(research.jsonlogic.facilities PROPERTIES TIMEOUT 30)
