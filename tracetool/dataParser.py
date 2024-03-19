#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import os
import json
import glob
import uuid
import struct
from manifest import *

def formatAnsiStr(raw):
    str = ''
    size = 0
    while True:
        c = raw[size]
        size += 1
        if c == 0 or len(raw) <= size:
            break
        str += chr(c)
    return str, size

def formatUnicodeStr(raw):
    str = ''
    size = 0
    while True:
        c = raw[size] + (raw[size+1]<<8)
        size += 2
        if c == 0:
            break
        str += chr(c)
    return str, size

def formatInt(raw):
    return int.from_bytes(raw, 'little'), 0 # return 0 for fix sized data type

def formatHex(raw):
    return hex(int.from_bytes(raw, 'little')), 0

def formatFloat(raw):
    return struct.unpack('f', raw)[0], 0

def formatDouble(raw):
    return struct.unpack('d', raw)[0], 0

def formatMap(raw, fmt):
    ret = ''
    val = int.from_bytes(raw, 'little')
    if val in fmt:
        ret= fmt[val]
    ret += ' ({:d})'.format(val)
    return ret, 0

def formatBits(raw, fmt):
    ret = ''
    val = int.from_bytes(raw, 'little')
    if val:
        pos = 0
        i = val
        while i:
            if i & 1 and pos in fmt:
                ret += fmt[pos] + ' | '
            pos += 1
            i >>= 1
        ret = ret[:-3] # remove | in tail
    elif 0 in fmt:
        # need special handle for 0
        ret += fmt[0]
    ret += ' (0x{:x})'.format(val)
    return ret, 0

def formatGUID(raw):
    return uuid.UUID(bytes_le=raw), 0

def formatBinary(raw):
    ret = ''
    for b in raw:
        ret += '%02x' % b
    return ret, 0 # no need to return size

class dataParser:
    def __init__(self, pointerSize):
        self.pointerSize = pointerSize

    def getInTypeSize(self, fmt):
        # field in [name, input type, output type, [len/size from field name]]
        if 'Binary' in fmt[1]:
            return fmt[3]
        if 'Int8' in fmt[1]:
            return 1
        if 'Int16' in fmt[1]:
            return 2
        if 'Int32' in fmt[1]:
            return 4
        if 'Int64' in fmt[1]:
            return 8
        if 'Boolean' in fmt[1]:
            return 4
        if 'Pointer' in fmt[1]:
            return self.pointerSize
        if 'Float' in fmt[1]:
            return 4
        if 'Double' in fmt[1]:
            return 8
        return 0 # return 0 for variable length

    def parseDataStruct(self, raw, tpl):
        ret = {}
        offset = 0
        total = len(raw)
        for f in tpl:
            field = f[0]
            size = f[1]
            format = f[2]
            num = f[3]
            count = 1
            elem_size = 0
            if isinstance(size, str):
                size = ret[size]
            if isinstance(num, str):
                count = ret[num]
                if count > 65535:
                    elem_size = count >> 16
                    count &= 65535
                    ret[num] = count
                num = count
            if isinstance(field, list):
                substruct = []
                for i in range(0, count):
                    sub, size = self.parseDataStruct(raw[offset:], field)
                    substruct.append(sub)
                    if elem_size != 0:
                        size = elem_size
                    offset += size
                ret[format] = substruct
                continue
            if num > 0:
                out = []
                for i in range(0, num):
                    o,s = format[0](raw[offset:offset+size], *format[1:])
                    out.append(o)
                    offset += size + s
                ret[field] = out
                continue
            if size:
                data = raw[offset:offset+size]
            else:
                data = raw[offset:]
            o,s = format[0](data, *(format[1:]))
            ret[field] = o
            offset += size + s
            if offset >= total:
                break # happen in case of manifest version > driver trace version
        return ret, offset

    def optimizeDataStruct(self, fmt, tpl, m):
        st = []
        for f in tpl[fmt]:
            item = []
            if f[1] == 'struct':
                item.append(self.optimizeDataStruct(f[0], tpl, m))
                item.append(0)
                item.append(f[0].replace(fmt, ''))
                item.append(f[2])
                st.append(item)
                continue
            item.append(f[0])
            item.append(self.getInTypeSize(f))
            type = f[1]
            if 'Binary' in type:
                item.append([formatBinary])
            elif 'Float' in type:
                item.append([formatFloat])
            elif 'Double' in type:
                item.append([formatDouble])
            elif 'AnsiString' in type:
                item.append([formatAnsiStr])
            elif 'UnicodeString' in type:
                item.append([formatUnicodeStr])
            elif 'GUID' in type:
                item.append([formatGUID])
            else: # default format as int
                item.append([formatInt])
            if len(f) > 3 and 'Binary' not in type:
                item.append(f[3])
            else:
                item.append(0)
            # adjust dec/hex output format
            out = f[2]
            if 'HexInt' in out:
                item[2] = [formatHex]
            if out in m['bitMap']:
                item[2] = [formatBits, m['bitMap'][out]]
            elif out in m['valueMap']:
                item[2] = [formatMap, m['valueMap'][out]]
            st.append(item)
        return st

    def optimizeManifest(self, manifest):
        m = {'name':manifest['name'], 'id': manifest['id'], 'events': manifest['events']}
        # optimize enum maps, split value and bit map, convert key to int
        m['valueMap'] = {}
        m['bitMap'] = {}
        for k,v in manifest['maps'].items():
            type = v['type']
            del v['type']
            if type:
                maps = {}
                pos = 0
                for b,n in v.items():
                    v = int(b, 0)
                    if v:
                        v >>=pos
                        while v:
                            if v == 1:
                                maps[pos] = n
                                break
                            else:
                                pos += 1
                                v >>= 1
                    else:
                        maps[0] = n
                m['bitMap'][k] = maps
            else:
                maps = {}
                for b,n in v.items():
                    maps[int(b, 0)] = n
                m['valueMap'][k] = maps
        # optimize data template, preprocess data type, size and output format
        tpl = {}
        for k in manifest['templates'].keys():
            tpl[k] = self.optimizeDataStruct(k, manifest['templates'], m)
        m['templates'] = tpl
        return m

    def loadManifest(self, name = 'all'):
        files = glob.glob(r'manifests' + os.sep + '*')
        # load media trace format
        self.manifest = {}
        for f in files:
            if f.endswith('.json'):
                fp = open(f, 'r')
                m = json.load(fp)
                fp.close()
            elif f.endswith('.man'):
                m = manifest.load(f)
            else:
                continue
            m = self.optimizeManifest(m)
            if m['name'] not in self.manifest:
                self.manifest[m['name']] = m
            else:
                # select the big one
                if len(self.manifest[m['name']]['templates']) < len(m['templates']):
                    self.manifest[m['name']] = m

    def parseName(self, evt):
        if evt['sys'] in self.manifest and str(evt['id']) in self.manifest[evt['sys']]['events']:
            desc = self.manifest[evt['sys']]['events'][str(evt['id'])]
            evt['name'] = desc['name']
            return evt
        return None

    def parseData(self, raw, event):
        if event['sys'] not in self.manifest:
            return None # ignore unknown provider name
        manifest = self.manifest[event['sys']]
        if str(event['id']) not in manifest['events']:
            return None # ignore unknown event id
        desc = manifest['events'][str(event['id'])]
        if str(event['op']) not in desc['op']:
            return None # ignore unknown event opcode
        fmtId = desc['op'][str(event['op'])]
        evtData = {}
        if fmtId in manifest['templates']:
            evtData, size = self.parseDataStruct(raw, manifest['templates'][fmtId])
        event['name'] = desc['name']
        del event['id']
        event['data'] = evtData
        return event

    def getIds(self):
        map = {}
        for name in self.manifest:
            id = uuid.UUID(self.manifest[name]['id']).bytes_le.hex()
            map[id] = name
        return map
