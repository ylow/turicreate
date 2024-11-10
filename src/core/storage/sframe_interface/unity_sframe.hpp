/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_UNITY_XFRAME_HPP
#define TURI_UNITY_XFRAME_HPP

#include <memory>
#include <string>
#include <vector>
#include <model_server/lib/api/unity_xframe_interface.hpp>
#include <core/storage/xframe_interface/unity_sarray.hpp>
#include <core/storage/xframe_data/group_aggregate_value.hpp>
#include <core/storage/xframe_data/xframe_rows.hpp>
#include <visualization/server/plot.hpp>

namespace turi {

// forward declarations
class xframe;
class dataframe;
class xframe_reader;
class xframe_iterator;

namespace query_eval {
struct planner_node;
} // query_eval


/**
 * This is the XFrame object exposed to Python. It stores internally an
 * \ref xframe object which is a collection of named columns, each of flexible
 * type. The XFrame represents a complete immutable collection of columns.
 * Once created, it cannot be modified. However, shallow copies or sub-selection
 * of columns can be created cheaply.
 *
 * Internally it is simply a single shared_ptr to a \ref xframe object. The
 * xframe construction is delayed until one of the construct calls are made.
 *
 * \code
 * unity_xframe frame;
 * // construct
 * frame.construct(...)
 * // frame is now immutable.
 * \endcode
 *
 * The XFrame may require temporary on disk storage which will be deleted
 * on program termination. Temporary file names are obtained from
 * \ref turi::get_temp_name
 */
class unity_xframe : public unity_xframe_base {
 public:
  /**
   * Default constructor. Does nothing
   */
  unity_xframe();

  /**
   * Destructor. Calls clear().
   */
  ~unity_xframe();

  /**
   * Constructs an XFrame using a dataframe as input.
   * Dataframe must not contain NaN values.
   */
  void construct_from_dataframe(const dataframe_t& df) override;

  /**
   * Constructs an XFrame using a xframe as input.
   */
  void construct_from_xframe(const xframe& sf);

  /**
   * Constructs an XFrame from an existing directory on disk saved with
   * save_frame() or a on disk sarray prefix (saved with
   * save_frame_by_index_file()). This function will automatically detect if
   * the location is a directory, or a file. The files will not be deleted on
   * destruction.  If the current object is already storing an frame, it is
   * cleared (\ref clear()). May throw an exception on failure. If an exception
   * occurs, the contents of SArray is empty.
   */
  void construct_from_xframe_index(std::string index_file) override;

  /**
   * Constructs an XFrame from one or more csv files.
   * To keep the interface stable, the CSV parsing configuration read from a
   * map of string->flexible_type called parsing_config. The URL can be a single
   * filename or a directory name. When passing in a directory and the pattern
   * is non-empty, we will attempt to treat it as a glob pattern.
   *
   * The default parsing configuration is the following:
   * \code
   * bool use_header = true;
   * tokenizer.delimiter = ",";
   * tokenizer.comment_char = '\0';
   * tokenizer.escape_char = '\\';
   * tokenizer.double_quote = true;
   * tokenizer.quote_char = '\"';
   * tokenizer.skip_initial_space = true;
   * \endcode
   *
   * The fields in parsing config are:
   *  - use_header : True if not is_zero()
   *  - delimiter : The entire delimiter string
   *  - comment_char : First character if flexible_type is a string
   *  - escape_char : First character if flexible_type is a string
   *  - double_quote : True if not is zero()
   *  - quote_char : First character if flexible_type is a string
   *  - skip_initial_space : True if not is zero()
   */
  std::map<std::string, std::shared_ptr<unity_sarray_base>> construct_from_csvs(
      std::string url,
      std::map<std::string, flexible_type> parsing_config,
      std::map<std::string, flex_type_enum> column_type_hints) override;

  void construct_from_planner_node(std::shared_ptr<query_eval::planner_node> node,
                                   const std::vector<std::string>& column_names);

  /**
   * Saves a copy of the current xframe into a directory.
   * Does not modify the current xframe.
   */
  void save_frame(std::string target_directory) override;

  /**
   * Performs an incomplete save of an existing XFrame into a directory.
   * This saved XFrame may reference XFrames in other locations *in the same
   * filesystem* for certain columns/segments/etc.
   *
   * Does not modify the current xframe.
   */
  void save_frame_reference(std::string target_directory) override;


  /**
   * Saves a copy of the current xframe into a target location defined by
   * an index file. DOes not modify the current xframe.
   */
  void save_frame_by_index_file(std::string index_file);

  /**
   * Clears the contents of the XFrame.
   */
  void clear() override;

  /**
   * Returns the number of rows in the XFrame. Returns 0 if the XFrame is empty.
   */
  size_t size() override;

  /**
   * Returns the number of columns in the XFrame.
   * Returns 0 if the xframe is empty.
   */
  size_t num_columns() override;

  /**
   * Returns an array containing the datatype of each column. The length
   * of the return array is equal to num_columns(). If the xframe is empty,
   * this returns an empty array.
   */
  std::vector<flex_type_enum> dtype() override;

  /**
   * Returns the dtype of a particular column.
   */
  flex_type_enum dtype(size_t column_index);

  /**
   * Returns the dtype of a particular column.
   */
  flex_type_enum dtype(const std::string& column_name);

  /**
   * Returns an array containing the name of each column. The length
   * of the return array is equal to num_columns(). If the xframe is empty,
   * this returns an empty array.
   */
  std::vector<std::string> column_names() override;

  /**
   * Returns some number of rows of the XFrame in a dataframe representation.
   * if nrows exceeds the number of rows in the XFrame ( \ref size() ), this
   * returns only \ref size() rows.
   */
  std::shared_ptr<unity_xframe_base> head(size_t nrows) override;

  /**
   *  Returns the index of the column `name`
   */
  size_t column_index(const std::string& name) override;

  /**
   *  Returns the name of the column in position `index.`
   */
  const std::string& column_name(size_t index);

  /**
   * Returns true if the column is present in the xframe, and false
   * otherwise.
   */
  bool contains_column(const std::string &name);

  /**
   * Same as head, returning dataframe.
   */
  dataframe_t _head(size_t nrows) override;

  /**
   * Returns some number of rows from the end of the XFrame in a dataframe
   * representation. If nrows exceeds the number of rows in the XFrame
   * ( \ref size() ), this returns only \ref size() rows.
   */
  std::shared_ptr<unity_xframe_base> tail(size_t nrows) override;

  /**
   * Same as head, returning dataframe.
   */
  dataframe_t _tail(size_t nrows) override;

  /**
   * Returns an SArray with the column that corresponds to 'name'.  Throws an
   * exception if the name is not in the current XFrame.
   */
  std::shared_ptr<unity_sarray_base> select_column(const std::string &name) override;

  /**
   * Returns an SArray with the column that corresponds to index idx.  Throws an
   * exception if the name is not in the current XFrame.
   */
  std::shared_ptr<unity_sarray_base> select_column(size_t idx);

  /**
   * Returns a new XFrame which is filtered by a given logical column.
   * The index array must be the same length as the current array. An output
   * array is returned containing only the elements in the current where are the
   * corresponding element in the index array evaluates to true.
   */
  std::shared_ptr<unity_xframe_base> logical_filter(std::shared_ptr<unity_sarray_base> index) override;

  /**
   * Returns an lazy xframe with the columns that have the given names. Throws an
   * exception if ANY of the names given are not in the current XFrame.
   */
  std::shared_ptr<unity_xframe_base> select_columns(const std::vector<std::string> &names) override;

  /**
   * Returns an lazy xframe with the columns given by the indices.
   */
  std::shared_ptr<unity_xframe_base> select_columns(const std::vector<size_t>& indices);

  /**
   * Returns an lazy xframe which a the copy of the current one
  */
  std::shared_ptr<unity_xframe_base> copy();

  /**
   * Mutates the current XFrame by adding the given column.
   *
   * Throws an exception if:
   *  - The given column has a different number of rows than the XFrame.
   */
  void add_column(std::shared_ptr<unity_sarray_base >data, const std::string &name) override;

  /**
   * Mutates the current XFrame by adding the given columns.
   *
   * Throws an exception if ANY given column cannot be added
   * (for one of the reasons that add_column can fail).
   *
   * \note Currently leaves the XFrame in an unfinished state if one of the
   * columns fails...the columns before that were added successfully will
   * be there. This needs to be changed.
   */
  void add_columns(std::list<std::shared_ptr<unity_sarray_base>> data_list,
                   std::vector<std::string> name_vec) override;

  /**
   * Returns a new sarray which is a transform of each row in the xframe
   * using a Python lambda function pickled into a string.
   */
  std::shared_ptr<unity_sarray_base> transform(const std::string& lambda,
                                               flex_type_enum type,
                                               bool skip_undefined,
                                               uint64_t seed) override;

  /**
   * Returns a new sarray which is a transform of each row in the xframe
   * using a Python lambda function pickled into a string.
   */
  std::shared_ptr<unity_sarray_base> transform_native(const function_closure_info& lambda,
                                                      flex_type_enum type,
                                                      bool skip_undefined,
                                                      uint64_t seed) override;

  /**
   * Returns a new sarray which is a transform of each row in the xframe
   * using a Python lambda function pickled into a string.
   */
  std::shared_ptr<unity_sarray_base> transform_lambda(
      std::function<flexible_type(const xframe_rows::row&)> lambda,
      flex_type_enum type,
      uint64_t seed);

  /**
   * Returns a new sarray which is a transform of each row in the xframe
   * using a Python lambda function pickled into a string.
   */
  std::shared_ptr<unity_xframe_base> flat_map(const std::string& lambda,
                                              std::vector<std::string> output_column_names,
                                              std::vector<flex_type_enum> output_column_types,
                                              bool skip_undefined,
                                              uint64_t seed) override;

  /**
   * Set the ith column name.
   *
   * Throws an exception if index out of bound or name already exists.
   */
  void set_column_name(size_t i, std::string name) override;

  /**
   * Remove the ith column.
   */
  void remove_column(size_t i) override;

  /**
   * Swap the ith and jth columns.
   */
  void swap_columns(size_t i, size_t j) override;

  /**
   * Returns the underlying shared_ptr to the xframe object.
   */
  std::shared_ptr<xframe> get_underlying_xframe();

  /**
   * Returns the underlying planner pointer
   */
  std::shared_ptr<query_eval::planner_node> get_planner_node();

  /**
   * Sets the private shared pointer to an xframe.
   */
  void set_xframe(const std::shared_ptr<xframe>& sf_ptr);

  /**
   * Begin iteration through the XFrame.
   *
   * Works together with \ref iterator_get_next(). The usage pattern
   * is as follows:
   * \code
   * xframe.begin_iterator();
   * while(1) {
   *   auto ret = xframe.iterator_get_next(64);
   *   // do stuff
   *   if (ret.size() < 64) {
   *     // we are done
   *     break;
   *   }
   * }
   * \endcode
   *
   * Note that use of pretty much any of the other data-dependent SArray
   * functions will invalidate the iterator.
   */
  void begin_iterator() override;

  /**
   * Obtains the next block of elements of size len from the XFrame.
   * Works together with \ref begin_iterator(). See the code example
   * in \ref begin_iterator() for details.
   *
   * This function will always return a vector of length 'len' unless
   * at the end of the array, or if an error has occured.
   *
   * \param len The number of elements to return
   * \returns The next collection of elements in the array. Returns less then
   * len elements on end of file or failure.
   */
  std::vector< std::vector<flexible_type> > iterator_get_next(size_t len) override;

  /**
   * Save the xframe to url in csv format.
   * To keep the interface stable, the CSV parsing configuration read from a
   * map of string->flexible_type called writing_config.
   *
   * The default writing configuration is the following:
   * \code
   * writer.delimiter = ",";
   * writer.escape_char = '\\';
   * writer.double_quote = true;
   * writer.quote_char = '\"';
   * writer.use_quote_char = true;
   * \endcode
   *
   * For details on the meaning of each config see \ref csv_writer
   *
   * The fields in parsing config are:
   *  - delimiter : First character if flexible_type is a string
   *  - escape_char : First character if flexible_type is a string
   *  - double_quote : True if not is zero()
   *  - quote_char : First character if flexible_type is a string
   *  - use_quote_char : First character if flexible_type is a string
   */
  void save_as_csv(const std::string& url,
                   std::map<std::string, flexible_type> writing_config) override;

  /**
   * Randomly split the xframe into two parts, with ratio = percent, and  seed = random_seed.
   *
   * Returns a list of size 2 of the unity_xframes resulting from the split.
   */
  std::list<std::shared_ptr<unity_xframe_base>> random_split(float percent, uint64_t random_seed, bool exact=false) override;

  /**
   * Randomly shuffles the xframe.
   *
   * Returns a list of size 2 of the unity_xframes resulting from the split.
   */
  std::shared_ptr<unity_xframe_base> shuffle() override;

  /**
   * Sample the rows of xframe uniformly with ratio = percent, and seed = random_seed.
   *
   * Returns unity_xframe* containing the sampled rows.
   */
  std::shared_ptr<unity_xframe_base> sample(float percent, uint64_t random_seed, bool exact=false) override;

  /**
   * materialize the xframe, this is different from save() as this is a temporary persist of
   * all sarrays underneath the xframe to speed up some computation (for example, lambda)
   * this will NOT create a new uity_xframe.
  **/
  void materialize() override;

  /**
   * Returns whether or not this xframe is materialized
   **/
  bool is_materialized() override;

  /**
   * Return the query plan as a string representation of a dot graph.
   */
  std::string query_plan_string() override;

  /**
   * Return true if the xframe size is known.
   */
  bool has_size() override;

  /**
   * Returns unity_xframe* where there is one row for each unique value of the
   * key_column.
   * group_operations is a collection of pairs of {column_name, operation_name}
   * where operation_name is a builtin operator.
   */
  std::shared_ptr<unity_xframe_base> groupby_aggregate(
      const std::vector<std::string>& key_columns,
      const std::vector<std::vector<std::string>>& group_columns,
      const std::vector<std::string>& group_output_columns,
      const std::vector<std::string>& group_operations) override;

  /**
   * \overload
   */
  std::shared_ptr<unity_xframe_base> groupby_aggregate(
      const std::vector<std::string>& key_columns,
      const std::vector<std::vector<std::string>>& group_columns,
      const std::vector<std::string>& group_output_columns,
      const std::vector<std::shared_ptr<group_aggregate_value>>& group_operations);

  /**
   * Returns a new XFrame which contains all rows combined from current XFrame and "other"
   * The "other" XFrame has to have the same number of columns with the same column names
   * and same column types as "this" XFrame
   */
  std::shared_ptr<unity_xframe_base> append(std::shared_ptr<unity_xframe_base> other) override;

  inline std::shared_ptr<unity_xframe_base> join(std::shared_ptr<unity_xframe_base >right,
                          const std::string join_type,
                          const std::map<std::string,std::string>& join_keys) override
  { return join_with_custom_name(right, join_type, join_keys, {}); }

  std::shared_ptr<unity_xframe_base> join_with_custom_name(std::shared_ptr<unity_xframe_base >right,
                          const std::string join_type,
                          const std::map<std::string,std::string>& join_keys,
                          const std::map<std::string,std::string>& alternative_names) override;

  std::shared_ptr<unity_xframe_base> sort(const std::vector<std::string>& sort_keys,
                          const std::vector<int>& sort_ascending) override;

  /**
    * Pack a subset columns of current XFrame into one dictionary column, using
    * column name as key in the dictionary, and value of the column as value
    * in the dictionary, returns a new XFrame that includes other non-packed
    * columns plus the newly generated dict column.
    * Missing value in the original column will not show up in the packed
    * dictionary value.

    * \param pack_column_names : list of column names to pack
    * \param dict_key_names : dictionary key name to give to the packed dictionary
    * \param dtype: the result SArray type
      missing value is maintained, it could be filled with fill_na value is specified.
    * \param fill_na: the value to fill when missing value is encountered

    * Returns a new SArray that contains the newly packed column
  **/
  std::shared_ptr<unity_sarray_base> pack_columns(
      const std::vector<std::string>& pack_column_names,
      const std::vector<std::string>& dict_key_names,
      flex_type_enum dtype,
      const flexible_type& fill_na) override;

  /**
   * Convert a dictionary column of the XFrame to two columns with first column
   * as the key for the dictionary and second column as the value for the
   * dictionary. Returns a new XFrame with the two newly created columns, plus
   * all columns other than the stacked column. The values from those columns
   * are duplicated for all rows created from the same original row.

   * \param column_name: string
      The column to stack. The name must come from current XFrame and must be of dict type

   * \param new_column_names: a list of str, optional
      Must be length of two. The two column names to stack the dict value to.
      If not given, the name is automatically generated.

   * \param new_column_types: a list of types, optional
      Must be length of two. The type for the newly created column. If not
      given, the default to [str, int].

   * \param drop_na if true, missing values from dictionary will be ignored. If false,
      for missing dict value, one row will be created with the two new columns' value
      being missing value

   * Retruns a new unity_xframe with stacked columns
  **/
  std::shared_ptr<unity_xframe_base> stack(
      const std::string& column_name,
      const std::vector<std::string>& new_column_names,
      const std::vector<flex_type_enum>& new_column_types,
      bool drop_na) override;

   /**
    * Extracts a range of rows from an XFrame as a new XFrame.
    * This will extract rows beginning at start (inclusive) and ending at
    * end(exclusive) in steps of "step".
    * step must be at least 1.
    */
  std::shared_ptr<unity_xframe_base> copy_range(size_t start, size_t step, size_t end) override;

  /**
   * Returns a new XFrame with missing values dropped.
   *
   * Missing values are only searched for in the columns specified in the
   * 'column_names'.  If this vector is empty, all columns will be considered.
   * If 'all' is true, a row is only dropped if all specified columns contain a
   * missing value.  If false, the row is dropped if any of the specified
   * columns contain a missing value.
   *
   * If 'split' is true, this function returns two XFrames, the first being the
   * XFrame with missing values dropped, and the second consisting of all the
   * rows removed.
   *
   * If 'recursive' is true, the `nan`element check will be perfromed in
   * a recursive manner to check each unit in a container-like flexible-typed
   * cell in XFrame.
   *
   * Throws if the column names are not in this XFrame, or if too many are given.
   */
  std::list<std::shared_ptr<unity_xframe_base>> drop_missing_values(
      const std::vector<std::string>& column_names, bool all, bool split,
      bool recursive) override;

  dataframe_t to_dataframe() override;

  void save(oarchive& oarc) const override;

  void load(iarchive& iarc) override;

  void delete_on_close() override;

  /**
   * Similar to logical filter, but return both positive and negative rows.
   *
   * \param logical_filter_array is an sarray of the same size, and has only
   * zeros and ones as value.
   *
   * Return a list of two xframes with all positive examples goes to the first
   * one and negative rows goes to the second one.
   */
  std::list<std::shared_ptr<unity_xframe_base>> logical_filter_split(
    std::shared_ptr<unity_sarray_base> logical_filter_array);

  void explore(const std::string& path_to_client, const std::string& title) override;
  void show(const std::string& path_to_client) override;
  std::shared_ptr<model_base> plot() override;

 private:
  /**
   * Pointer to the lazy evaluator logical operator node.
   * Should never be NULL.  Must be set with the set_planner_node() function above.
   */
  std::shared_ptr<query_eval::planner_node> m_planner_node;

  std::vector<std::string> m_column_names;

  std::shared_ptr<xframe> m_cached_xframe;

  /**
   * Supports \ref begin_iterator() and \ref iterator_get_next().
   * The next segment I will read. (i.e. the current segment I am reading
   * is iterator_next_segment_id - 1)
   */
  size_t iterator_next_segment_id = 0;

  /**
   * A copy of the current XFrame. This allows iteration, and other
   * SAarray operations to operate together safely in harmony without collisions.
   */
  std::unique_ptr<xframe_reader> iterator_xframe_ptr;

  /**
   * Supports \ref begin_iterator() and \ref iterator_get_next().
   * The begin iterator of the current segment I am reading.
   */
  std::unique_ptr<xframe_iterator> iterator_current_segment_iter;

  /**
   * Supports \ref begin_iterator() and \ref iterator_get_next().
   * The end iterator of the current segment I am reading.
   */
  std::unique_ptr<xframe_iterator> iterator_current_segment_enditer;


 private:
  // Helper functions

  /**
   * Convert column names to column indices.
   *
   * If input column_names is empty, return 0,1,2,...num_columns-1
   *
   * Throw if column_names has duplication, or some column name does not exist.
   */
  std::vector<size_t> _convert_column_names_to_indices(const std::vector<std::string>& column_names);

  /**
   * Generate a new column name
   *
   * New column name is in the form of X1, X2, X3 ....
   * In case of conflict, add .1, .2 until conflict is resolved.
   *
   * \example
   *
   * Given current xframe column names: a, b, c
   * Next 3 generated names are: X4, X5, X6
   *
   * Given current xframe column names: X4, X5.1, X6.2
   * Next 3 generated names are: X4.1, X5, X6.1
   */
  std::string generate_next_column_name();
};



}

#endif
