
# This library is only needed to parse s3 metadata.
if(NOT ${TC_BUILD_REMOTEFS})
  make_empty_library(libxml2)
  make_empty_library(ex_libxml2)
  return()
endif()

set(EXTRA_CONFIGURE_FLAGS "")
if(WIN32 AND ${MSYS_MAKEFILES})
  set(EXTRA_CONFIGURE_FLAGS --build=x86_64-w64-mingw32)
endif()

if(APPLE)
  set(__SDKCMD "SDKROOT=${CMAKE_OSX_SYSROOT}")

  ExternalProject_Add(ex_libxml2_x86
    PREFIX ${CMAKE_SOURCE_DIR}/deps/build/libxml2_x86
    URL ${CMAKE_SOURCE_DIR}/deps/src/libxml2-2.9.1/
    INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/local
    CONFIGURE_COMMAND bash -c "${__SDKCMD} CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=\"-target x86_64-apple-macos10.12 ${CMAKE_C_FLAGS} -w -Wno-everything\" <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared=no --enable-static=yes --without-lzma --libdir=<INSTALL_DIR>/libxml_x86 --with-python=./ ${EXTRA_CONFIGURE_FLAGS}"
    BUILD_COMMAND cp <SOURCE_DIR>/testchar.c <SOURCE_DIR>/testapi.c && ${__SDKCMD} make
    BUILD_BYPRODUCTS ${CMAKE_SOURCE_DIR}/deps/local/libxml_x86/libxml2.a
    )
  ExternalProject_Add(ex_libxml2_arm
    PREFIX ${CMAKE_SOURCE_DIR}/deps/build/libxml2_arm
    URL ${CMAKE_SOURCE_DIR}/deps/src/libxml2-2.9.1/
    INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/local
    CONFIGURE_COMMAND bash -c "${__SDKCMD} CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=\"-target arm64-apple-macos11 ${CMAKE_C_FLAGS} -w -Wno-everything\" <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared=no --enable-static=yes --without-lzma --libdir=<INSTALL_DIR>/libxml_arm --with-python=./ ${EXTRA_CONFIGURE_FLAGS}"
    BUILD_COMMAND cp <SOURCE_DIR>/testchar.c <SOURCE_DIR>/testapi.c && ${__SDKCMD} make
    BUILD_BYPRODUCTS ${CMAKE_SOURCE_DIR}/deps/local/libxml_arm/libxml2.a
    )
  add_library(libxml2_arm STATIC IMPORTED)
  set_property(TARGET libxml2_arm PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/libxml_arm/libxml2.a)
  add_library(libxml2_x86 STATIC IMPORTED)
  set_property(TARGET libxml2_x86 PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/libxml_x86/libxml2.a)

  add_library(libxml2a INTERFACE)
  target_link_libraries(libxml2a INTERFACE libxml2_arm libxml2_x86)
  add_dependencies(libxml2a ex_libxml2_x86)
  add_dependencies(libxml2a ex_libxml2_arm)
else()

  ExternalProject_Add(ex_libxml2
    PREFIX ${CMAKE_SOURCE_DIR}/deps/build/libxml2
    URL ${CMAKE_SOURCE_DIR}/deps/src/libxml2-2.9.1/
    INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/local
    CONFIGURE_COMMAND bash -c "${__SDKCMD} CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=\"${__ARCH_FLAG} ${CMAKE_C_FLAGS} -w -Wno-everything\" <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared=no --enable-static=yes --without-lzma --libdir=<INSTALL_DIR>/lib --with-python=./ ${EXTRA_CONFIGURE_FLAGS}"
    BUILD_COMMAND cp <SOURCE_DIR>/testchar.c <SOURCE_DIR>/testapi.c && ${__SDKCMD} make
    BUILD_BYPRODUCTS ${CMAKE_SOURCE_DIR}/deps/local/lib/libxml2.a
    )
  add_library(libxml2a STATIC IMPORTED)
  set_property(TARGET libxml2a PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/lib/libxml2.a)
endif()
# the with-python=./ prevents it from trying to build/install some python stuff
# which is poorly installed (always ways to stick it in a system directory)
include_directories(${CMAKE_SOURCE_DIR}/deps/local/include/libxml2)


add_library(libxml2 INTERFACE )
add_dependencies(libxml2 ex_libxml2)
target_link_libraries(libxml2 INTERFACE libxml2a)
if(WIN32)
  target_link_libraries(libxml2 INTERFACE iconv ws2_32)
endif()
target_compile_definitions(libxml2 INTERFACE HAS_LIBXML2)
set(HAS_LIBXML2 TRUE CACHE BOOL "")
