# -*- coding: utf-8 -*-
# Copyright © 2017 Apple Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-3-clause license that can
# be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
"""
@package turicreate
...
Turi Create is a machine learning platform that enables data scientists and app
developers to easily create intelligent applications at scale.
"""




__version__ = "{{VERSION_STRING}}"
from turicreate.version_info import __version__

from turicreate.data_structures.sarray import SArray
from turicreate.data_structures.sframe import SFrame
from turicreate.data_structures.sketch import Sketch
from .data_structures.sarray_builder import SArrayBuilder
from .data_structures.sframe_builder import SFrameBuilder


import turicreate.aggregate

# internal util
from turicreate._connect.main import launch as _launch

## bring load functions to the top level
from turicreate.data_structures.sframe import load_sframe
from turicreate.data_structures.sarray import load_sarray

################### Extension Importing ########################
import turicreate.extensions

# rewrite the extensions module
class _extensions_wrapper(object):
    def __init__(self, wrapped):
        self._wrapped = wrapped
        self.__doc__ = wrapped.__doc__

    def __getattr__(self, name):
        try:
            return getattr(self._wrapped, name)
        except:
            pass
        turicreate._connect.main.get_unity()
        return getattr(self._wrapped, name)


import sys as _sys

_sys.modules["turicreate.extensions"] = _extensions_wrapper(
    _sys.modules["turicreate.extensions"]
)
# rewrite the import
extensions = _sys.modules["turicreate.extensions"]

from .visualization import plot, show

_launch()
