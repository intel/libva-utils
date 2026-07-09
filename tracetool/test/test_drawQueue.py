#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import unittest
from drawQueue import drawQueue

class stub:
    def __init__(self):
        self.traceEvents = []
    def AddEvent(self, evt):
        self.traceEvents.append(evt)

class TestDrawQueue(unittest.TestCase):

    def setUp(self):
        self.output = stub()
        self.q = drawQueue(self.output)

    def tearDown(self):
        del self.output
        del self.q

    def test_queue1(self):
        self.q.enter({'ts':0, 'name':1})
        self.q.exit({'ts':3, 'name':1})
        out = [{'ts':0, 'dur':3, 'name':1}]
        self.assertEqual(self.output.traceEvents, out)

    def test_queue2(self):
        self.q.enter({'ts':0, 'name':1})
        self.q.enter({'ts':1, 'name':2})
        self.q.exit({'ts':3, 'name':1})
        self.q.exit({'ts':4, 'name':2})
        out = [{'ts':0, 'dur':3, 'name':1}, {'ts':1, 'dur':2, 'name':2}, {'ts':3, 'dur':1, 'name':2}]
        self.assertEqual(self.output.traceEvents, out)

    def test_queue3(self):
        self.q.enter({'ts':0, 'name':1})
        self.q.enter({'ts':1, 'name':2})
        self.q.enter({'ts':2, 'name':3})
        self.q.enter({'ts':3, 'name':4})
        self.q.exit({'ts':4, 'name':2})
        self.q.exit({'ts':5, 'name':1})
        self.q.exit({'ts':6, 'name':4})
        self.q.exit({'ts':7, 'name':3})
        out = [ {'ts':1, 'dur':3, 'name':2}, {'ts':2, 'dur':2, 'name':3}, {'ts':3, 'dur':1, 'name':4},
                {'ts':0, 'dur':5, 'name':1}, {'ts':4, 'dur':1, 'name':3}, {'ts':4, 'dur':1, 'name':4},
                {'ts':5, 'dur':1, 'name':4}, {'ts':5, 'dur':2, 'name':3}]
        self.assertEqual(self.output.traceEvents, out)

    def test_queue3(self):
        self.q.enter({'ts':0, 'name':1})
        self.q.enter({'ts':1, 'name':2})
        self.q.enter({'ts':2, 'name':3})
        self.q.exit({'ts':3, 'name':2})
        self.q.exit({'ts':3, 'name':1})
        self.q.exit({'ts':4, 'name':3})
        out = [ {'ts':1, 'dur':2, 'name':2}, {'ts':2, 'dur':1, 'name':3}, {'ts':0, 'dur':3, 'name':1},
                {'ts':3, 'dur':1, 'name':3}]
        self.assertEqual(self.output.traceEvents, out)
