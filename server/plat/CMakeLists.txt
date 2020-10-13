find_package(PkgConfig REQUIRED)
#
# Implementation specific dependencies
#
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_search_module(GOBJECT REQUIRED gobject-2.0)
set (GDIAL_PLAT_DEPEND_LIBRARIES "${GLIB_LIBRARIES} ${GOBJECT_LIBRARIES}")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -std=c++11 -Wno-nonnull -DRT_PLATFORM_LINUX")

#
# Template that generates .pc file
#
set (GDIAL_PLAT_PC_LIBNAME "gdial-plat")
set (GDIAL_PLAT_PC_FILENAME ${PROJECT_SOURCE_DIR}/plat/gdial-plat.pc)
string(REGEX REPLACE " " ";" GDIAL_PLAT_DEPEND_LIBRARIES "${GDIAL_PLAT_DEPEND_LIBRARIES}")
foreach (deplib ${GDIAL_PLAT_DEPEND_LIBRARIES})
  set (GDIAL_PLAT_PC_LIBS_PRIVATE "${GDIAL_PLAT_PC_LIBS_PRIVATE} -l${deplib} ")
endforeach(deplib)
configure_file ("${GDIAL_PLAT_PC_FILENAME}.in" "${GDIAL_PLAT_PC_FILENAME}" @ONLY)

#
# Build target library
#
include_directories(
  ${GLIB_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/linux
  ${PROJECT_SOURCE_DIR}/plat
  ${CMAKE_CURRENT_SOURCE_DIR}
  $ENV{PKG_CONFIG_SYSROOT_DIR}/usr/include/rdk/rtcast
  $ENV{PKG_CONFIG_SYSROOT_DIR}/usr/include/pxcore
)

set (GDIAL_PLAT_LIB_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../linux/gdial-plat-dev.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../linux/gdial-plat-util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/gdial-plat-app.c
  ${CMAKE_CURRENT_SOURCE_DIR}/rtdial.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/rtcache.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/rtabstractservice.cpp
)

add_library(gdial-plat SHARED ${GDIAL_PLAT_LIB_SOURCE_FILES})
target_link_Libraries(gdial-plat PRIVATE ${GLIB_LIBRARIES} ${GOBJECT_LIBRARIES} -lpthread -lrtRemote -lrtCore)
