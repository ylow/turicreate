# -*- coding: utf-8 -*-
# Copyright © 2020 Apple Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-3-clause license that can
# be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
#
  # noqa
  # noqa
  # noqa
import turicreate as _tc
import pytest

pytestmark = [pytest.mark.minimal]


@pytest.mark.skipif(not _tc._deps.is_minimal_pkg(), reason="skip when testing full pkg")
class TestMinimalPackage(object):
    """ test minimal package for toolkits.
    well, other toolkits are too hard to setup
    """

    def test_audio_classifier(self):
        with pytest.raises(
            ImportError, match=r".*pip install --force-reinstall turicreate==.*"
        ):
            _tc.load_audio("./dummy/audio")
