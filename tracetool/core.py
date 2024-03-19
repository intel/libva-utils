#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import os
import sys
import importlib
from writeUI import writeUI
from statistic import statistic
from callStack import callStack
from util import *

class core:

    def __init__(self):
        self.source = None
        self.sharedCtx = {} # shared context for all handlers, dict for flexible usage
        self.handlers  = {}
        self.instances = []
        self.readers = []
        self.parsers = {}
        self.dumpRaw = False

        cur = os.path.abspath(os.path.dirname(__file__))
        sys.path.append(cur+os.sep+'modules')
        for py in os.listdir('modules'):
            name = os.path.splitext(py)[0]
            m = importlib.import_module(name)
            if hasattr(m, 'traceHandler'):
                cls = getattr(m, 'traceHandler')
                # create handler instace, the class init should call register of this instance
                instance = cls(self)
                # just for keep instance ref
                self.instances.append(instance)
            elif hasattr(m, 'traceReader'):
                cls = getattr(m, 'traceReader')
                self.readers.append(cls)

    # open trace file
    def open(self, input, options) -> int:
        ret = -1
        if isinstance(input, list) and len(input) == 1:
            input = input[0]
        if isinstance(input, list):
            # enumerate and open trace files
            names = []
            readers = []
            for i in input:
                for cls in self.readers:
                    reader = cls()
                    sts = reader.open(i, options)
                    if sts == 0:
                        names.append(i)
                        readers.append(reader)
                        break
            if len(input) == len(readers):
                # sync time stamp across multi trace files, need find single source reader
                print('Multi trace input files, sync time line ...')
                for i in readers:
                    for j in readers:
                        if i != j and i.syncSource(j) == 0:
                            self.source = i
                            self.sharedCtx['sourceFile'] = names[readers.index(i)]
                            break
                    if self.source != None:
                        break
                if self.source != None:
                    print('done')
                    ret = 0
                else:
                    print('Error! could not syn time line')
        else:
            for cls in self.readers:
                reader = cls()
                sts = reader.open(input, options)
                if sts == 0:
                    self.source = reader
                    self.sharedCtx['sourceFile'] = input
                    ret = 0
                    break
        # setup handlers and output if success
        if ret == 0:
            self.source.setParser(self.parsers)

            baseName = self.sharedCtx['sourceFile']
            baseName = os.path.splitext(baseName)[0]
            self.sharedCtx['Output'] = baseName
            self.sharedCtx['UI'] = writeUI(baseName)
            self.sharedCtx['Stat'] = statistic(baseName)
            self.sharedCtx['Stack'] = callStack()
            self.sharedCtx['Opt'] = options
        return ret

    # start process event from trace file
    def process(self) -> None:
        self.source.process(self.filter, self.callback)

    # close
    def __del__(self):
        del self.source
        del self.readers
        del self.instances
        del self.handlers
        del self.sharedCtx

    # test if event handler is set for this event
    def filter(self, evt) -> bool:
        if 'raw' in self.sharedCtx['Opt']:
            return True
        if 'sys' not in evt or 'name' not in evt:
            return False
        if evt['sys'] not in self.handlers:
            return False
        handler = self.handlers[evt['sys']]
        if evt['name'] not in handler and 'allEvent' not in handler:
            return False
        return True

    # call back function to process event with handler
    def callback(self, evt) -> None:
        if evt['sys'] not in self.handlers:
            return
        # get handler, could be a list, multi handler for single event
        hnd = self.handlers[evt['sys']]
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

    # register event handler
    def regHandler(self, sys, name, handler) -> None:
        if name == None:
            name = 'allEvent' # name = None means handler for all events of this trace system
        if sys not in self.handlers:
            self.handlers[sys] = {}
        # add handler to list
        hnd = self.handlers[sys]
        if name in hnd:
            hnd[name].append(handler)
        else:
            hnd[name] = [handler]

    # register event head parser from raw message
    def regParser(self, id, parser) -> int:
        if id in self.parsers:
            print('Warning! duplicated event header id')
            return -1
        self.parsers[id] = parser
        return 0

    # get shared context
    def getContext(self) -> dict:
        return self.sharedCtx
