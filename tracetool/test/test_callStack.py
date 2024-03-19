#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import unittest
from callStack import *

class TestCallStack(unittest.TestCase):

    def test_stack(self):
        ctx = callStack()
        self.assertEqual(ctx.current(0, 1), None)
        self.assertEqual(ctx.current(2, 3), None)

        # test normal pid/tid
        first = ctx.get(0, 1)
        self.assertEqual(len(first), 0)
        ctx.push({'pid': 0, 'tid':1, 'id':123, 'name': 'ddi'})
        ctx.push({'pid': 0, 'tid':1, 'id':456, 'name': 'hal'})
        self.assertEqual(ctx.current(0, 1)['id'], 456)
        last = ctx.pop({'pid': 0, 'tid':1, 'id':123, 'name': 'ddi'})
        self.assertEqual(last['id'], 123)
        self.assertEqual(ctx.current(0, 1), None)

        # test string pid/tid
        first = ctx.get('pid', 'tid')
        self.assertEqual(len(first), 0)
        ctx.push({'pid': 'pid', 'tid':'tid', 'id':123, 'name': 'ddi'})
        ctx.push({'pid': 'pid', 'tid':'tid', 'id':456, 'name': 'hal'})
        self.assertEqual(ctx.current('pid', 'tid')['id'], 456)
        self.assertEqual(len(first), 2)
        last = ctx.pop({'pid': 'pid', 'tid':'tid', 'id':123, 'name': 'pop'})
        self.assertEqual(last, None)
        self.assertEqual(len(first), 2)

        # test push and pop from diff thread id
        ctx.push({'pid': 100, 'tid':3, 'id':123, 'name': 'n'})
        ctx.pop({'pid': 100, 'tid':3, 'id':123, 'name': 'n'})
        ctx.push({'pid': 100, 'tid':1, 'id':123, 'name': 'ddi'})
        out = ctx.pop({'pid': 100, 'tid':2, 'id':456, 'name': 'ddi'})
        self.assertEqual(out['name'], 'ddi')
        self.assertEqual(ctx.current(100, 1), None)

    def test_find(self):
        ctx = callStack()
        ctx.push({'sys': 's1', 'pid': 1, 'tid':2, 'id':123, 'name': 'app'})
        ctx.push({'sys': 's1', 'pid': 'pid', 'tid':'tid', 'id':123, 'name': 'ddi'})
        ctx.push({'sys': 's2', 'pid': 'pid', 'tid':'tid', 'id':456, 'name': 'hal'})
        # test find top event with same sys/pid/tid
        s1 = ctx.find({'sys': 's1', 'pid': 1, 'tid':2})
        self.assertEqual(s1['name'], 'app')
        s1p = ctx.find({'sys': 's1', 'pid': 'pid', 'tid':'tid'})
        self.assertEqual(s1p['name'], 'ddi')
        s2 = ctx.find({'sys': 's2', 'pid': 'pid', 'tid':'tid'})
        self.assertEqual(s2['name'], 'hal')
