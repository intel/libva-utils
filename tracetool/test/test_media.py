#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import unittest
from callStack import *
from modules.iHD import *

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

    def AddMetaData(self, meta):
        self.meta.append(meta)

class surface:
    def fetch(self, info):
        return None

class core:
    def __init__(self):
        self.ctx = {'UI':ui(), 'Stat':stat(), 'Stack':callStack(), 'Surface':surface()}
        self.handlers = {}

    def getContext(self):
        return self.ctx

    def regHandler(self, sys, name, handler):
        if name == None:
            name = 'allEvent'
        if name in self.handlers:
            self.handlers[name].append(handler)
        else:
            self.handlers[name] = [handler]

    def regParser(self, id, handler):
        pass

    def process(self, evt):
        hnd = self.handlers
        flag = 0
        if evt['name'] in hnd:
            for h in hnd[evt['name']]:
                sts = h(evt)
                if sts != None and sts < 0:
                    flag = 1
        # call all event handler at last step, skip if any handler has returned -1
        if 'allEvent' in hnd and flag == 0:
            for h in hnd['allEvent']:
                h(evt)

class TestMedia(unittest.TestCase):
    def write(self, line):
        self.line = line

    def setUp(self):
        self.core = core()
        self.ui = self.core.getContext()['UI']
        self.stat = self.core.getContext()['Stat']
        self.iHD = traceHandler(self.core)
        self.iHD.writeOutput = self.write

    def tearDown(self):
        self.ui = None
        self.stat = None
        del self.iHD
        del self.core

    def test_common(self):
        add = [{'sys': 'm', 'name': 'test', 'pid':1, 'tid':1, 'ts':1, 'op': 1, 'data':{'ctx':12}},
               {'sys': 'm', 'name': 'test', 'pid':1, 'tid':1, 'ts':2, 'op': 0, 'data':{'seq':1}},
               {'sys': 'm', 'name': 'test', 'pid':1, 'tid':1, 'ts':3, 'op': 2, 'data':{}}]
        for e in add:
            self.core.process(e)
        self.assertEqual(len(self.ui.evt), 2)
        self.assertEqual(self.ui.evt[1]['args']['test'], {'seq':1})
        self.assertEqual(len(self.stat.stat), 1)

    def test_resource(self):
        self.core.process({'sys': 'media', 'name': 'eDDI_Codec_View', 'pid':1, 'tid':1, 'ts':1, 'op': 1, 'data':{'hAllocation':11, 'Width':2, 'CpTag':0, 'hMediaDevice':0}})
        self.core.process({'sys': 'media', 'name': 'RegisterResource', 'pid':1, 'tid':1, 'ts':1, 'op': 0, 'data':{'Surface Handle':122, 'Width':2}})
        self.core.process({'sys': 'media', 'name': 'RegisterResource', 'pid':1, 'tid':1, 'ts':1, 'op': 3, 'data':{'HwCommand':'MiStoreRegisterMem  (13)', 'Offset':'0x18', 'Size': '0x0', 'GpuAddr':'0xFFFF80040231E018'}})
        stack = self.core.ctx['Stack']
        media = stack.current(1, 1)
        resource = media['resource']
        self.assertEqual(resource[0]['Offset'], '0x18')
        self.assertEqual(resource[0]['handle'], 122)
        self.core.process({'sys': 'media', 'name': 'eDDI_Codec_View', 'pid':1, 'tid':1, 'ts':2, 'op': 2, 'data':{}})
        self.assertEqual(len(self.ui.evt), 2)
        self.assertEqual(len(self.stat.stat), 1)
        self.assertFalse('render' in self.ui.evt[0])

    def test_RTLog(self):
        self.core.process({'ts':1, 'pid':2, 'tid':3, 'name': 'MediaRuntimeLog', 'data':{'LogId':'MT_ERR_MEM_ALLOC (1)', 'LogLevel':'Normal (1)', 'Id1':'MT_ERROR_CODE (1)', 'Value1': '0x3'}})
        self.assertTrue('Common' in self.line)
        self.assertTrue('ERROR_CODE' in self.line)
