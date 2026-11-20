#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import re
from util import *

class traceHandler:
    phaseMap = {1:'B', 2:'E'}
    compMap = ['Common', 'CP', 'VP', 'Decode', 'Encode']

    def __init__(self, core):
        self.sharedCtx = core.getContext()
        self.fp = None

        self.DataDump = {}

        core.regParser(b'ETMI', self.mediaHeaderParse)
        core.regHandler('Intel-Media', 'RegisterResource', self.registerResource)
        core.regHandler('Intel-Media', 'DataDump', self.processDataDump)
        core.regHandler('Intel-Media', 'MetaData', self.processMetaData)
        core.regHandler('Intel-Media', 'VA_Render', self.vaRenderCtx)
        core.regHandler('Intel-Media', 'Picture_Param_AVC', self.PicParamAVC)
        core.regHandler('Intel-Media', 'MediaRuntimeLog', self.rtLog)
        core.regHandler('Intel-Media', 'MediaRuntimeError', self.rtLog)
        core.regHandler('Intel-Media', None, self.MediaEventHander)

    def mediaHeaderParse(self, raw, evt):
        # media trace header = 4byte tag, 2bytes size, 2 bytes event id, 4bytes type
        id = int.from_bytes(raw[4:8], 'little')
        type = int.from_bytes(raw[8:12], 'little')
        total = id & ((1<<16)-1)
        id >>= 16

        # overwrite event name/id/etc
        evt['id'] = id
        evt['sys'] = 'Intel-Media'
        evt['op'] = type
        return raw[12:12+total]

    def MediaEventHander(self, evt):
        key = str(evt['pid']) + str(evt['tid'])
        stack = self.sharedCtx['Stack']
        # WA old driver eDDI_Codec_View start end is not in pair, force it to info
        # new driver don't need this, which have new added CpTag field.
        if evt['name'] == 'eDDI_Codec_View' and evt['op'] == 1 and 'CpTag' not in evt['data']:
            evt['op'] = 0
        output = self.sharedCtx['UI']
        # process start end event
        if evt['op'] in [1, 2]:
            stat = self.sharedCtx['Stat']
            out = {'pid':evt['pid'], 'tid':evt['tid'], 'ts':evt['ts'], 'ph': self.phaseMap[evt['op']], 'name':evt['name']}
            if evt['op'] == 1:
                if evt['data']:
                    out['args'] = {'input':evt['data']}
                evt['event'] = out
                stack.push(evt)
                stat.enter({'id':key, 'class':'Media', 'name':evt['name'], 'ts':evt['ts']})
            else:
                stat.exit({'id':key, 'ts':evt['ts']})
                if evt['data']:
                    out['args'] = {'output':evt['data']}
                exit = stack.pop(evt)
                if exit != None and 'group' in exit:
                    if 'args' not in out:
                        out['args'] = exit['group']
                    else:
                        out['args'].update(exit['group'])
            output.AddEvent(out)
        else:
            cur = stack.find(evt)
            if cur != None:
                if 'group' not in cur:
                    cur['group'] = {}
                group = cur['group']
                if evt['name'] not in group:
                    group[evt['name']] = evt['data']
                else:
                    if isinstance(group[evt['name']], list):
                        group[evt['name']].append(evt['data'])
                    else: # change to list and append
                        save = group[evt['name']]
                        group[evt['name']] = [save, evt['data']]
            else:
                # this info event did not belong to any activity
                out = {'pid':evt['pid'], 'tid':evt['tid'], 'ts':evt['ts'], 'ph': 'i', 'name':evt['name']}
                out['args'] = evt['data']
                output.AddEvent(out)

    def vaRenderCtx(self, evt):
        if evt['op'] != 1:
            return
        ctx = evt['data']['VAContext']
        if 'Context' in ctx:
            # cut out XXX from 'ContextXXX (d)'
            v = ctx.split('Context')
            evt['name'] += '_'+v[1].split(' ')[0]

    def processDataDump(self, event):
        output = self.sharedCtx['UI']
        key = str(event['pid']) + str(event['tid'])
        if event['op'] == 1:
            event['ph'] = 'X'
            event['name'] = event['data']['Name']
            del event['data']['Name']
            event['raw'] = []
            self.DataDump[key] = event
        if event['op'] == 0:
            self.DataDump[key]['raw'] += event['data']['Data']
        if event['op'] == 2:
            name = self.DataDump[key]['name']
            self.DataDump[key]['dur'] = event['ts'] - self.DataDump[key]['ts']
            if 'dataParser' in self.sharedCtx and name in self.sharedCtx['dataParser']:
                self.sharedCtx['dataParser'][name](self.DataDump[key])
            else:
                self.ProcessRawData(self.DataDump[key])
            del self.DataDump[key]
        return -1

    def ProcessRawData(self, event):
        output = self.sharedCtx['UI']
        #event['args'] = {event['name']:event['raw']};
        event['args'] = {event['name']: 'skip'} # skip for now, the decrypt output is too big, UI fail to open
        del event['raw']
        output.AddEvent(event)

    def processMetaData(self, evt):
        output = self.sharedCtx['UI']
        if evt['op'] == 0:
            evt['ph'] = 'R'
            output.AddEvent(evt)
        elif evt['op'] == 1:
            output.AddMetaData(evt['data'])
        return -1

    def registerResource(self, evt):
        if evt['op'] == 0:
            stack = self.sharedCtx['Stack']
            media = stack.find(evt)
            if media != None:
                media['lastSurf'] = evt['data']['Surface Handle']
            return
        if evt['op'] != 3:
            return
        data = evt['data']
        if 'GpuAddr' not in data:
            return -1 # skip old event handling
        stack = self.sharedCtx['Stack']
        media = stack.find(evt)
        if media == None:
            return -1 # skip if no context found
        if 'resource' not in media:
            media['resource'] = []
        data['accessSize'] = data['Size']
        if 'lastSurf' in media:
            data['handle'] = media['lastSurf']
            # find more surface info from surface db
            info = {'pid': evt['pid'], 'handle':media['lastSurf']}
            surface = self.sharedCtx['Surface']
            record = surface.fetch(info)
            if record != None:
                for k in ['primary', 'Format', 'Width', 'Height', 'Pitch', 'Size']:
                    if k in record:
                        data[k] = record[k]
        media['resource'].append(data)
        return -1

    def PicParamAVC(self, evt):
        if evt['op'] != 0:
            return
        # find component name from context
        context = None
        if 'Stack' in self.sharedCtx:
            context = self.sharedCtx['Stack'].find(evt)
        if context == None or 'render' not in context:
            return
        data = evt['data']
        # update frame type to render target
        if 'IntraPic' in data['Flags']:
            context['render'][context['render']['target']]['Access'] += ' I'
        else:
            context['render'][context['render']['target']]['Access'] += ' P' # simply to P frame

    def writeOutput(self, line):
        if self.fp == None:
            self.fp = open(self.sharedCtx['Output'] + '_rtlog.txt', 'w')
        self.fp.write(line)

    def stripField(self, evt):
        data = evt['data']
        out = []
        comp = int(GetEnumVal(data['LogId'])) >> 24
        out.append(self.compMap[comp])
        id = GetEnumName(data['LogId'])[3:] # remove prefix MT_
        out.append(id)
        if 'LogLevel' in data:
            out.append(GetEnumName(data['LogLevel']))
        else:
            out.append('Error')
        cnt = len(data)//2
        for i in range(1, cnt):
            n = GetEnumName(data['Id'+str(i)])[3:] # remove prefix MT_
            v = GetEnumName(data['Value'+str(i)])
            out.append(n+': '+v)
        return out

    def rtLog(self, evt):
        out = self.stripField(evt)
        txt = '{:<10d} {:<6} {:<6} {:<6s} {:<30s} {:<8s}'.format(evt['ts'], evt['pid'], evt['tid'], out[0], out[1], out[2])
        for i in out[3:]:
            txt += '    ' + i
        self.writeOutput(txt+'\n')
        return -1
 
    def __del__(self):
        del self.DataDump
        if self.fp:
            self.fp.close()

