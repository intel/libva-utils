#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import os

class statistic:

    def __init__(self, file):
        self.file = file
        self.fp = None
        self.fields = ['class','name','ts','latency']
        self.cache = {}

    def add(self, elem):
        if self.fp == None:
            self.fp = open(self.file + '_stat.csv', 'w')
            line = ''
            for f in self.fields:
                line += '{0},'.format(f)
            line += '\n'
            self.fp.write(line)
        line = ''
        for f in self.fields:
            if f in elem:
                line += '{0},'.format(elem[f])
            else:
                line += ','
        line += '\n'
        self.fp.write(line)

    def enter(self, elem):
        if elem['id'] in self.cache:
            self.cache[elem['id']].append(elem)
        else:
            self.cache[elem['id']] = [elem]

    def exit(self, elem):
        if elem['id'] in self.cache and len(self.cache[elem['id']]) > 0:
            start = self.cache[elem['id']].pop()
            del start['id']
            start['latency'] = elem['ts'] - start['ts']
            self.add(start)

    def __del__(self):
        if self.fp != None:
            self.fp.close()

