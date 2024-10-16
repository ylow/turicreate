project(Turi)

add_subdirectory(feature_engineering)
add_subdirectory(ml_data_2)
add_subdirectory(synthetic_timings)
add_subdirectory(recsys)
add_subdirectory(sparse_similarity)
add_subdirectory(pattern_mining)

make_boost_test(kmeans_test.cxx
  REQUIRES unity_shared_for_testing
)

make_boost_test (nearest_neighbors.cxx
  REQUIRES unity_shared_for_testing
)

make_boost_test (topic_model.cxx
  REQUIRES unity_shared_for_testing
)

make_boost_test (test_evaluation.cxx
  REQUIRES unity_shared_for_testing
)
