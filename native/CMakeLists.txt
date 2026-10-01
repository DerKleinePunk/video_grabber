cmake_minimum_required(VERSION 3.16)
project(video_grabber_native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

# Gerätefreier Kern, auf dem Entwicklungsrechner und auf dem Pi testbar.
add_library(vg_core STATIC src/uyvy.cc src/v4l2_capture.cc src/status.cc src/params.cc)
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
  add_executable(status_test test/status_test.cc)
  target_link_libraries(status_test vg_core)
  add_test(NAME status_test COMMAND status_test)
  add_executable(params_test test/params_test.cc)
  target_link_libraries(params_test vg_core)
  add_test(NAME params_test COMMAND params_test)
endif ()

# Platform-View-Plugin für ivi-homescreen, gebaut gegen ein Shell-Build-
# Verzeichnis wie test/integration/pv_bench dort:
#   cmake -B build -DIHS_BUILD_DIR=<shell-build> -DIHS_SOURCE_DIR=<ivi-homescreen>
#         -DCMAKE_TOOLCHAIN_FILE=<emb-toolchain>
if (IHS_BUILD_DIR)
  if (NOT IHS_SOURCE_DIR)
    message(FATAL_ERROR "IHS_SOURCE_DIR (ivi-homescreen) fehlt")
  endif ()
  set(_ihs_shared_so "${IHS_BUILD_DIR}/shell/lib/libihs_shared.so")
  if (NOT EXISTS "${_ihs_shared_so}")
    message(FATAL_ERROR "kein libihs_shared.so in ${IHS_BUILD_DIR}")
  endif ()
  # pkg-config weiß nichts vom Querbau (Hinweis aus pv_bench).
  if (CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
    file(GLOB _pc_dirs
         "${CMAKE_SYSROOT}/usr/lib/*/pkgconfig"
         "${CMAKE_SYSROOT}/usr/lib/pkgconfig"
         "${CMAKE_SYSROOT}/usr/share/pkgconfig")
    string(REPLACE ";" ":" _pc_path "${_pc_dirs}")
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
    set(ENV{PKG_CONFIG_LIBDIR} "${_pc_path}")
  endif ()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GBM REQUIRED IMPORTED_TARGET gbm)
  pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
  find_package(Threads REQUIRED)

  add_library(video_grabber_view SHARED src/grabber_view.cc)
  set_target_properties(video_grabber_view PROPERTIES CXX_VISIBILITY_PRESET hidden)
  target_include_directories(video_grabber_view PRIVATE
                             ${IHS_SOURCE_DIR}/shared/include
                             ${IHS_BUILD_DIR}/shared/include)
  target_link_libraries(video_grabber_view PRIVATE vg_core ${_ihs_shared_so}
                        PkgConfig::GBM PkgConfig::LIBDRM Threads::Threads)
endif ()
