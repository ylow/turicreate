project(Turi)
set(EXTENSIONS_LIST)
macro(make_extension NAME)
  list(APPEND EXTENSIONS_LIST "${NAME}")
  message(STATUS "Registering Extension: " ${NAME})
  make_library(
    ${NAME}
    ${ARGV}
    REQUIRES
      unity_shared
      boost
      eigen
    COMPILE_FLAGS_EXTRA_GCC
      -Wno-unknown-pragmas  # NOTE: used for auto-generated protobuf source files
      -Wno-unused-function  # NOTE: used for auto-generated protobuf source files
    SHARED)
endmacro()
#----------------------------
make_extension(additional_sframe_utilities SOURCES additional_sframe_utilities.cpp)

# make_extension(grouped_sframe SOURCES grouped_sframe.cpp)

make_extension(internal_demo SOURCES internal_demo.cpp)
make_extension(json SOURCES
  json/decoder.cpp
  json/encoder.cpp
  json/json.cpp
)
set_property(DIRECTORY PROPERTY EXTENSIONS_LIST "${EXTENSIONS_LIST}")
