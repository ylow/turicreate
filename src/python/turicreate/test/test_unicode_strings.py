# -*- coding: utf-8 -*-
# Copyright © 2017 Apple Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-3-clause license that can
# be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.





import six
import unittest
import turicreate as tc

import pytest

pytestmark = [pytest.mark.minimal]


class UnicodeStringTest(unittest.TestCase):
    def test_unicode_column_accessor(self):
        sf = tc.SFrame({"a": list(range(100))})
        self.assertEqual(sf["a"][0], sf["a"][0])

    def test_unicode_unpack_prefix(self):
        sf = tc.SFrame({"a": [{"x": 1}, {"x": 2}, {"x": 3}]})
        sf = sf.unpack("a", "\u00aa")
        for col in sf.column_names():
            if six.PY2:
                # column names come out as str
                self.assertTrue(col.startswith("\u00aa".encode("utf-8")))
            else:
                # column names come out as unicode
                self.assertTrue(col.startswith("\u00aa"))

    def test_unicode_column_construction(self):
        sf = tc.SFrame({"\u00aa": [1, 2, 3]})
        self.assertEqual(sf["\u00aa"][0], 1)

    def test_access_nonexistent_column(self):
        sf = tc.SFrame({"\u00aa": [1, 2, 3], "a": [4, 5, 6]})
        with self.assertRaises(RuntimeError):
            sf["b"]
        with self.assertRaises(RuntimeError):
            sf["\u00ab"]
