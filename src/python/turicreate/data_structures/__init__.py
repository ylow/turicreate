# -*- coding: utf-8 -*-
# Copyright © 2017 Apple Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-3-clause license that can
# be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
"""
Turi Create offers several data structures for data analysis.  Concise
descriptions of the data structures and their methods are contained in the API
documentation, along with a small number of simple examples.
"""




__all__ = ["sframe", "sarray", "sketch", "serialization"]

from . import sframe
from . import sarray
from . import sketch
from . import serialization

