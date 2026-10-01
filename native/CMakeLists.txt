cmake_minimum_required(VERSION 3.16)
project(video_grabber_native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

# Gerätefreier Kern, auf dem Entwicklungsrechner und auf dem Pi testbar.
add_library(vg_core STATIC src/uyvy.cc src/v4l2_capture.cc)
target_include_directories(vg_core PUBLIC src)
set_property(TARGET vg_core PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(vg_probe tools/vg_probe.cc)
target_link_libraries(vg_probe vg_core)

include(CTest)
if (BUILD_TESTING)
  add_executable(uyvy_test test/uyvy_test.cc)
  target_link_libraries(uyvy_test vg_core)
  add_test(NAME uyvy_test COMMAND uyvy_test)
  add_executable(v4l2_capture_test test/v4l2_capture_test.cc)
  target_link_libraries(v4l2_capture_test vg_core)
  add_test(NAME v4l2_capture_test COMMAND v4l2_capture_test)
endif ()
