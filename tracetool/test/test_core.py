#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import unittest
from core import *

class handler:
    def __init__(self):
        self.h1 = 0
        self.h2 = 0
        self.h3 = 0
        self.com = 0

    def hander1(self, evt):
        self.h1 = 1

    def hander2(self, evt):
        self.h2 = 1

    def hander3(self, evt):
        self.h3 = 1
        return -1

    def common(self, evt):
        self.com = 1

class reader:
    def __init__(self):
        self.evt = []

    def open(self, input, options):
        self.options = options
        if input.endswith('.dat'):
            return 0
        else:
            return -1

    def setParser(self, parsers):
        self.parsers = parsers

    def syncSource(self, src):
        self.aux = src
        return 0

    def process(self, filter, callback):
        for e in self.evt:
            callback(e)

    def __del__(self):
        pass

class reader2:
    def __init__(self):
        self.syncSts = 0

    def open(self, input, options):
        self.options = options
        if input.endswith('.bin'):
            return 0
        else:
            return -1

    def setParser(self, parsers):
        self.parsers = parsers

    def syncSource(self, src):
        return -1

    def process(self, filter, callback):
        self.callback = callback

    def __del__(self):
        pass

class TestCore(unittest.TestCase):

    def setUp(self):
        self.instance = core()
        # remove real readers/handlers
        self.instance.readers = []
        self.instance.instances = []
        self.instance.handlers = {}
        self.instance.parsers = {}

    def tearDown(self):
        del self.instance

    def test_init(self):
        ctx = self.instance.getContext()
        self.assertTrue(isinstance(ctx, dict))

    def test_reader(self):
        options = {'raw':None}
        self.instance.readers = [reader]
        # check error path
        sts = self.instance.open('test', options)
        self.assertEqual(sts, -1)
        # check sucess path
        sts = self.instance.open('test.dat', options)
        self.assertEqual(sts, 0)

    def test_reader2(self):
        options = {'raw':None}
        self.instance.readers = [reader, reader2]
        sts = self.instance.open('test.bin', options)
        self.assertEqual(sts, 0)
        # check option is set in source
        self.assertEqual(self.instance.source.options, options)

    def test_reader3(self):
        options = {'raw':None}
        files = ['test.bin', 'test.dat']
        self.instance.readers = [reader]
        sts = self.instance.open(files, options)
        self.assertEqual(sts, -1)
        # check success path
        self.instance.readers.append(reader2)
        sts = self.instance.open(files, options)
        self.assertEqual(sts, 0)
        # check source and name, should select correct source within multi files
        self.assertNotEqual(self.instance.source, None)
        self.assertEqual(self.instance.sharedCtx['sourceFile'], 'test.dat')

    def test_callback(self):
        h1 = handler()
        h2 = handler()

        self.instance.regHandler('test1', 'e1', h1.hander1)
        self.instance.regHandler('test1', 'e1', h2.hander2)
        self.instance.regHandler('test1', None, h1.common)
        self.instance.regHandler('test3', None, h1.hander3)
        self.instance.regHandler('test3', 'e1', h2.hander3)

        sts = self.instance.regParser(b'ETMI', h2.hander3)
        self.assertEqual(sts, 0)
        sts = self.instance.regParser(b'ETMI', h1.hander3)
        self.assertEqual(sts, -1)

        # check filter
        self.instance.sharedCtx['Opt'] = {}
        self.assertTrue(self.instance.filter({'sys':'test1', 'name': 'e1'}))
        self.assertTrue(self.instance.filter({'sys':'test1', 'name':'str'}))
        self.assertFalse(self.instance.filter({'sys':'test2', 'name': 'str'}))
        self.assertTrue(self.instance.filter({'sys':'test3', 'name': 'str'}))

        # only handle3 called
        evt = {'sys':'test3', 'name':'dw'}
        self.instance.callback(evt)
        self.assertEqual(h1.h1, 0)
        self.assertEqual(h1.h2, 0)
        self.assertEqual(h1.h3, 1)
        self.assertEqual(h1.com, 0)

        evt = {'sys':'test1', 'name':'e1'}
        self.instance.callback(evt)
        # handle1 should have hander1 handler called
        self.assertEqual(h1.h1, 1)
        self.assertEqual(h1.h2, 0)
        self.assertEqual(h1.h3, 1)
        self.assertEqual(h1.com, 1)
        # handle2 should have handle2 called
        self.assertEqual(h2.h1, 0)
        self.assertEqual(h2.h2, 1)
        self.assertEqual(h2.h3, 0)

    def test_process(self):
        h1 = handler()

        self.instance.regHandler('test1', 'e1', h1.hander1)
        self.instance.readers = [reader]
        self.instance.open('test.dat', {})
        # inject event and process it
        self.instance.source.evt.append({'sys':'test1', 'name': 'e1'})
        self.instance.process()
        # check handler is called
        self.assertEqual(h1.h1, 1)
        # check output files
        ctx = self.instance.getContext()
        self.assertTrue(isinstance(ctx, dict))
        self.assertTrue('UI' in ctx)
        self.assertTrue('Stat' in ctx)
        self.assertTrue('Stack' in ctx)
        self.assertTrue('Opt' in ctx)
