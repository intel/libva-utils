#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import unittest
from dataParser import *

class TestDataParser(unittest.TestCase):
    def setUp(self):
        # load media trace format
        self.parser = dataParser(8)
        self.parser.loadManifest()

    def test_rawdata1(self):
        data = b'\x01\x00\x00\x00\x01\x00\x00\x00'
        head = {'sys':'Intel-Media', 'id':79, 'op':1}
        output = {'ContextId': 1, 'VAContext': 'ContextDecoder (1)'}

        out = self.parser.parseData(data, head)
        self.assertEqual(out['data'], output)

    def test_rawdata2(self):
        data = b'\x02\x00\x00\x00\x13\x00\x01\x00\x02\x40'
        head = {'sys':'Intel-Media', 'id':1, 'op':1}
        output = {}

        out = self.parser.parseData(data, head)
        self.assertEqual(out['data'], output)

    def test_rawdata3(self):
        data = b'\x02\x00\x03\x00\x12\x01\x03\x00'
        head = {'sys':'Intel-Media', 'id':83, 'op':2}
        output = {'num': 2, 'Id': [196882, 0]}

        out = self.parser.parseData(data, head)
        self.assertEqual(out['data'], output)
