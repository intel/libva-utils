#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import os
import re
import time
from dataParser import *
from util import *

TYPE_PADDING = 29
TYPE_TIME_EXTEND = 30
TYPE_TIME_STAMP = 31

FIELD_ARRAY = (1<<8)
FIELD_DYNAMIC = (1<<9)
FIELD_STRING = (1<<10)

OPTION_DATE = 1
OPTION_CPUSTAT = 2
OPTION_BUFFER = 3
OPTION_TRACECLOCK = 4
OPTION_UNAME = 5
OPTION_HOOK = 6
OPTION_OFFSET = 7
OPTION_CPUCOUNT = 8
OPTION_VERSION = 9
OPTION_PROCMAPS = 10
OPTION_TRACEID = 11
OPTION_TIME_SHIFT = 12
OPTION_GUEST = 13

def readString(file):
    str = ''
    while True:
        c = file.read(1)
        if c == b'\x00':
            break
        str += c.decode('ascii')
    return str

def readNum(file, bytes):
    return int.from_bytes(file.read(bytes), 'little')

def parseProcessName(string):
    lines = string.splitlines()
    map = {}
    for l in lines:
        sub = l.split(' ')
        map[int(sub[0])] = sub[1]
    return map

def parsePrintkName(string):
    lines = string.splitlines()
    map = {}
    for l in lines:
        sub = l.split(':', 1)
        fmt = sub[1].replace('"', '')
        fmt = fmt.replace('\\n', '')
        map[int(sub[0], 16)] = fmt
    return map

def parseSymbolName(string):
    lines = string.splitlines()
    map = {}
    for l in lines:
        sub = l.split(' ')
        if sub[1] in ['A', 'a'] or '[' not in sub[2]:
            continue
        name = sub[2].split('\t')
        name[1] = name[1][1:-1] # remove []
        map[int(sub[0], 16)] = name
    return map

def parseEventFormat(string, fmt, sys, pointerSize):
    lines = string.splitlines()
    val = re.findall(r'name:\s+(\w+)', lines[0])
    if len(val) != 1:
        print('invalid event format: ' + string)
        return
    name = val[0]
    val = re.findall(r'ID:\s+(\d+)', lines[1])
    if len(val) != 1:
        print('invalid event format: ' + string)
        return
    id = int(val[0])
    fields = []
    for i in range(7, len(lines)): # skip common fields in header
        # skip non field line
        if 'field:' not in lines[i]:
            continue
        attr = lines[i].split(';')
        # in format of [field: xxx, offset:dd, size:dd, ...]
        if len(attr) > 3:
            offset = int(re.findall(r'\d+', attr[1])[0])
            size = int(re.findall(r'\d+', attr[2])[0])
            head = attr[0].split(' ')
            fname = head[-1] # normally field name is the last one, but array name could be diff
            flags = 0
            if '[' in attr[0]:
                # remove [xxx] in field name if have
                if '[' in fname:
                    fname = fname.partition('[')[0]
                # handle case like __u8 saddr[sizeof(struct sockaddr_in6)]
                if ']' in fname:
                    fname = head[-2].partition('[')[0]
                head[0] = head[0].strip().replace('field:', '')
                # remove __data_loc/signed/unsigned, only keep data type
                if head[0] == '__data_loc':
                    del head[0]
                if head[0] in ['signed', 'unsigned']:
                    del head[0]
                if '8' in head[0] or 'char' in head[0]:
                    elem_size = 1
                elif '16' in head[0] or 'short' in head[0]:
                    elem_size = 2
                elif '32' in head[0] or 'int' in head[0]:
                    elem_size = 4
                elif '64' in head[0] or (head[0] == 'long' and head[1] == 'long'):
                    elem_size = 8
                elif 'long' in head[0]:
                    elem_size = pointerSize
                else:
                    print('warning: fail to extract field in ' + lines[i])
                    return
                flags = size//elem_size
                if size != elem_size * flags:
                    print('warning: array size mismatch in ' + lines[i])
                flags |= FIELD_ARRAY
                if '__data_loc' in attr[0]:
                    flags |= FIELD_DYNAMIC
                    if 'char' in attr[0]:
                        flags |= FIELD_STRING
            fields.append([fname, offset, size, flags])
    fmt[id] = {'sys': sys, 'name':name, 'field':fields}

def formatPrintk(fmt, raw):
    if '%' in fmt:
        sub = fmt.split('%')
        ret = sub[0]
        offset = 0
        for i in sub[1:]:
            if i == '':
                ret += '%'
                continue
            if i[0] == 's':
                o, s = formatAnsiStr(raw[offset:])
                ret += o + i[1:]
                offset += s
                continue
            # skip number after %
            while i[0] in ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9']:
                i = i[1:]
            # calc data size
            s = 4
            if i[0:2] == 'll':
                s = 8
                i = i[2:]
            elif i[0] == 'l':
                i = i[1:]
            offset += 3
            offset &= ~3
            val = int.from_bytes(raw[offset:offset+s], 'little', signed=True)
            offset += s
            if i[0] in ['x', 'X']:
                ret += hex(val)
            else:
                ret += str(val)
            ret += i[1:]
    else:
        ret = fmt
    return ret

def ip2addr(sl, target):
    end = len(sl)
    pos = 0
    # binary search in sorted list
    while end - pos > 1:
        mid = pos + (end - pos)//2
        if target < sl[mid]:
            end = mid
        else:
            pos = mid
    return sl[pos]

def getTs(it):
    return it['ts']

def field2str(field):
    ret = ''
    for k in field:
        if isinstance(field[k], list):
            if len(field[k]) == 0:
                continue
            if isinstance(field[k][0], dict):
                for a in field[k]:
                    ret += field2str(a)
            else:
                ret += '{0}: '.format(k)
                for a in field[k]:
                    ret += '{0} '.format(a)
                ret += '; '
        elif isinstance(field[k], dict):
            ret += field2str(field[k])
        else:
            ret += '{0}:{1}; '.format(k, field[k])
    return ret

def dumpTrace(fp, evt):
    opmap = {0:'info', 1:'start', 2:'end', 3:'info'}
    line = evt['pid']+'-'+str(evt['tid'])+','+str(evt['ts'])+','+evt['name']+','+opmap[evt['op']]+','
    line += field2str(evt['data'])
    line += '\n'
    fp.write(line)

class traceReader:

    def __init__(self):
        self.tracefp = None
        self.dumpRaw = False
        self.metaData = {}

    def open(self, file, options):
        if 'raw' in options:
            self.dumpRaw = True
        fp = open(file, 'rb')
        if fp == -1:
           print("failed to open trace file" + file)
           return -1
        magic = fp.read(10)
        if magic != b'\x17\x08\x44\x74\x72\x61\x63\x69\x6e\x67':
            return -1
        if self.dumpRaw:
            self.dumpFp = open(os.path.splitext(file)[0]+'.csv', 'w')
            self.dumpFp.write('process,ts,event,opcode,event data\n')
        ver = readString(fp)
        print('trace version '+ver)
        endian = readNum(fp, 1)
        if endian != 0:
            print('big endian trace not support yet')
            return -1
        self.pointerSize = readNum(fp, 1)
        self.pageSize = readNum(fp, 4)
        # page header struct in string
        if readString(fp) != 'header_page':
            print('invalid header page in ftrace data file')
            return -1
        size = readNum(fp, 8)
        # skip page header, script don't need that
        fp.seek(size, 1)
        # event header
        if readString(fp) != 'header_event':
            print('invalid header event in ftrace data file')
            return -1
        size = readNum(fp, 8)
        # skip event header, script don't need that
        fp.seek(size, 1)
        # ftrace event format
        count = readNum(fp, 4)
        manifest = {}
        for i in range(0, count):
            size = readNum(fp, 8)
            buf = fp.read(size).decode('ascii')
            parseEventFormat(buf, manifest, 'ftrace', self.pointerSize)
        # event provider system
        count = readNum(fp, 4)
        for i in range(0, count):
            providerName = readString(fp)
            num = readNum(fp, 4)
            for j in range(0, num):
                size = readNum(fp, 8)
                buf = fp.read(size).decode('ascii')
                parseEventFormat(buf, manifest, providerName, self.pointerSize)
        self.manifest = manifest
        # ksym info
        size = readNum(fp, 4)
        buf = fp.read(size).decode('ascii')
        self.symbolMap = parseSymbolName(buf)
        self.symbolAddr = sorted(list(self.symbolMap.keys()))
        # prink info
        size = readNum(fp, 4)
        buf = fp.read(size).decode('ascii')
        self.printkMap = parsePrintkName(buf)
        # process info
        size = readNum(fp, 8)
        buf = fp.read(size).decode('ascii')
        self.procesNameMap = parseProcessName(buf)
        # load fixup section, sometime trace-cmd trace miss process name/id map
        fixup = os.path.splitext(file)[0]+'.fixup'
        if os.path.isfile(fixup):
            f = open(fixup, 'r')
            m = parseProcessName(f.read())
            f.close()
            self.procesNameMap.update(m)
        # cpu num info
        cpuNum = readNum(fp, 4)
        # option
        str = readString(fp)
        if 'options' in str:
            type = readNum(fp, 2)
            while type != 0:
                size = readNum(fp, 4)
                if size > 0:
                    self.parseTraceOptions(type, fp.read(size))
                type = readNum(fp, 2)
            str = readString(fp) # read next option
        # event buffers
        totalSize = 0
        bufmap = []
        if 'flyrecord' in str:
            # read buffer in pair of offset and size in file
            for i in range(0, cpuNum):
                offset = readNum(fp, 8)
                size = readNum(fp, 8)
                bufmap.append({'offset':offset, 'size':size, 'cur':0, 'events':[]})
                totalSize += size
        self.bufmap = bufmap
        self.tracefp = fp
        # load media trace format
        self.parser = dataParser(self.pointerSize)
        self.parser.loadManifest()
        self.timeStamp = time.time()
        self.curOffset = 0           # for progress report
        self.totalSize = totalSize
        return 0

    def setParser(self, parsers):
        self.headerParser = parsers

    def syncSource(self, src):
        return -1 # not support ftace with guc log

    def parseTraceOptions(self, type, raw):
        if type == OPTION_UNAME:
            self.metaData['Uname'] = raw.decode('ascii').rstrip('\x00')
        if type == OPTION_CPUSTAT:
            stat = raw.decode('ascii').rstrip('\x00').split('\n', 1)
            self.metaData[stat[0]] = stat[1]

    def parseBPrint(self, raw, evt, fmt):
        f = fmt[0]
        ip = int.from_bytes(raw[f[1]:f[1]+f[2]], 'little')
        addr = ip2addr(self.symbolAddr, ip)
        [func, mod] = self.symbolMap[addr]
        f = fmt[1]
        v = int.from_bytes(raw[f[1]:f[1]+f[2]], 'little')
        if v in self.printkMap:
            s = self.printkMap[v]
        else:
            s = '...'
        f = fmt[2]
        msg = func + ': ' + formatPrintk(s, raw[f[1]:])
        evt['data'] = {'mod': mod, 'msg': msg}
        return evt

    def parseFTrace(self, raw, event, fmt):
        event['op'] = 0
        data = {}
        offset = 0
        if event['name'] == 'bprint':
            return self.parseBPrint(raw, event, fmt)
        if event['name'] == 'print':
            for f in fmt:
                if f[0] == 'buf':
                    offset = f[1]
                    string, size = parseDataString(raw[offset:])
                    fields = string.split(':')
                    if len(fields) != 2:
                        return None
                    label = fields[0]
                    content = fields[1]
                    if label == 'intel_media':
                        data[f[0]] = content
                        event['data'] = data
                    else:
                        event = None
                    return event
        for f in fmt:
            flags = f[3]
            if flags == 0:
                val = int.from_bytes(raw[f[1]:f[1]+f[2]], 'little')
                data[f[0]] = val
            elif flags & FIELD_STRING:
                if flags & FIELD_DYNAMIC:
                    val = int.from_bytes(raw[f[1]:f[1]+f[2]], 'little')
                    offset = val & ((1<<16)-1)
                else:
                    offset = f[1]
                string, size = formatAnsiStr(raw[offset:])
                data[f[0]] = string
            elif flags & FIELD_ARRAY:
                if flags & FIELD_DYNAMIC:
                    val = int.from_bytes(raw[f[1]:f[1]+f[2]], 'little')
                    offset = val & ((1<<16)-1)
                    size = val >> 16
                else:
                    offset = f[1]
                    size = f[2]
                array = []
                elemSize = f[3] & 255
                if elemSize == 0:
                    continue
                for i in range(0, size//elemSize):
                    elem = raw[offset+i*elemSize:offset+(i+1)*elemSize]
                    array.append(int.from_bytes(elem, 'little'))
                data[f[0]] = array
        event['data'] = data
        return event

    def parseEventData(self, data):
        # event data header = 4byte id, 4bytes pid
        id = int.from_bytes(data[0:4], 'little') & ((1<<16)-1)
        pid = int.from_bytes(data[4:8], 'little')
        if id not in self.manifest:
            print('event id:(0) is not recognized'.format(id))
            return None
        fmt = self.manifest[id]
        if pid in self.procesNameMap:
            pname = self.procesNameMap[pid]
        else:
            pname = 'Unknown'
        event = {'pid':pname, 'tid':pid, 'sys':fmt['sys'], 'name':fmt['name']}
        offset = 8

        if event['name'] == 'raw_data':
            for k in self.headerParser.keys():
                size = len(k)
                id = data[offset:offset+size]
                if id in self.headerParser:
                    evtRawData = self.headerParser[id](data[offset:], event)
                    if self.parser.parseName(event) == None:
                        return None
                    # delay event data parse
                    event['rawData'] = evtRawData
                    return event
        # delay ftrace data parse too
        event['rawData'] = data
        event['format'] = fmt['field']
        return event

    def processInPage(self, page):
        events = []
        pageTime = int.from_bytes(page[0:8], 'little')
        offset = 8 + self.pointerSize
        while offset < len(page):
            hdr = int.from_bytes(page[offset:offset+4], 'little')
            offset += 4
            type = hdr & ((1<<5)-1)
            delta = hdr >> 5
            if type == TYPE_PADDING:
                offset += int.from_bytes(page[offset:offset+4], 'little')
                size = 0
            elif type == TYPE_TIME_EXTEND:
                pageTime += int.from_bytes(page[offset:offset+4], 'little') << 27
                pageTime += delta
                offset += 4
                size = 0
            elif type == TYPE_TIME_STAMP:
                pageTime = int.from_bytes(page[offset:offset+4], 'little') << 27
                offset += 4
                size = 0
            elif type == 0:
                size = int.from_bytes(page[offset:offset+4], 'little') - 4
                size = (size+3) & ~3
                offset += 4
            else:
                size = type * 4
            if size > 0:
                eventData = page[offset:offset+size]
                offset += size
                pageTime += delta
                event = self.parseEventData(eventData)
                if event != None:
                    event['ts'] = pageTime//1000
                    events.append(event)
        return events

    def peekNextEvent(self):
        latest = None
        event = None
        # search from buf queue, find the smallest time stamp event
        for buf in self.bufmap:
            if not buf['events']:
                if buf['cur'] < buf['size']:
                    self.tracefp.seek(buf['offset']+buf['cur'], 0)
                    data = self.tracefp.read(self.pageSize)
                    buf['cur'] += self.pageSize
                    buf['events'] = self.processInPage(data)
                    self.reportProgress(self.pageSize)
                if not buf['events']:
                    continue
            if latest is None or latest['events'][0]['ts'] > buf['events'][0]['ts']:
                latest = buf
        if latest is not None:
            event = latest['events'].pop(0)
        return event

    def reportProgress(self, size):
        self.curOffset += size
        # print progress every second
        if time.time() - self.timeStamp < 1:
            return
        else:
            print('\rProcessEvent: %.1f%%' %(self.curOffset / self.totalSize * 100), end='')
            self.timeStamp = time.time()

    def process(self, filter, handler):
        self.filter = filter
        event = self.peekNextEvent()
        if event == None:
            return
        startTs = event['ts']
        # add a marker event to match UI timestamp with statistic
        marker = {'sys':'Intel-Media', 'pid':event['pid'], 'tid':event['tid'], 'name':'MetaData', 'op':0, 'ts':0}
        handler(marker)
        for k,v in self.metaData.items():
            handler({'sys':'Intel-Media', 'name':'MetaData', 'op':1, 'data':{k:v}})
        del self.metaData
        while event != None:
            if self.dumpRaw or self.filter(event):
                if 'format' in event:
                    self.parseFTrace(event['rawData'], event, event['format'])
                    del event['format']
                else:
                    self.parser.parseData(event['rawData'], event)
                del event['rawData']
            if self.dumpRaw:
                dumpTrace(self.dumpFp, event)
            event['ts'] -= startTs # give tool relative timestamp for ftrace
            handler(event)
            event = self.peekNextEvent()

    def __del__(self):
        if self.tracefp:
            self.tracefp.close()
        if self.dumpRaw:
            self.dumpFp.close()
