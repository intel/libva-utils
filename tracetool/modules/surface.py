#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import os
from util import GetCompress

# sync surface attributes, update record if mismatch, sync to output if not set
def syncAttr(out, record, key):
    if key in out and (key not in record or record[key] != out[key]):
        record[key] = out[key]
    elif key in record:
        out[key] = record[key]

class traceHandler:

    def __init__(self, core):
        self.sharedCtx = core.getContext()
        self.fp = None
        self.surfaceTrack = []
        self.allocation = {}
        self.globalAllocate = {}

        self.vaIdMap = {}

        core.regHandler('Intel-Media', 'RegisterResource', self.resourceRef)
        core.regHandler('Intel-Media', 'AllocateResource', self.mediaAlloc)
        core.regHandler('Intel-Media', 'FreeResource', self.mediaFree)
        core.regHandler('Intel-Media', 'Mos_Batch_Submit', self.batchSubmit)
        core.regHandler('Intel-Media', 'VA_CreateSurface', self.vaSurfaceAlloc)
        core.regHandler('Intel-Media', 'VA_DestroySurface', self.vaSurfaceFree)
        core.regHandler('i915', 'i915_gem_object_create', self.boAlloc)
        core.regHandler('i915', 'i915_vma_bind', self.boVmBind)

        self.sharedCtx['Surface'] = self

    def setupOutput(self):
        if self.fp == None:
            self.fp = open(self.sharedCtx['Output'] + '_surface.csv', 'w')

    def Allocate(self, info):
        key = str(info['pid']) + str(info['handle'])
        # find by primary handle first
        if 'primary' in info:
            if info['primary'] not in self.globalAllocate:
                self.globalAllocate[info['primary']] = {}
            if key not in self.allocation:
                self.allocation[key] = self.globalAllocate[info['primary']]
        if key not in self.allocation:
            self.allocation[key] = {}
        record = self.allocation[key]
        out = info.copy()
        if 'operate' not in out:
            out['operate'] = 'Allocate'
        self.surfaceTrack.append(out)
        del info['ts']
        del info['pid']
        del info['handle']
        if 'tid' in info:
            del info['tid']
        record.update(info)
        self.setupOutput()

    def Destroy(self, info):
        key = str(info['pid']) + str(info['handle'])
        if key in self.allocation:
            del self.allocation[key]
        if 'primary' in info and info['primary'] in self.globalAllocate:
            del self.globalAllocate[info['primary']]
        info['operate'] = 'Destroy'
        self.surfaceTrack.append(info)

    def Reference(self, ref):
        key = str(ref['pid']) + str(ref['handle'])
        if key in self.allocation:
            record = self.allocation[key]
            syncAttr(ref, record, 'primary')
            syncAttr(ref, record, 'Name')
            syncAttr(ref, record, 'Format')
            syncAttr(ref, record, 'Width')
            syncAttr(ref, record, 'Height')
            syncAttr(ref, record, 'Pitch')
            syncAttr(ref, record, 'Address')
            syncAttr(ref, record, 'Size')
            syncAttr(ref, record, 'Compress')
            syncAttr(ref, record, 'Tile')
            syncAttr(ref, record, 'Protected')
            syncAttr(ref, record, 'CpTag')
            syncAttr(ref, record, 'Local')
        else:
            # unknown surfaces
            ref['primary'] = 'N/A'
        self.surfaceTrack.append(ref)
        self.setupOutput()

    def recordMark(self, marker):
        out = {'ts': marker['ts'], 'pid':'N/A', 'tid':'N/A', 'operate':marker['name'], 'handle': marker['msg'], 'primary': 'N/A'}
        self.surfaceTrack.append(out)

    def fetch(self, info):
        if 'handle' in info:
            key = str(info['pid']) + str(info['handle'])
            if key in self.allocation:
                return self.allocation[key]
        if 'primary' in info:
            key = info['primary']
            if key in self.globalAllocate:
                return self.globalAllocate[key]

    def updateTrack(self, record):
        if 'handle' in record:
            for i in self.surfaceTrack:
                if i['pid'] == record['pid'] and i['handle'] == record['handle']:
                    i.update(record)
        if 'primary' in record:
            for i in self.surfaceTrack:
                if 'primary' in i and i['primary'] == record['primary']:
                    i.update(record)

    def vaSurfaceAlloc(self, evt):
        key = str(evt['pid'])
        if evt['op'] == 1:
            if key not in self.vaIdMap:
                self.vaIdMap[key] = {'bo':[], 'idMap':{'Surf':{}, 'Buf':{}, 'Img':{}}}
            self.vaIdMap[key]['bo'+str(evt['tid'])] = []
            self.vaIdMap[key]['vaAlloc'+str(evt['tid'])] = []
            return
        if evt['op'] == 2:
            if key not in self.vaIdMap:
                return
            idMap = self.vaIdMap[key]['idMap']
            vaId = self.vaIdMap[key]['vaAlloc'+str(evt['tid'])]
            data = evt['data']
            i = 0
            if len(data['Id']) == len(vaId):
                for id in data['Id']:
                    idMap['Surf'][id] = vaId[i]
                    i += 1
            self.vaIdMap[key]['bo'+str(evt['tid'])] = []
            self.vaIdMap[key]['vaAlloc'+str(evt['tid'])] = []
            return
        if evt['op'] != 0:
            return
        key = str(evt['pid'])
        data = evt['data']
        alloc = {'ts':evt['ts'], 'pid':evt['pid'], 'tid':evt['tid'], 'handle':data['Handle']}
        alloc['Compress'] = GetCompress(data['gmmFlags_Info'])
        # find primary handle from i915
        if key in self.vaIdMap:
            self.vaIdMap[key]['vaAlloc'+str(evt['tid'])].append(data['Handle'])
            for i in self.vaIdMap[key]['bo'+str(evt['tid'])]:
                if i['size'] >= data['Size']:
                    alloc['primary'] = i['primary']
                    break
            self.vaIdMap[key]['bo'+str(evt['tid'])] = []
        for k in ['Format', 'Width', 'Height', 'Pitch', 'Size', 'Tile', 'CpTag']:
            alloc[k] = data[k]
        alloc['Name'] = 'VASurface'+str(data['Handle'])
        self.Allocate(alloc)

    def vaSurfaceFree(self, evt):
        if evt['op'] != 1:
            return
        key = str(evt['pid'])
        data = evt['data']
        if key not in self.vaIdMap:
            return
        idMap = self.vaIdMap[key]['idMap']
        for id in data['Id']:
            data = {'ts':evt['ts'], 'pid': evt['pid'], 'tid':evt['tid']}
            if id in idMap['Surf']:
                data['handle'] = idMap['Surf'][id]
            else:
                data['handle'] = 'NA'
            self.Destroy(data)

    def boVmBind(self, evt):
        data = evt['data']
        record = self.fetch({'primary':hex(data['obj'])})
        if record is not None and 'Address' not in record:
            record['Address'] = hex(data['vm'] + data['offset'])
            self.updateTrack(record)

    def mediaAlloc(self, event):
        if event['op'] == 1:
            return self.vaSurfaceAlloc(event)
        if event['op'] != 0:
            return

        data = event['data']
        alloc = {'ts':event['ts'], 'pid':event['pid'], 'tid':event['tid'], 'handle':data['Surface Handle']}
        alloc['Compress'] = GetCompress(data['InfoFlags'])
        for k in ['Format', 'Width', 'Height', 'Pitch', 'Size']:
            alloc[k] = data[k]
        alloc['Tile'] = data['TileType']
        if 'Name' in data:
            alloc['Name'] = data['Name']
        # find primary handle from i915
        key = str(event['pid'])
        if key in self.vaIdMap:
            for i in self.vaIdMap[key]['bo'+str(event['tid'])]:
                if i['size'] >= data['Size']:
                    alloc['primary'] = i['primary']
                    break
            self.vaIdMap[key]['bo'+str(event['tid'])] = []
        self.Allocate(alloc)

    def mediaFree(self, event):
        if event['op'] != 0:
            return
        data = {'ts':event['ts'], 'pid': event['pid'], 'tid':event['tid'], 'handle': event['data']['Surface Handle']}
        self.Destroy(data)

    def batchSubmit(self, evt):
        if evt['op'] != 0:
            return
        data = evt['data']
        info = {'pid': evt['pid'], 'handle':data['Bo Handle']}
        record = self.fetch(info)
        if record != None:
            data['Name'] = record['Name']
        else:
            data['Name'] = 'Media'
        # find component name from context
        stack = self.sharedCtx['Stack']
        media = stack.find(evt)
        if media == None:
            media = {'name':evt['name'], 'ts':evt['ts']}
        info['ts'] = media['ts']
        info['tid'] = evt['tid']
        info['operate'] = media['name']
        if data['Write Flag'] != 0:
            info['Access'] = 'Write'
        else:
            info['Access'] = 'Read'
        self.Reference(info)

    def resourceHwRef(self, evt):
        # find component name from context
        stack = self.sharedCtx['Stack']
        media = stack.find(evt)
        if media == None:
            return -1 # skip common process
        if 'lastSurf' not in media or media['lastSurf'] == None:
            return -1 # skip common process
        if 'lastRef' not in media:
            return -1 # skip common process
        handle = media['lastSurf']
        media['lastSurf'] = None
        ref = media['lastRef']
        ref['Address'] = evt['data']['GpuAddr']
        ref['Offset'] = evt['data']['Offset']
        offset = evt['data']['DwOffset']
        # TODO: put HW CMD and its resource name in centralized class
        if 'MfxPipeBufAddr' in evt['data']['HwCommand']:
            offsetMap = {}
            for ctx in stack.get(evt['pid'], evt['tid']):
                if ctx['name'] == 'Codec_DecodeDDI':
                    if 'H264' in ctx['group']['Codec_DecodeDDI']['DecodeMode']:
                        # MFX Pipe Buffer addr cmd
                        offsetMap[1] = 'PreDeblock'
                        offsetMap[4] = 'PostDeblock'
                        offsetMap[10] = 'StreamOut'
                        offsetMap[13] = 'IntraRowScratch'
                        offsetMap[16] = 'DeblockingFilter'
                        for i in range(19, 51):
                            offsetMap[i] = 'RefPic'
                        offsetMap[55] = 'MacroblockIldbStreamOut'
                        offsetMap[58] = 'MacroblockIldbStreamOut2'
                        offsetMap[62] = '4xDsSurface'
                        offsetMap[65] = 'SliceSizeStreamOut'
                    else:
                        # HCP pipe buffer addr cmd
                        offsetMap[1] = 'PreDeblock'
                        offsetMap[4] = 'DeblockingFilter'
                        offsetMap[7] = 'DeblockingFilterTile'
                        offsetMap[10] = 'DeblockingFilterColumn'
                        offsetMap[13] = 'MetadataLine'
                        offsetMap[16] = 'MetadataTileLine'
                        offsetMap[19] = 'MetadataTileColumn'
                        offsetMap[22] = 'SaoLine'
                        offsetMap[25] = 'SaoTileLine'
                        offsetMap[28] = 'SaoTileColumn'
                        offsetMap[31] = 'CurMvTemp'
                        for i in range(37, 53):
                            offsetMap[i] = 'RefPic'
                        offsetMap[54] = 'Raw'
                        offsetMap[57] = 'StreamOut'
                        offsetMap[60] = 'LcuBaseAddress'
                        offsetMap[63] = 'LcuILDBStreamOut'
                        for i in range(66, 82):
                            offsetMap[i] = 'CollocatedMotionVector'
                        offsetMap[83] = 'Vp9Prob'
                        offsetMap[86] = 'Vp9SegmentId'
                        offsetMap[89] = 'HvdLine'
                        offsetMap[92] = 'HvdTile'
                        offsetMap[95] = 'SaoRow'
                        offsetMap[98] = 'FrameStat'
                        offsetMap[101] = 'SseSrcPixel'
                        offsetMap[104] = 'SliceState'
                        offsetMap[107] = 'CABACSyntax'
                        offsetMap[110] = 'MvUpRightCol'
                        offsetMap[113] = 'IntraPredUpRightCol'
                        offsetMap[116] = 'IntraPredLeftReconCol'
                        offsetMap[119] = 'CABACSyntax'
            if offset in offsetMap:
                if 'Access' not in ref:
                    ref['Access'] = offsetMap[offset]
                else:
                    ref['Access'] += ' ' + offsetMap[offset]
        return -1

    def resourceRef(self, evt):
        if evt['op'] == 3:
            return self.resourceHwRef(evt)
        if evt['op'] != 0:
            return
        stack = self.sharedCtx['Stack']
        handle = evt['data']['Surface Handle']

        # find component name from context
        media = stack.find(evt)
        if media == None:
            media = {'name':evt['name'], 'ts':evt['ts']}
        # save surface handle for RegisterResource2 locat surface handle, thus it can find detail surface info
        media['lastSurf'] = handle
        data = {'ts':media['ts'], 'pid':evt['pid'], 'tid':evt['tid'], 'operate': media['name'], 'handle':handle}
        # load attribute from context if have
        if 'render' in media and handle in media['render']:
            for k in media['render'][handle]:
                data[k] = media['render'][handle][k]
        # set attribute in current event
        for k in ['Format', 'Width', 'Height', 'Pitch', 'Size']:
            data[k] = evt['data'][k]
        data['Compress'] = GetCompress(evt['data']['InfoFlags'])
        data['Tile'] = evt['data']['TileType']
        self.Reference(data)
        #save a reference to update info later
        media['lastRef'] = data
        # return -1 to skip common media handler
        return -1

    def boAlloc(self, evt):
        k1 = str(evt['pid'])
        k2 = 'bo'+str(evt['tid'])
        if k1 in self.vaIdMap and k2 in self.vaIdMap[k1]:
            self.vaIdMap[k1][k2].append({'primary': hex(evt['data']['obj']), 'size':evt['data']['size']})

    def close(self):
        if len(self.surfaceTrack) == 0:
            return
        file = self.sharedCtx['Output']
        # remove duplicated record
        setflag = set() # use set to test whether record is duplicated
        record = []
        for d in self.surfaceTrack:
            t = tuple(d.items())
            if t not in setflag:
                setflag.add(t)
                record.append(d)
        # build up render reference
        render = {}
        for i in record:
            if 'Access' in i and 'primary' in i and ('Target' in i['Access'] or 'Write' in i['Access']):
                render[i['primary']] = i
            if 'Destroy' in i['operate'] and 'primary' in i and i['primary'] in render:
                del render[i['primary']]
            if 'PAVPTeardown' == i['operate']:
                # need clean up all render history when teardown happens
                render.clear()
            # only check on protected surface
            if 'Present' in i['operate'] and 'CpTag' in i and i['CpTag'] != '0x0':
                if i['primary'] not in render:
                    i['Health'] = 'Corruption'

        fields = ['ts','pid','tid','operate', 'handle', 'primary', 'Name', 'Access', 'Health', 'Format', 'Width', 'Height', 'Address', 'Offset', 'Compress', 'Protected', 'CpTag', 'Local']
        line = ''
        for f in fields:
            line += '{0},'.format(f)
        line += '\n'
        self.fp.write(line)
        for i in record:
            line = ''
            for f in fields:
                if f in i:
                    line += '{0},'.format(i[f])
                else:
                    line += ','
            line += '\n'
            self.fp.write(line)
        self.fp.close();

    def __del__(self):
        self.close()
