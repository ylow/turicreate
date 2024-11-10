project(Turi)

make_library(xframe OBJECT
  SOURCES
    dataframe.cpp
    sarray_index_file.cpp
    xframe_constants.cpp
    xframe_config.cpp
    xframe.cpp
    xframe_reader.cpp
    xframe_index_file.cpp
    parallel_csv_parser.cpp
    xframe_io.cpp
    shuffle.cpp
    csv_line_tokenizer.cpp
    sarray_v2_block_manager.cpp
    sarray_v2_type_encoding.cpp
    sarray_v2_block_writer.cpp
    sarray_sorted_buffer.cpp
    sarray_v2_encoded_block.cpp
    groupby.cpp
    groupby_aggregate.cpp
    groupby_aggregate_impl.cpp
    group_aggregate_value.cpp
    xframe_iterators.cpp
    csv_writer.cpp
    join.cpp
    join_impl.cpp
    unfair_lock.cpp
    xframe_rows.cpp
    testing_utils.cpp
    xframe_saving.cpp
    xframe_saving_impl.cpp
    xframe_compact.cpp
    rolling_aggregate.cpp
  REQUIRES
    random flexible_type fileio parallel lz4
    cancel_serverside_ops serialization libjson globals
  EXTERNAL_VISIBILITY
)
