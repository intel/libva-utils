#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

# build call stack from events with the same process and thread id
class callStack:

    def __init__(self):
        self.context = {} # maintain call stack

    # get latest pushed call event in stack
    def current(self, pid, tid):
        if pid not in self.context or tid not in self.context[pid]:
            return None
        if self.context[pid][tid]:
            return self.context[pid][tid][0]
        return None

    # get full call stack record
    def get(self, pid, tid):
        if pid not in self.context:
            self.context[pid] = {}
        if tid not in self.context[pid]:
            self.context[pid][tid] = []
        return self.context[pid][tid]

    # push event into stack
    def push(self, evt):
        if evt['pid'] not in self.context:
            self.context[evt['pid']] = {}
        if evt['tid'] not in self.context[evt['pid']]:
            self.context[evt['pid']][evt['tid']] = []
        self.context[evt['pid']][evt['tid']].insert(0, evt)

    # pop event from stack
    def pop(self, evt):
        if evt['pid'] not in self.context:
            return None
        if evt['tid'] not in self.context[evt['pid']] or not self.context[evt['pid']][evt['tid']]:
            thrds = self.context[evt['pid']]
            for t in thrds.values():
                if t and t[0]['name'] == evt['name']:
                    return t.pop(0)
            return None
        ctx = self.context[evt['pid']][evt['tid']]
        name = evt['name']
        idx = 0
        ret = None
        # find target in the stack
        for i in range(len(ctx)):
            if ctx[i]['name'] == name:
                idx = i+1
                ret = ctx[i]
                break
        # remove target from stack
        del ctx[0:idx]
        return ret

    # find top event with the same sys id + pid + tid
    def find(self, evt):
        if evt['pid'] not in self.context or evt['tid'] not in self.context[evt['pid']]:
            return None
        for e in self.context[evt['pid']][evt['tid']]:
            if e['sys'] == evt['sys']:
                return e
        return None

    def __del__(self):
        del self.context
