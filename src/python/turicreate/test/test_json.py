# -*- coding: utf-8 -*-
# Copyright © 2017 Apple Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-3-clause license that can
# be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

# This file tests invertibility (serializing to/from) the "serializable" format
# of variant_type (produced by extensions.json). This extension results in a
# naively-JSON-serializable flexible_type that should retain all necessary
# information to be rehydrated into the original variant_type.





import array
import datetime
import json  # Python built-in JSON module
import math
import os
import pandas
import pytz
import six
import string
import sys
import unittest
import tempfile
import pytest

from . import util
from .. import _json  # turicreate._json
from ..data_structures.sarray import SArray
from ..data_structures.xframe import XFrame

pytestmark = [pytest.mark.minimal]

if sys.version_info.major == 3:
    long = int


_XFrameComparer = util.XFrameComparer()


class JSONTest(unittest.TestCase):
    def _assertEqual(self, x, y):
        if type(x) in [int, int]:
            self.assertTrue(type(y) in [int, int])
        elif isinstance(x, six.string_types):
            self.assertTrue(isinstance(y, six.string_types))
        else:
            self.assertEqual(type(x), type(y))
        if isinstance(x, six.string_types):
            self.assertEqual(str(x), str(y))
        elif isinstance(x, SArray):
            _XFrameComparer._assert_sarray_equal(x, y)
        elif isinstance(x, XFrame):
            _XFrameComparer._assert_xframe_equal(x, y)
        elif isinstance(x, dict):
            for (k1, v1), (k2, v2) in zip(sorted(x.items()), sorted(y.items())):
                self._assertEqual(k1, k2)
                self._assertEqual(v1, v2)
        elif isinstance(x, list):
            for v1, v2 in zip(x, y):
                self._assertEqual(v1, v2)
        else:
            self.assertEqual(x, y)

    def _run_test_case(self, value):
        # test that JSON serialization is invertible with respect to both
        # value and type.
        (data, schema) = _json.to_serializable(value)

        # ensure that resulting value is actually naively serializable
        data = json.loads(json.dumps(data, allow_nan=False))
        schema = json.loads(json.dumps(schema, allow_nan=False))

        # print("----------------------------------")
        # print("Value: %s" % value)
        # print("Serializable Data: %s" % data)
        # print("Serializable Schema: %s" % schema)
        result = _json.from_serializable(data, schema)
        # print("Deserialized Result: %s" % result)
        # print("----------------------------------")
        self._assertEqual(result, value)
        # test that JSON serialization gives expected result
        serialized = _json.dumps(value)
        deserialized = _json.loads(serialized)
        self._assertEqual(deserialized, value)

    @unittest.skipIf(sys.platform == "win32", "Windows long issue")
    def test_int(self):
        [
            self._run_test_case(value)
            for value in [
                0,
                1,
                -2147483650,
                -2147483649,  # boundary of accurate representation in JS 64-bit float
                2147483648,  # boundary of accurate representation in JS 64-bit float
                2147483649,
            ]
        ]

    def test_float(self):
        [
            self._run_test_case(value)
            for value in [-1.1, -1.0, 0.0, 1.0, 1.1, float("-inf"), float("inf"),]
        ]
        self.assertTrue(
            math.isnan(_json.from_serializable(*_json.to_serializable(float("nan"))))
        )

    def test_string_to_json(self):
        [self._run_test_case(value) for value in ["hello", "a'b", 'a"b', "ɖɞɫɷ",]]

    def test_vec_to_json(self):
        [
            self._run_test_case(value)
            for value in [
                array.array("d"),
                array.array("d", [1.5]),
                array.array("d", [2.1, 2.5, 3.1]),
                array.array("d", [float("-inf"), float("inf")]),
            ]
        ]

    def test_list_to_json(self):
        # TODO -- we can't test lists of numbers, due to
        # Python<->flexible_type not being reversible for lists of numbers.
        # if `list` of `int` goes into C++, the flexible_type representation
        # becomes flex_vec (vector<flex_float>). This is a lossy representation.
        # known issue, can't resolve here.
        [
            self._run_test_case(value)
            for value in [
                [],
                ["hello", "world"],
                ["hello", 3, None],
                [3.14159, None],
                [{}, {"x": 1, "y": 2}],
                ["hello", float("-inf"), float("inf")],
            ]
        ]

    def test_dict_to_json(self):
        [self._run_test_case(value) for value in [{}, {"x": 1, "y": 2},]]

    def test_date_time_to_json(self):
        d = datetime.datetime(year=2016, month=3, day=5)
        [
            self._run_test_case(value)
            for value in [
                d,
                pytz.utc.localize(d),
                pytz.timezone("US/Arizona").localize(d),
            ]
        ]

    def test_sarray_to_json(self):

        d = datetime.datetime(year=2016, month=3, day=5)
        [
            self._run_test_case(value)
            for value in [
                SArray(),
                SArray([1, 2, 3]),
                SArray([1.0, 2.0, 3.0]),
                SArray([None, 3, None]),
                SArray(["hello", "world"]),
                SArray(array.array("d", [2.1, 2.5, 3.1])),
                SArray(
                    [["hello", None, "world"], ["hello", 3, None], [3.14159, None],]
                ),
                SArray([{"x": 1, "y": 2}, {"x": 5, "z": 3},]),
                SArray(
                    [d, pytz.utc.localize(d), pytz.timezone("US/Arizona").localize(d),]
                ),
            ]
        ]

    def test_xframe_to_json(self):
        [
            self._run_test_case(value)
            for value in [
                XFrame(),
                XFrame({"foo": [1, 2, 3, 4], "bar": [None, "Hello", None, "World"]}),
            ]
        ]


    def test_nested_to_json(self):
        # not tested in the cases above: nested data, nested schema
        # (but all flexible_type compatible)
        [
            self._run_test_case(value)
            for value in [
                {
                    "foo": ["a", "b", "c"],
                    "bar": array.array("d", [0.0, float("inf"), float("-inf")]),
                },
                [["a", "b", "c"], array.array("d", [0.0, float("inf"), float("-inf")])],
                {
                    "baz": {
                        "foo": ["a", "b", "c"],
                        "bar": array.array("d", [0.0, float("inf"), float("-inf")]),
                    },
                    "qux": [
                        ["a", "b", "c"],
                        array.array("d", [0.0, float("inf"), float("-inf")]),
                    ],
                },
            ]
        ]

    def test_variant_to_json(self):
        # not tested in the cases above: variant_type other than XFrame-like
        # but containing XFrame-like (so cannot be a flexible_type)
        sf = XFrame({"col1": [1, 2], "col2": ["hello", "world"]})
        sa = SArray([5.0, 6.0, 7.0])
        [self._run_test_case(value) for value in [{"foo": sf, "bar": sa}, [sf, sa],]]

    def test_malformed_json(self):
        out = """
[
  {
  "text": "["I", "have", "an", "atlas"]",
  "label": ["NONE", "NONE", "NONE", "NONE"]
  },
  {
  "text": ["These", "are", "my", "dogs"],
  "label": ["NONE", "NONE", "NONE", "PLN"]
  },
  {
  "text": ["The", "sheep", "are", "fluffy"],
  "label": ["NONE","PLN","NONE","NONE"]
  },
  {
  "text": ["Billiards", "is", "my", "favourite", "game"],
  "label": ["NONE", "NONE", "NONE", "NONE", "NONE"]
  },
  {
  "text": ["I", "went", "to", "five", "sessions", "today"],
  "label": ["NONE", "NONE", "NONE", "NONE", "PLN", "NONE"]
  }
 ]
 """
        with tempfile.NamedTemporaryFile("w") as f:
            f.write(out)
            f.flush()

            self.assertRaises(RuntimeError, SArray.read_json, f.name)
            self.assertRaises(RuntimeError, XFrame.read_json, f.name)

    def test_nonexistant_json(self):
        self.assertRaises(IOError, SArray.read_json, "/nonexistant.json")
        self.assertRaises(IOError, XFrame.read_json, "/nonexistant.json")

    def test_strange_128_char_corner_case(self):
        json_text = """
{"foo":[{"bar":"Lorem ipsum dolor sit amet, consectetur adipiscing elit. In eget odio velit. Suspendisse potenti. Vivamus a urna feugiat nullam."}]}
"""
        with tempfile.NamedTemporaryFile("w") as f:
            f.write(json_text)
            f.flush()

            df = pandas.read_json(f.name, lines=True)
            sf_actual = XFrame.read_json(f.name, orient="lines")
            sf_expected = XFrame(df)
            _XFrameComparer._assert_xframe_equal(sf_expected, sf_actual)

    def test_true_false_substitutions(self):
        expecteda = [["a", "b", "c"], ["a", "b", "c"]]
        expectedb = [["d", "false", "e", 0, "true", 1, "a"], ["d", "e", "f"]]

        records_json_file = """
[{"a" : ["a", "b", "c"],
  "b" : ["d", "false", "e", false, "true", true, "a"]},
 {"a" : ["a", "b", "c"],
  "b" : ["d", "e", "f"]}]
"""
        lines_json_file = """
{"a" : ["a", "b", "c"], "b" : ["d", "false", "e", false, "true", true, "a"]}
{"a" : ["a", "b", "c"], "b" : ["d", "e", "f"]}
"""

        with tempfile.NamedTemporaryFile("w") as f:
            f.write(records_json_file)
            f.flush()
            records = XFrame.read_json(f.name, orient="records")
        self.assertEqual(list(records["a"]), expecteda)
        self.assertEqual(list(records["b"]), expectedb)

        with tempfile.NamedTemporaryFile("w") as f:
            f.write(lines_json_file)
            f.flush()
            lines = XFrame.read_json(f.name, orient="lines")

        self.assertEqual(list(lines["a"]), expecteda)
        self.assertEqual(list(lines["b"]), expectedb)
