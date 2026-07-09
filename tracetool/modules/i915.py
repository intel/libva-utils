#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

from drawQueue import drawQueue

class traceHandler:
    i915EngineName = ['Render', 'Copy', 'VDBox', 'VEBox', 'Compute']

    def __init__(self, core):
        self.sharedCtx = core.getContext()
        self.gpuContext = {}
        self.fp = None

        core.regHandler('i915', 'i915_request_add', self.requestStart)
        core.regHandler('i915', 'i915_request_submit', self.requestStart)
        core.regHandler('i915', 'i915_request_retire', self.requestEnd)
        core.regHandler('i915', 'i915_request_in', self.requestGpuStart)
        core.regHandler('i915', 'i915_request_out', self.requestGpuEnd)
        core.regHandler('i915', 'i915_request_wait_begin', self.fenceWaitStart)
        core.regHandler('i915', 'i915_request_wait_end', self.fenceWaitEnd)
        core.regHandler('ftrace', 'print', self.traceMarkerProcess)
        core.regHandler('ftrace', 'bprint', self.i915printk)

    def writeMsg(self, line):
        if self.fp == None:
            self.fp = open(self.sharedCtx['Output'] + '_i915.txt', 'w')
        self.fp.write(line)

    def i915printk(self, evt):
        if evt['data']['mod'] != 'i915':
            return
        # get more runtime info from i916 trace log in next.
        line = '{:<10d} {:<6} {:<6} '.format(evt['ts'], evt['pid'], evt['tid'])
        self.writeMsg(line + evt['data']['msg'] + '\n')

    def requestStart(self, evt):
        data = evt['data']
        tname = data['ctx']
        output = self.sharedCtx['UI']
        stat = self.sharedCtx['Stat']
        if tname not in self.gpuContext:
            ctx = {'active':{}, 'queue':drawQueue(output), 'process':[]}
            for k in ['ctx', 'class', 'dev', 'instance']:
                ctx[k] = data[k]
            self.gpuContext[data['ctx']] = ctx
        ctx = self.gpuContext[data['ctx']]
        if data['seqno'] in ctx['active']:
            return # ingore sequence already known
        if evt['pid'] not in ctx['process']:
            ctx['process'].append(evt['pid'])
            engine = {'name':'thread_name', 'ph':'M', 'pid':evt['pid'], 'tid':tname}
            engine['args'] = {'name':' ctx '+str(ctx['ctx'])+'('+self.i915EngineName[ctx['class']]+str(ctx['instance'])+')'}
            output.AddMetaEvent(engine)
            namesort = {'name':'thread_sort_index', 'ph':'M', 'pid':evt['pid'], 'tid':tname, 'args':{'sort_index':1000000+ctx['class']}}
            output.AddMetaEvent(namesort)
        out = {'pid':evt['pid'], 'tid':tname, 'ts':evt['ts'], 'ph': 'X'}
        out['name'] = 'SW #{0}'.format(data['seqno'])
        fstart = {'pid':evt['pid'], 'tid':evt['tid'], 'ts':evt['ts'], 'ph': 's', 'id':str(data['ctx'])+str(data['seqno']), 'name':'submit', 'cat':'Workload Flow'}
        fend = {'pid':evt['pid'], 'tid':tname, 'ts':evt['ts'], 'ph': 'f', 'id':str(data['ctx'])+str(data['seqno']), 'name':'submit', 'bp':'e', 'cat':'Workload Flow'}
        ctx['active'][data['seqno']] = {'pid':evt['pid'], 'flow':[fstart, fend]}
        ctx['queue'].enter(out)
        stat.enter({'id':str(data['ctx'])+str(data['seqno']), 'class':'Engine'+self.i915EngineName[data['class']], 'name':'Ctx'+str(tname), 'ts':evt['ts']})

    def requestEnd(self, evt):
        data = evt['data']
        output = self.sharedCtx['UI']
        stat = self.sharedCtx['Stat']
        if data['ctx'] not in self.gpuContext:
            return # ignore unknown context
        # retire all fence <= current in context
        ctx = self.gpuContext[data['ctx']]
        for k in list(ctx['active']):
            if k <= data['seqno']:
                out = ctx['active'][k]
                for e in out['flow']:
                    output.AddEvent(e)
                ctx['queue'].exit({'name':'HW #{0}'.format(k), 'ts':evt['ts']})
                ctx['queue'].exit({'name':'SW #{0}'.format(k), 'ts':evt['ts']})
                del ctx['active'][k]
                stat.exit({'id':str(data['ctx'])+str(data['seqno']), 'ts':evt['ts']})

    def requestGpuStart(self, evt):
        data = evt['data']
        if data['ctx'] not in self.gpuContext or data['seqno'] not in self.gpuContext[data['ctx']]['active']:
            return # ignore unknown context
        ctx = self.gpuContext[data['ctx']]
        out = {'pid':ctx['active'][data['seqno']]['pid'], 'tid':data['ctx'], 'ts':evt['ts'], 'ph': 'X'}
        out['name'] = 'HW #{0}'.format(data['seqno'])
        ctx['queue'].enter(out)

    def requestGpuEnd(self, evt):
        data = evt['data']
        if data['ctx'] not in self.gpuContext:
            return # ignore unknown context
        ctx = self.gpuContext[data['ctx']]
        ctx['queue'].exit({'name':'HW #{0}'.format(data['seqno']), 'ts':evt['ts']})
        self.requestEnd(evt)

    def fenceWaitStart(self, evt):
        tname = evt['data']['ctx']
        if tname not in self.gpuContext:
            return
        out = {'pid':evt['pid'], 'tid':tname, 'ts':evt['ts'], 'ph': 'X'}
        out['name'] = 'Wait #{0}'.format(evt['data']['seqno'])
        ctx = self.gpuContext[tname]
        ctx['queue'].enter(out)

    def fenceWaitEnd(self, evt):
        data = evt['data']
        if data['ctx'] not in self.gpuContext:
            return
        ctx = self.gpuContext[data['ctx']]
        ctx['queue'].exit({'name':'Wait #{0}'.format(data['seqno']), 'ts':evt['ts']})
        # fence complete
        self.requestEnd(evt)

    def traceMarkerProcess(self, evt):
        # per-process counters trace format:
        #  intel_media:counters,process=abc,pid=123,counter1=value1,counter2=value2
        # global counters trace format:
        #  intel_media:counters,counter1=value1,counter2=value2
        output = self.sharedCtx['UI']
        data = evt['data']
        tmpStr = data['buf']
        elements = tmpStr.split(',')
        if elements[0] == 'counters':
            evt['ph'] = 'C'
            del elements[0]
            args = {}
            for e in elements:
                name = e.split("=")[0]
                value = e.split("=")[1]
                if name == 'process':
                    evt['pid'] = value
                    evt['name'] = 'per-process counters'
                elif name == 'pid':
                    evt['id'] = value
                else:
                    args[name] = int(value)
            evt['args'] = args
            if evt['name'] == 'print':
                evt['name'] = 'global counters'
                evt['id'] = evt['tid']
            output.AddEvent(evt)

    def __del__(self):
        if self.fp:
            self.fp.close()

