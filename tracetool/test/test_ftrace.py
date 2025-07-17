#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import unittest
from modules.ftrace import *

class Testftrace(unittest.TestCase):

    def test_field2str(self):
        data = [ {'a':1, '1':2, 0:0, 'b':'c'},
                 {'a':[{'a':0, 'b':'a'}]},
                 {'a':[{'a':0, 'b':'a'}, {'c':0, 'd':1}]}]
        out = [ 'a:1; 1:2; 0:0; b:c; ',
                'a:0; b:a; ',
                'a:0; b:a; c:0; d:1; ']
        for i in range(0, len(data)):
            self.assertEqual(field2str(data[i]), out[i])

    def test_parseEventFormat(self):
        data = [ 'name: user_stack\nID: 12\nformat:\n\tfield:unsigned short common_type;\toffset:0;\tsize:2;\tsigned:0;\n\tfield:unsigned char common_flags;\toffset:2;\tsize:1;\tsigned:0;\n\tfield:unsigned char common_preempt_count;\toffset:3;\tsize:1;\tsigned:0;\n\tfield:int common_pid;\toffset:4;\tsize:4;\tsigned:1;\n\n\tfield:unsigned int tgid;\toffset:8;\tsize:4;\tsigned:0;\n\tfield:unsigned long caller[8];\toffset:16;\tsize:64;\tsigned:0;\n\n\n',
                 'name: print\nID: 5\nformat:\n\tfield:unsigned short common_type;\toffset:0;\tsize:2;\tsigned:0;\n\tfield:unsigned char common_flags;\toffset:2;\tsize:1;\tsigned:0;\n\tfield:unsigned char common_preempt_count;\toffset:3;\tsize:1;\tsigned:0;\n\tfield:int common_pid;\toffset:4;\tsize:4;\tsigned:1;\n\n\tfield:unsigned long ip;\toffset:8;\tsize:8;\tsigned:0;\n\tfield:char buf[];\toffset:16;\tsize:0;\tsigned:1;\n\nprint fmt: "%ps: %s", (void *)REC->ip, REC->buf\n',
                 'name: branch\nID: 9\nformat:\n\tfield:unsigned short common_type;\toffset:0;\tsize:2;\tsigned:0;\n\tfield:unsigned char common_flags;\toffset:2;\tsize:1;\tsigned:0;\n\tfield:unsigned char common_preempt_count;\toffset:3;\tsize:1;\tsigned:0;\n\tfield:int common_pid;\toffset:4;\tsize:4;\tsigned:1;\n\n\tfield:unsigned int line;\toffset:8;\tsize:4;\tsigned:0;\n\tfield:char func[30+1];\toffset:12;\tsize:31;\tsigned:1;\n\tfield:char file[20+1];\toffset:43;\tsize:21;\tsigned:1;\n\tfield:char correct;\toffset:64;\tsize:1;\tsigned:1;\n',
                 'name: tcp_probe\nID: 1426\nformat:\n\tfield:unsigned short common_type;\toffset:0;\tsize:2;\tsigned:0;\n\tfield:unsigned char common_flags;\toffset:2;\tsize:1;\tsigned:0;\n\tfield:unsigned char common_preempt_count;\toffset:3;\tsize:1;\tsigned:0;\n\tfield:int common_pid;\toffset:4;\tsize:4;\tsigned:1;\n\n\tfield:__u8 saddr[sizeof(struct sockaddr_in6)];\toffset:8;\tsize:28;\tsigned:0;\n\tfield:__u8 daddr[sizeof(struct sockaddr_in6)];\toffset:36;\tsize:28;\tsigned:0;\n\tfield:__u16 sport;\toffset:64;\tsize:2;\tsigned:0;\n\tfield:__u16 dport;\toffset:66;\tsize:2;\tsigned:0;\n']
        out = [ {12:{'sys':'ftrace', 'name':'user_stack', 'field':[['tgid',8,4,0], ['caller',16,64,264]]}},
                {5:{'sys':'ftrace', 'name':'print', 'field':[['ip',8,8,0], ['buf',16,0,256]]}},
                {9:{'sys':'ftrace', 'name':'branch', 'field':[['line',8,4,0], ['func',12,31,287], ['file',43,21,277],['correct',64,1,0]]}},
                {1426:{'sys':'ftrace', 'name':'tcp_probe', 'field':[['saddr',8,28,284], ['daddr',36,28,284], ['sport',64,2,0],['dport',66,2,0]]}}]
        for i in range(0, len(data)):
            testout = {}
            parseEventFormat(data[i], testout, 'ftrace', 8)
            self.assertEqual(testout, out[i])

    def test_parseFTrace(self):
        data = b'\xe9\x05\x01\x00\x19p\x00\x00\x00\x00\x00\x00\x06\x00\x00\x009\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x07\x00\x008\x04\x00\x00\x00\x00\x00\x00\x00\x00\x80\x07\x00\x008\x04\x00\x008\x00\t\x00plane 1A\x00\xb93\x01'
        out = {'name':'i915_flip', 'data': {'dst[4]': [0, 0, 1920, 1080], 'frame': 6, 'name': 'plane 1A', 'pipe': 0, 'scanline': 825, 'src[4]': [0, 0, 125829120, 70778880]}, 'op': 0,}
        format = [['pipe', 8, 4, 0], ['frame', 12, 4, 0], ['scanline', 16, 4, 0], ['src[4]', 20, 16, 260], ['dst[4]', 36, 16, 260], ['name', 52, 4, 1792]]
        cls = traceReader()
        self.assertEqual(out, cls.parseFTrace(data, {'name':'i915_flip'}, format))

    def test_ip2addr(self):
        sl = [2, 4, 6, 8, 10, 12]
        self.assertEqual(2, ip2addr(sl, 3))
        self.assertEqual(4, ip2addr(sl, 5))
        self.assertEqual(6, ip2addr(sl, 7))
        self.assertEqual(8, ip2addr(sl, 9))
        self.assertEqual(10, ip2addr(sl, 11))
        self.assertEqual(12, ip2addr(sl, 13))
        self.assertEqual(2, ip2addr(sl, 2))
        self.assertEqual(2, ip2addr(sl, 1))
