#!/usr/bin/env python3

#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#
import sys
import os
import gzip
import json
from core import *
from version import version

# print version
name = os.path.basename(os.path.dirname(os.path.realpath(__file__)))
print('{:s} - Version: {:s}'.format(name, version))

inputs = []
options = []
for i in sys.argv[1:]:
    if i.startswith('-'):
        options.append(i[1:])
    else:
        inputs.append(i)

if len(inputs) < 1:
    print('No input trace specified.')
    print('  usage: python3 main.py [-raw] file.dat|file.etl [file.dat|file.etl ...]')
    print('')
    print('Options:')
    print('-raw      Parse trace events and dump into <trace-file>.csv file (ftrace) or <trace-file>.json file')
    sys.exit(0)

trace = core()
if trace.open(inputs, options) < 0:
    print('fail to open trace file', inputs)
    sys.exit(0)

trace.getContext()['UI'].AddMetaData({'TraceTool':version})
trace.process()

print("\rProcessEvent: 100% !")

# finalize and generate report, remove input file ext
print("Generating reports ...")
del trace
print("Trace Analysis Complete!")
