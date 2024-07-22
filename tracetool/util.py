#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

def GetCompress(val):
    flag = int(val, 16)
    compress = ''
    if flag & 1048576:
        compress += 'Media'
    if flag & 1073741824:
        compress += 'Render'
    if compress == '':
        compress = 'None'
    return compress

def GetTile(val):
    flag = int(val, 16)
    flag >>= 32
    tile = 0
    if flag & 8:
        tile = 4
    if flag & 0x10:
        tile = 64
    return tile

def GetHwProtect(val):
    flag = int(val, 16)
    if flag & 65536:
         return 1
    return 0

def GetCameraCapture(val):
    flag = int(val, 16)
    return flag & 1

def GetVpState(state, state_name, s, i):
    assert state_name in ('bEnable', 'data')
    if s in state[state_name] and i in state[state_name][s]:
        return state[state_name][s][i]
    if state_name == 'bEnable':
        return 0
    elif state_name == 'data':
        return ['0x0', '0x0', '0x0', '0x0']

def GetEnumVal(val):
    # cut substring '0xddd' from 'name (0xddd)'
    v = val.split('(')
    return v[1].split(')')[0]

def GetEnumName(val):
    # cut substring 'name' from 'name (0xddd)'
    v = val.split('(')
    return v[0].strip()

def getTs(it):
    return it['ts']
