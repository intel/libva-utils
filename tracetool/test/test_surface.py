#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import unittest
from unittest import mock
from callStack import *
from modules.surface import *

class core:
    def __init__(self):
        self.ctx = {'Stack':callStack()}
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
    def __del__(self):
        pass

class TestSurface(unittest.TestCase):
    def setUp(self):
        self.core = core()
        self.surface = traceHandler(self.core)
        self.surface.setupOutput = mock.Mock(return_value=0)
        self.surface.close = mock.Mock(return_value=0)

    def tearDown(self):
        del self.surface
        del self.core

    def test_syncAttr(self):
        src = {'Name':1}
        db = {'Format':0, 'Name':2}
        syncAttr(src, db, 'Name')
        self.assertEqual(db['Name'], 1)
        syncAttr(src, db, 'Format')
        self.assertEqual(src['Format'], 0)

    def test_SurfaceTracking(self):
        track = self.surface
        internal = {'pid':0, 'ts':1, 'handle':2, 'Format':3}
        track.Allocate(internal)
        self.assertEqual(track.allocation['02']['Format'], 3)
        self.assertEqual(track.surfaceTrack[0]['operate'], 'Allocate')

        globalA = {'pid':1, 'ts':1, 'handle':2, 'Format':4, 'primary':5}
        track.Allocate(globalA)
        self.assertEqual(track.allocation['12']['Format'], 4)
        self.assertEqual(track.surfaceTrack[1]['operate'], 'Allocate')

        track.Reference({'pid':1, 'handle':2})
        self.assertEqual(track.surfaceTrack[2]['Format'], 4)
        
        #alloc = track.fetch({'pid':1, 'handle':2})
        #self.assertEqual(alloc['12']['Format'], 4)

        boVmBindEvent = {'sys': 'i915', 'data': {'flags': 2048, 'obj': 18446637090493330688, 'offset': 2097152, 'size': 2097152, 'vm': 18446637093275460608}, 'name': 'i915_vma_bind', 'op': 0, 'pid': 'sample_multi_tr', 'tid': 9419, 'ts': 24794}
        track.boVmBind(boVmBindEvent)

        mediaAllocEvent = {'sys': 'Intel-Media', 'data': {'Format':'L8 (50)', 'GpuFlags':'0x400000000', 'Height':1, 'InfoFlags':'0xc0300', 'Name':'', 'Pitch':32768, 'Reserve':'0x0', 'Size':65536, 'Surface Handle':'0x40000d80', 'TileType':3, 'WarFlags':'0x0', 'Width': 32768}, 'name':'AllocateRessource', 'op':0, 'pid':3820, 'tid':6036, 'ts':8958621}
        track.mediaAlloc(mediaAllocEvent)

        mediaFreeEvent = {'sys':'Intel-Media', 'data':{'Surface Handle':'0x40001740'}, 'name':'FreeResource', 'op':0, 'pid':3820, 'tid':6036, 'ts':8985446}
        track.mediaFree(mediaFreeEvent)

        batchSubmitEvent = {'sys': 'Intel-Media', 'data': {'Bo Handle': '0x33', 'Write Flag': 1}, 'name': 'Mos_Batch_Submit', 'op': 0, 'pid': 'sample_multi_tr', 'tid': 10308, 'ts': 473462}
        track.batchSubmit(batchSubmitEvent)

        resourceRefEvent = {'sys': 'Intel-Media', 'data': {'Access': 'NA (0)', 'Format': 'L8 (50)', 'GpuFlags': '0x400000000', 'Height': 1, 'InfoFlags': '0xc0300', 'Pitch': 149632, 'Size': 196608, 'Surface Handle': '0x40001640', 'TileType': 3, 'WarFlags':'0x0', 'Width':149632}, 'name': 'RegisterResource', 'op': 0, 'pid': 3820, 'tid': 6036, 'ts': 8975228}
        track.resourceRef(resourceRefEvent)

        boAllocEvent = {'sys': 'i915', 'data': {'obj': 18446637090493330688, 'size': 65536}, 'name': 'i915_gem_object_create', 'op': 0, 'pid': 'sample_multi_tr', 'tid': 9419, 'ts': 24781}
        track.boAlloc(boAllocEvent)

    def test_vaSurface(self):
        track = self.surface
        vaSurfStart = {'sys': 'Intel-Media', 'data': {'Format': 'YUV420 (0x1)', 'Height': 720, 'Width': 1280}, 'name': 'VA_CreateSurface', 'op': 1, 'pid': 'lucas', 'tid': 6903, 'ts': 549908}
        track.vaSurfaceAlloc(vaSurfStart)
        vaSurfEnd = {'sys': 'Intel-Media', 'data': {'Id': [1], 'num': 1}, 'name': 'VA_CreateSurface', 'op': 2, 'pid': 'lucas', 'tid': 6903, 'ts': 549923}
        track.vaSurfaceAlloc(vaSurfEnd)
        vaSurfFree = {'sys': 'Intel-Media', 'data': {'Id': [1], 'num': 1}, 'name': 'VA_DestroySurface', 'op': 1, 'pid': 'lucas', 'tid': 6903, 'ts': 566284}
        track.vaSurfaceFree(vaSurfFree)

    def test_registerResource(self):
        track = self.surface
        ctx = self.core.getContext()['Stack'].get(10080, 9068)
        call = [{'sys':'media', 'name': 'eDDI_Codec_View', 'lastRef':{'Access': 'Target'}, 'lastSurf': '0xc000c880'},
        {'sys':'media', 'name': 'Codec_DecodeDDI', 'group':{'Codec_DecodeDDI':{'DecodeMode':'HEVC (1)'}}},
        {'sys':'media', 'name': 'Codec_Decode', 'data': {'Frame': 9538, 'Standard': 'HEVC (64)'}}]
        ctx.extend(call)
        reg1 = {'data': {'DwOffset': 38, 'GpuAddr': '0x10f590000', 'Offset': '0x0', 'HwCommand': 'MfxPipeBufAddr (18)'}, 'name': 'RegisterResource', 'op': 3, 'pid': 10080, 'tid': 9068, 'ts': 978563, 'sys':'media'}
        track.resourceHwRef(reg1)
        self.assertEqual(ctx[0]['lastRef']['Access'], 'Target RefPic')

    def test_registerResource2(self):
        track = self.surface
        ctx = self.core.getContext()['Stack'].get(10080, 9068)
        call = [{'sys':'media', 'name': 'eDDI_Codec_View', 'lastRef':{'Access': 'Target'}, 'lastSurf': '0xc000c880'},
            {'sys':'media', 'name': 'Codec_DecodeDDI', 'group':{'Codec_DecodeDDI':{'DecodeMode':'H264 (1)'}}},
            {'sys':'media', 'name': 'Codec_Decode', 'data': {'Frame': 9538, 'Standard': 'AVC (3)'}}]
        ctx.extend(call)
        reg1 = {'data': {'DwOffset': 19, 'GpuAddr': '0x10f590000', 'Offset': '0x0', 'HwCommand': 'MfxPipeBufAddr (18)'}, 'name': 'RegisterResource', 'op': 3, 'pid': 10080, 'tid': 9068, 'ts': 978563, 'sys':'media'}
        track.resourceHwRef(reg1)
        self.assertEqual(ctx[0]['lastRef']['Access'], 'Target RefPic')
