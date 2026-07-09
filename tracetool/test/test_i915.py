#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import unittest
from modules.i915 import *

class stat:
    def __init__(self):
        self.ctx = {}
        self.stat = []

    def enter(self, elem):
        if elem['id'] in self.ctx:
            self.ctx[elem['id']].append(elem)
        else:
            self.ctx[elem['id']] = [elem]

    def exit(self, elem):
        if elem['id'] in self.ctx and len(self.ctx[elem['id']]) > 0:
            start = self.ctx[elem['id']].pop()
            del start['id']
            start['latency'] = elem['ts'] - start['ts']
            self.stat.append(start)

class ui:
    def __init__(self):
        self.evt = []
        self.meta = []

    def AddMetaEvent(self, evt):
        self.meta.append(evt)

    def AddEvent(self, evt):
        self.evt.append(evt)

class core:
    def __init__(self):
        self.ctx = {'UI':ui(), 'Stat':stat()}
        self.handlers = {}

    def getContext(self):
        return self.ctx

    def regHandler(self, sys, name, handler):
        self.handlers[name] = handler

    def process(self, evt):
        for e in evt:
            self.handlers[e['name']](e)

class Testi915(unittest.TestCase):
    def setUp(self):
        self.core = core()
        self.ui = self.core.getContext()['UI']
        self.stat = self.core.getContext()['Stat']
        self.i915 = traceHandler(self.core)

    def tearDown(self):
        self.ui = None
        self.stat= None
        del self.core

    def test_gpusubmit(self):
        add = [{'name': 'i915_request_add', 'pid':1, 'tid':1, 'ts':1, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':0}},
               {'name': 'i915_request_add', 'pid':1, 'tid':1, 'ts':2, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':1}}]
        self.core.process(add)
        self.assertEqual(len(self.ui.meta), 2)

        add = [{'name': 'i915_request_in', 'pid':9, 'tid':1, 'ts':3, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':0}},
               {'name': 'i915_request_in', 'pid':9, 'tid':1, 'ts':4, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':1}}]
        self.core.process(add)

        add = [{'name': 'i915_request_out', 'pid':9, 'tid':1, 'ts':5, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':0}},
               {'name': 'i915_request_out', 'pid':9, 'tid':1, 'ts':6, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':1}}]
        self.core.process(add)

        add = [{'name': 'i915_request_wait_begin', 'pid':1, 'tid':1, 'ts':7, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':0}},
               {'name': 'i915_request_wait_end',   'pid':1, 'tid':1, 'ts':8, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':0}}]
        self.core.process(add)

        add = [{'name': 'i915_request_retire', 'pid':1, 'tid':1, 'ts':9, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':0}},
               {'name': 'i915_request_retire', 'pid':1, 'tid':1, 'ts':9, 'data':{'ctx':12, 'class':0, 'dev':1234,'instance':0, 'seqno':1}}]
        self.core.process(add)
        self.assertEqual(len(self.ui.meta), 2)

        stat = self.stat.stat
        self.assertEqual(len(stat), 2)
        self.assertEqual(stat[0]['latency'], 4)
        self.assertEqual(stat[1]['latency'], 4)

        evt = self.ui.evt
        # minimal check on UI, events may change in future
        self.assertGreater(len(evt), 5) # at least 2*(sw activty/hw activity/flow) + wait

    def test_counter(self):
        add = [{'name': 'print', 'pid':1, 'tid':1, 'ts':1, 'data':{'buf':'counters,process=1,pid=2,bo=1,gpu=2'}},
               {'name': 'print', 'pid':1, 'tid':1, 'ts':2, 'data':{'buf':'counters,local=1,system=2'}}]
        self.core.process(add)
        evt = self.ui.evt
        self.assertEqual(len(evt), 2)
        self.assertEqual(evt[0]['name'], 'per-process counters')
        self.assertEqual(evt[0]['args']['bo'], 1)
        self.assertEqual(evt[1]['name'], 'global counters')
        self.assertEqual(evt[1]['args']['system'], 2)

