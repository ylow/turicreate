# -*- coding: utf-8 -*-
# Copyright © 2017 Apple Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-3-clause license that can
# be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
"""
Python byte-code tools expands on the Python dis module.

"""





from .instruction import Instruction

from .disassembler_ import disassembler

from .bytecode_consumer import ByteCodeConsumer, StackedByteCodeConsumer
