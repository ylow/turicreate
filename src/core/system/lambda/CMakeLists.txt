project(Turi)

make_library(python_callbacks OBJECT
  SOURCES
    python_callbacks.cpp
  REQUIRES
    logger
)

make_library(pylambda OBJECT
  SOURCES
    worker_pool.cpp
    lambda_constants.cpp
    lambda_master.cpp
    pylambda_function.cpp
  REQUIRES
    flexible_type cppipc xframe shmipc process python_callbacks
  EXTERNAL_VISIBILITY
)

make_library(pylambda_worker OBJECT
  SOURCES
    pylambda.cpp
    pylambda_worker.cpp
  REQUIRES
    flexible_type cppipc xframe shmipc process python_callbacks
  EXTERNAL_VISIBILITY
)
