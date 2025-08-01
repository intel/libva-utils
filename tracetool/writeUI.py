#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import os
import gzip
import json

# chrome browser has limitation on trace file, should < 250MB
MAX_EVENTS_PER_FILE = 1500000

class writeUI:

    def __init__(self, name):
        self.name = name
        self.traceEvents = []
        self.metaEvents = []
        self.metaData = {}
        self.book = None
        self.num = 0
        self.idx = 1

    def setupOutput(self):
        if self.book == None:
            self.book = gzip.open(self.name+'.json.gz', 'wt')
            self.book.write('{"traceEvents":[\n')

    def AddMetaEvent(self, evt):
        self.metaEvents.append(evt)

    def AddMetaData(self, evt):
        self.metaData.update(evt)

    def AddEvent(self, evt):
        self.setupOutput()
        #cleanup event field
        if 'op' in evt:
            del evt['op']
        if 'sys' in evt:
            del evt['sys']
        if 'data' in evt:
            del evt['data']
        if 'surfaces' in evt:
            del evt['surfaces']
        if self.num != 0:
            self.book.write(',\n')
        json.dump(evt, self.book)
        self.num += 1
        if self.num > MAX_EVENTS_PER_FILE:
            self.close()
            self.book = gzip.open(self.name+str(self.idx)+'.json.gz', 'wt')
            self.idx += 1
            self.num = 1
            self.book.write('{"traceEvents":[\n')
            start = {'pid':evt['pid'], 'tid':evt['tid'], 'name':'start', 'ph':'R', 'ts':0}
            json.dump(start, self.book)

    def close(self):
        if self.num == 0:
            return
        # append meta event, which is process/thread name event
        for e in self.metaEvents:
            self.book.write(',\n')
            json.dump(e, self.book)
        self.book.write('],\n')
        # append meta data, which is overall trace info event
        line = json.dumps(self.metaData)
        self.book.write(line[1:])
        self.book.close()

    def __del__(self):
        self.close()
