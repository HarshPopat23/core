cmake_minimum_required(VERSION 3.24)
project(core_http_usockets LANGUAGES CXX)

find_package(Core REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(USOCKETS_ROOT "" CACHE PATH "Path to a built uSockets checkout")
find_path(USOCKETS_INCLUDE_DIR libusockets.h
          PATHS "${USOCKETS_ROOT}/src" NO_DEFAULT_PATH REQUIRED)
find_file(USOCKETS_LIBRARY uSockets.a
          PATHS "${USOCKETS_ROOT}" NO_DEFAULT_PATH REQUIRED)

add_executable(core_http_usockets server.cc)
target_compile_features(core_http_usockets PRIVATE cxx_std_23)
target_include_directories(core_http_usockets SYSTEM PRIVATE "${USOCKETS_INCLUDE_DIR}")
target_link_libraries(core_http_usockets PRIVATE
                      sourcemeta::core::http "${USOCKETS_LIBRARY}")

enable_testing()
add_test(NAME core.http.usockets
         COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test.py"
                 $<TARGET_FILE:core_http_usockets>)
set_tests_properties(core.http.usockets PROPERTIES TIMEOUT 30)
