#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

# split block to fix chrome tracing queue limitation
class drawQueue:
    def __init__(self, output):
        self.queue = []
        self.Output = output

    def enter(self, evt):
        self.queue.append(evt)

    def exit(self, evt):
        found = 0
        for i in range(0, len(self.queue)):
            if self.queue[i]['name'] == evt['name']:
                found = 1
                break
        if found == 0:
            return
        # handle exit for itself
        out = self.queue[i]
        if evt['ts'] > out['ts']:
            out['dur'] = evt['ts'] - out['ts']
            self.Output.AddEvent(out)
        del self.queue[i]
        # split block behinds in queue
        for s in range(i, len(self.queue)):
            slice = self.queue[s]
            if evt['ts'] > slice['ts']:
                out = slice.copy()
                out['dur'] = evt['ts'] - out['ts']
                slice['ts'] = evt['ts']
                self.Output.AddEvent(out)

    def finalize(self):
        for blk in self.queue:
            blk['ph'] = 'B'
            self.Output.AddEvent(blk)
        self.queue = []
