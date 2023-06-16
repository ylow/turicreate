
subdirs(
  external
  capi
  core
  ml
  toolkits
  model_server
  visualization)


if(TC_BUILD_PYTHON)
  subdirs(python)

  include_directories(${PYTHON_INCLUDE_DIRS})
  message("Python library = ${PYTHON_LIBRARIES}")

endif()

