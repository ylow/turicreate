if(NOT ${TC_BUILD_REMOTEFS})
  make_empty_library(openssl)
  return()
endif()

message(STATUS "Building OpenSSL library.")

# WARNING:  OPENSSL DOES NOT SUPPORT PARALLEL BUILDS. make -j1 MUST BE USED!!!!!

# Note, the following list was generated from running ls in the openssl directory
#
set(_openssl_installed_headers
pem2.h pem.h ssl3.h ossl_typ.h dtls1.h err.h bn.h blowfish.h cms.h engine.h conf_api.h x509.h asn1_mac.h ui.h kssl.h sha.h symhacks.h asn1.h opensslconf.h bio.h rc2.h dh.h ui_compat.h x509v3.h ssl23.h conf.h md5.h x509_vfy.h txt_db.h safestack.h ecdsa.h objects.h pkcs12.h crypto.h opensslv.h pkcs7.h obj_mac.h buffer.h ssl.h srp.h camellia.h evp.h e_os2.h md4.h hmac.h aes.h comp.h cast.h rc4.h stack.h des.h ocsp.h ec.h ecdh.h rand.h ts.h pqueue.h dso.h seed.h modes.h ssl2.h rsa.h krb5_asn.h des_old.h ripemd.h whrlpool.h tls1.h mdc2.h dsa.h srtp.h asn1t.h cmac.h ebcdic.h idea.h lhash.h)

list(TRANSFORM _openssl_installed_headers PREPEND "${CMAKE_SOURCE_DIR}/deps/local/include/openssl/")


if(APPLE)

  set(__SDKCMD "SDKROOT=${CMAKE_OSX_SYSROOT}")
  if(${CMAKE_C_COMPILER_TARGET})
    set(__ARCH_FLAG "--target=${CMAKE_C_COMPILER_TARGET}")
  endif()

  # SSL seems to link fine even when compiled using the default compiler
  # The alternative to get openssl to use gcc on mac requires a patch to
  # the ./Configure script
  ExternalProject_Add(ex_openssl_arm
    PREFIX ${CMAKE_SOURCE_DIR}/deps/build/libssl_arm
    URL ${CMAKE_SOURCE_DIR}/deps/src/openssl-1.1.1s
    INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/local
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND bash -c "env ${__SDKCMD} CC=\"${CMAKE_C_COMPILER}\" ./Configure darwin64-arm64-cc no-rc5 --prefix=<INSTALL_DIR>  -fPIC -Os -g ${CMAKE_C_FLAGS} -Wno-everything -w"
    BUILD_COMMAND bash -c "SDKROOT=${CMAKE_OSX_SYSROOT} make -j1"
    INSTALL_COMMAND bash -c "SDKROOT=${CMAKE_OSX_SYSROOT} make -j1 install && cp ./libcrypto.a <INSTALL_DIR>/ssl/libcrypto_arm.a && cp ./libssl.a <INSTALL_DIR>/ssl/libssl_arm.a"
    BUILD_BYPRODUCTS
    ${CMAKE_SOURCE_DIR}/deps/local/lib/libssl.a
    ${CMAKE_SOURCE_DIR}/deps/local/lib/libcrypto.a
    ${CMAKE_SOURCE_DIR}/deps/local/ssl/libssl_arm.a
    ${CMAKE_SOURCE_DIR}/deps/local/ssl/libcrypto_arm.a
    ${_openssl_installed_headers}
    )

  ExternalProject_Add(ex_openssl_x86
    PREFIX ${CMAKE_SOURCE_DIR}/deps/build/libssl_x86
    URL ${CMAKE_SOURCE_DIR}/deps/src/openssl-1.1.1s
    INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/local
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND bash -c "env ${__SDKCMD} CC=\"${CMAKE_C_COMPILER}\" ./Configure darwin64-x86_64-cc no-rc5 --prefix=<INSTALL_DIR>  -fPIC -Os -g ${CMAKE_C_FLAGS} -Wno-everything -w"
    BUILD_COMMAND bash -c "SDKROOT=${CMAKE_OSX_SYSROOT} make -j1"
    INSTALL_COMMAND bash -c "SDKROOT=${CMAKE_OSX_SYSROOT} cp ./libcrypto.a <INSTALL_DIR>/ssl/libcrypto_x86.a && cp ./libssl.a <INSTALL_DIR>/ssl/libssl_x86.a"
    BUILD_BYPRODUCTS
    ${CMAKE_SOURCE_DIR}/deps/local/lib/libssl.a
    ${CMAKE_SOURCE_DIR}/deps/local/lib/libcrypto.a
    ${CMAKE_SOURCE_DIR}/deps/local/ssl/libssl_x86.a
    ${CMAKE_SOURCE_DIR}/deps/local/ssl/libcrypto_x86.a
    ${_openssl_installed_headers}
    )

  add_library(libssl_x86 STATIC IMPORTED)
  set_property(TARGET libssl_x86 PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/ssl/libssl_x86.a)

  add_library(libcrypto_x86 STATIC IMPORTED)
  set_property(TARGET libcrypto_x86 PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/ssl/libcrypto_x86.a)

  add_library(libssl_arm STATIC IMPORTED)
  set_property(TARGET libssl_arm PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/ssl/libssl_arm.a)

  add_library(libcrypto_arm STATIC IMPORTED)
  set_property(TARGET libcrypto_arm PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/ssl/libcrypto_arm.a)

  add_library(libcryptoa INTERFACE)
  target_link_libraries(libcryptoa INTERFACE libcrypto_arm libcrypto_x86)
  add_library(libssla INTERFACE)
  target_link_libraries(libssla INTERFACE libssl_arm libssl_x86)
elseif(WIN32)
ExternalProject_Add(ex_openssl
  PREFIX ${CMAKE_SOURCE_DIR}/deps/build/libssl
  URL ${CMAKE_SOURCE_DIR}/deps/src/openssl-1.1.1s
  INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/local
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ./Configure mingw64 no-idea no-mdc2 no-rc5 --prefix=<INSTALL_DIR>
  BUILD_COMMAND make depend && make -j1
  INSTALL_COMMAND make -j1 install_sw
  )
  add_library(libssla STATIC IMPORTED)
  set_property(TARGET libssla PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/lib/libssl.a)

  add_library(libcryptoa STATIC IMPORTED)
  set_property(TARGET libcryptoa PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/lib/libcrypto.a)
else()
ExternalProject_Add(ex_openssl
  PREFIX ${CMAKE_SOURCE_DIR}/deps/build/libssl
  URL ${CMAKE_SOURCE_DIR}/deps/src/openssl-1.1.1s
  INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/local
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND env CC=${CMAKE_C_COMPILER} ./config no-rc5 --prefix=<INSTALL_DIR> -fPIC -Os -g -Wno-everything
  BUILD_COMMAND make -j1
  INSTALL_COMMAND make -j1 install_sw
  BUILD_BYPRODUCTS
  ${CMAKE_SOURCE_DIR}/deps/local/lib/libssl.a
  ${CMAKE_SOURCE_DIR}/deps/local/lib/libcrypto.a
  ${_openssl_installed_headers}
  )

  add_library(libssla STATIC IMPORTED)
  set_property(TARGET libssla PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/lib/libssl.a)

  add_library(libcryptoa STATIC IMPORTED)
  set_property(TARGET libcryptoa PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/deps/local/lib/libcrypto.a)
endif()

add_library(openssl INTERFACE )
target_link_libraries(openssl INTERFACE libssla libcryptoa)
if(NOT WIN32)
  target_link_libraries(openssl INTERFACE dl)
endif()

target_compile_definitions(openssl INTERFACE HAS_OPENSSL)

add_dependencies(openssl libssla libcryptoa)
if (APPLE)
  add_dependencies(libssla ex_openssl_arm)
  add_dependencies(libcryptoa ex_openssl_arm)
  add_dependencies(libssla ex_openssl_x86)
  add_dependencies(libcryptoa ex_openssl_x86)
else()
  add_dependencies(libssla ex_openssl)
  add_dependencies(libcryptoa ex_openssl)
endif()
set(HAS_OPENSSL TRUE CACHE BOOL "")
