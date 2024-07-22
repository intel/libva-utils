#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import sys
import os
import json
import xml.dom.minidom

# get the first valid node
def getFirstNode(nodes):
    for i in nodes:
        if i.nodeType == i.ELEMENT_NODE:
            return i
    return None

# strip empty node
def getNodes(nodes):
    ret = []
    for i in nodes:
        if i.nodeType == i.ELEMENT_NODE:
            ret.append(i)
    return ret

# parse xml string nodes into python dict
def parseXmlString(nodes):
    ret = {}
    list = getNodes(nodes)
    for i in list:
        ret[i.getAttribute('id')] = i.getAttribute('value')
    return ret

# parse xml map into python dict
def parserXmlMap(node):
    ret = {}
    list = getNodes(node.childNodes)
    for i in list:
        key = i.getAttribute('value')
        val = i.getAttribute('message')[9:-1] # remove $(string.<>)
        ret[key] = val
    return ret

# parse xml struct into python list
def parserXmlStruct(node):
    ret = []
    list = getNodes(node.childNodes)
    for i in list:
        elem = {}
        if i.nodeName == 'struct':
            elem['struct'] = parserXmlStruct(i)
        for attr in i.attributes.items():
            elem[attr[0]] = attr[1]
        ret.append(elem)
    return ret

# parser xml element
def parseXmlElement(node):
    ret = {'type': node.nodeName}
    if node.nodeName in ['bitMap', 'valueMap']:
        ret['map'] = parserXmlMap(node)
    if node.nodeName == 'template':
        ret['struct'] = parserXmlStruct(node)
    for attr in node.attributes.items():
        ret[attr[0]] = attr[1]
    return ret

# parser provider
def parseProvider(nodes):
    ret = {}
    for attr in nodes.attributes.items():
        ret[attr[0]] = attr[1]
    list = getNodes(nodes.childNodes)
    for i in list:
        data = []
        child = getNodes(i.childNodes)
        for j in child:
            data.append(parseXmlElement(j))
        ret[i.nodeName] = data
    return ret

# format field
def formatField(node):
    ret = [node['name'], node['inType']]
    if 'map' in node:
        ret.append(node['map'])
    else:
        if 'outType' not in node:
            node['outType'] = 'xs:unsignedInt'
        ret.append(node['outType'])
    if 'length' in node:
        if node['length'].isdigit():
            ret.append(int(node['length']))
        else:
            ret.append(node['length'])
    if 'count' in node:
        if node['count'].isdigit():
            ret.append(int(node['count']))
        else:
            ret.append(node['count'])
    return ret

class manifest:

    def load(file):
        # Open XML manifest
        tree = xml.dom.minidom.parse(file)
        collection = tree.documentElement

        if collection.nodeName != 'instrumentationManifest':
           print('Error: invalid manifest')
           return

        # parse from manifest
        providerNode = getFirstNode(getFirstNode(getFirstNode(collection.childNodes).childNodes).childNodes)
        stringNode = getFirstNode(getFirstNode(getFirstNode(collection.getElementsByTagName('localization')).childNodes).childNodes)
        stringMap = parseXmlString(stringNode.childNodes)
        provider = parseProvider(providerNode)

        # build event format
        opcodes = {'win:Info':'0', 'win:Start':'1', 'win:Stop':'2', 'win:DC_Start':'3', 'win:DC_Stop':'4', 'win:Extension':'5', 'win:Reply':'6', 'win:Resume':'7', 'win:Suspend':'8', 'win:Send':'9', 'win:Receive':'240'}
        if 'opcodes' in provider:
            for i in provider['opcodes']:
                opcodes[i['name']] = i['value']

        task = {}
        for i in provider['tasks']:
            task[i['name']] = i['value']

        evt = {}
        for i in provider['events']:
            if not 'template' in i:
                i['template'] = 't_Empty'
            if not 'opcode' in i:
                i['opcode'] = 'win:Info'

            id = task[i['task']]
            if id in evt:
                evt[id]['op'][opcodes[i['opcode']]] = i['template']
            else:
                evt[id] = {'name': i['task'], 'op':{opcodes[i['opcode']]:i['template']}}

        map = {}
        for i in provider['maps']:
            elem = {}
            if i['type'] == 'bitMap':
                elem['type'] = 1 # bitmap require loop
            else:
                elem['type'] = 0
            for x, y in i['map'].items():
                elem[x] = stringMap[y]
            map[i['name']] = elem

        tmpl = {}
        for i in provider['templates']:
            field = []
            for j in i['struct']:
                if 'struct' in j:
                    subfield = []
                    for sub in j['struct']:
                        subfield.append(formatField(sub))
                    tmpl[i['tid']+j['name']] = subfield
                    if 'count' in j:
                        field.append([i['tid']+j['name'], 'struct', j['count']])
                    else:
                        field.append([i['tid']+j['name'], 'struct'])
                    continue
                field.append(formatField(j))
            tmpl[i['tid']] = field

        # sort dict by key, for tracking changes in json
        sorted_evt = {}

        for i in sorted(evt.keys(), key=lambda x: int(x)):
            sorted_op = {}
            for j in sorted(evt[i]['op'].keys(), key=lambda x: int(x)):
                sorted_op[j] = evt[i]['op'][j]
            sorted_evt[i] = {'name':evt[i]['name'], 'op':sorted_op}
        sorted_map = {}
        for i in sorted(map.keys()):
            sorted_map_elem = {'type':map[i]['type']}
            del map[i]['type']
            for v in sorted(map[i].keys(), key=lambda x: int(x,0)):
                sorted_map_elem[v] = map[i][v]
            sorted_map[i] = sorted_map_elem
        sorted_tmpl = {}
        for i in sorted(tmpl.keys()):
            sorted_tmpl[i] = tmpl[i]
        return {'name':provider['name'], 'id':provider['guid'], 'events':sorted_evt, 'maps':sorted_map, 'templates':sorted_tmpl}

