#
# Copyright (C) Intel Corporation. All rights reserved.
# Licensed under the MIT License.
#

import os
from ctypes import Structure, c_uint, c_ushort, c_ubyte, Array, cast, POINTER
from util import *

class traceHandler:

    def __init__(self, core):
        self.sharedCtx = core.getContext()
        self.vaDb = {}

        core.regParser(b'VATE', self.libvaHeaderParse)
        core.regHandler('Libva', 'BufferData', self.bufferDataHandler)
        core.regHandler('Libva', 'CreateConfigure', self.configHandler)
        core.regHandler('Libva', 'CreateContext', self.contextHandler)
        core.regHandler('Libva', 'CreateBuffer', self.bufferHandler)
        core.regHandler('Libva', 'CreateSurface', self.surfaceHandler)
        core.regHandler('Libva', 'RenderPicture', self.renderHandler)
        core.regHandler('Libva', None, self.commonHander)

        self.dispatch = {}
        self.profileMap = {
            'MPEG2Simple':mpeg2,
            'MPEG2Main':mpeg2,
            'MPEG4Simple':mpeg4,
            'MPEG4AdvancedSimple':mpeg4,
            'MPEG4Main':mpeg4,
            'H264Baseline':h264,
            'H264Main':h264,
            'H264High':h264,
            'VC1Simple':vc1,
            'VC1Main':vc1,
            'VC1Advanced':vc1,
            'H263Baseline':h263,
            'JPEGBaseline':jpeg,
            'H264ConstrainedBaseline':h264,
            'VP8Version0_3':vp8,
            'H264MultiviewHigh':h264,
            'H264StereoHigh':h264,
            'HEVCMain':hevc,
            'HEVCMain10':hevc,
            'VP9Profile0':vp9,
            'VP9Profile1':vp9,
            'VP9Profile2':vp9,
            'VP9Profile3':vp9,
            'HEVCMain12':hevc,
            'HEVCMain422_10':hevc,
            'HEVCMain422_12':hevc,
            'HEVCMain444':hevc,
            'HEVCMain444_10':hevc,
            'HEVCMain444_12':hevc,
            'HEVCMainSccMain':hevc,
            'HEVCMainSccMain10':hevc,
            'HEVCMainSccMain444':hevc,
            'AV1Profile0':av1,
            'AV1Profile1':av1,
            'HEVCMainSccMain444_10':hevc,
            'Protected':protected}

    def libvaHeaderParse(self, raw, evt):
        # libva trace header = 4byte tag, 2bytes size, 2 bytes event id, 4bytes type
        id = int.from_bytes(raw[4:8], 'little')
        type = int.from_bytes(raw[8:12], 'little')
        size = id & ((1<<16)-1)
        id >>= 16

        # set event name/id/etc
        evt['id'] = id
        evt['sys'] = 'Libva'
        evt['op'] = type
        return raw[12:12+size]

    def commonHander(self, evt):
        output = self.sharedCtx['UI']
        stack = self.sharedCtx['Stack']
        stat = self.sharedCtx['Stat']
        if evt['op'] == 1:
            out = {'pid':evt['pid'], 'tid':evt['tid'], 'ts':evt['ts'], 'ph': 'B', 'name':evt['name'], 'args':evt['data']}
            output.AddEvent(out)
            stack.push(evt)
            stat.enter({'id':evt['tid'], 'class':evt['sys'], 'name':evt['name'], 'ts':evt['ts']})
        elif evt['op'] == 2:
            out = {'pid':evt['pid'], 'tid':evt['tid'], 'ts':evt['ts'], 'ph': 'E', 'name':evt['name'], 'args':evt['data']}
            output.AddEvent(out)
            stack.pop(evt)
            stat.exit({'id':evt['tid'], 'ts':evt['ts']})

    def bufferDataHandler(self, evt):
        output = self.sharedCtx['UI']
        stack = self.sharedCtx['Stack']
        data = evt['data']
        cur = stack.current(evt['pid'], evt['tid'])
        if cur == None or cur['name'] != 'RenderPicture':
            return -1
        id = cur['data']['ContextId']
        if evt['pid'] not in self.vaDb or id not in self.vaDb[evt['pid']]['ctx']:
            return -1
        ctx = self.vaDb[evt['pid']]['ctx'][id]
        # setup profile for buffer parser, fetch from context/config event
        if ctx['ConfigId'] not in self.vaDb[evt['pid']]['cfg']:
            return -1
        cfg = self.vaDb[evt['pid']]['cfg'][ctx['ConfigId']]
        profile = cfg['Profile']
        out = None
        if evt['op'] == 0:
            data['Profile'] = profile
            out = self.parseBuffer(data)
        else:
            if evt['op'] == 1:
                data['Profile'] = profile
                data['BufData'] = ''
                cur['buffer'] = data # store data in stack
            if evt['op'] == 2:
                out = self.parseBuffer(cur['buffer'])
                del cur['buffer']
            if evt['op'] == 3:
                cur['buffer']['BufData'] += data['BufData'] # append data
        if out != None:
            name = out['Profile'] + ' : ' + out['BufferType']
            blk = {'pid':evt['pid'], 'tid':evt['tid'], 'ts':evt['ts'], 'ph': 'i', 'name':name, 'args':out}
            output.AddEvent(blk)
        return -1 # skip common handler

    # separate va instance by process
    def getInstance(self, pid):
        if pid not in self.vaDb:
            self.vaDb[pid] = {'ctx':{}, 'cfg':{}, 'buf':{}, 'surf':{}}
        return self.vaDb[pid]

    def configHandler(self, evt):
        output = self.sharedCtx['UI']
        stack = self.sharedCtx['Stack']
        va = self.getInstance(evt['pid'])

        if evt['op'] == 1:
            va['cfg']['cur'] = evt['data']
        elif evt['op'] == 2:
            data = evt['data']
            if 'Success ' in data['vaStatus'] and 'cur' in va['cfg']:
                va['cfg'][data['ConfigId']] = va['cfg']['cur']
                del va['cfg']['cur']

    def contextHandler(self, evt):
        output = self.sharedCtx['UI']
        stack = self.sharedCtx['Stack']
        va = self.getInstance(evt['pid'])

        if evt['op'] == 1:
            va['ctx']['cur'] = evt['data']
        elif evt['op'] == 2:
            data = evt['data']
            if 'Success ' in data['vaStatus'] and 'cur' in va['ctx']:
                va['ctx'][data['ContextId']] = va['ctx']['cur']
                del va['ctx']['cur']

    def bufferHandler(self, evt):
        output = self.sharedCtx['UI']
        stack = self.sharedCtx['Stack']
        va = self.getInstance(evt['pid'])

        if evt['op'] == 1:
            va['buf']['cur'] = evt['data']
        elif evt['op'] == 2:
            data = evt['data']
            if 'Success ' in data['vaStatus'] and 'cur' in va['buf']:
                va['buf'][data['BufferId']] = va['buf']['cur']
                del va['buf']['cur']

    def surfaceHandler(self, evt):
        output = self.sharedCtx['UI']
        stack = self.sharedCtx['Stack']
        va = self.getInstance(evt['pid'])

        if evt['op'] == 1:
            va['surf']['cur'] = evt['data']
        elif evt['op'] == 2:
            data = evt['data']
            if 'Success ' in data['vaStatus'] and 'cur' in va['surf']:
                for s in data['VASurfaceID']:
                    va['surf'][s] = va['surf']['cur']
                del va['surf']['cur']

    def renderHandler(self, evt):
        if evt['op'] != 1:
            return
        data = evt['data']
        va = self.getInstance(evt['pid'])
        # fill more info from buffer id
        info = []
        for b in data['Buffers']:
            buf = 'BufferId: ' + str(b)
            if b in va['buf']:
                buf += ' BufferType: ' + GetEnumName(va['buf'][b]['BufferType'])
            info.append(buf)
        data['Buffers'] = info

    def parseBuffer(self, data):
        raw = bytes.fromhex(data['BufData'])
        profile = GetEnumName(data['Profile'])
        type = GetEnumName(data['BufferType'])
        cnt = data['MemSize']//data['Size']
        ret = {'Profile':profile, 'BufferType':type, 'Count':cnt, 'Size':data['Size'], 'Details':[]}
        if profile not in self.profileMap:
            print('Warning! non supported profile {s}'.format(profile))
            return None
        if profile not in self.dispatch:
            self.dispatch[profile] = self.profileMap[profile]()
        pos = 0
        for i in range(cnt):
            out = self.dispatch[profile].parse(type, raw[pos:(i+1)*data['Size']])
            if out == None:
                print('Warning! non supported buffer type {s}'.format(type))
                return None
            ret['Details'].append(out)
            pos += data['Size']
        return ret

class buffer(Structure):
    def formatArray(self, val):
        ret = ''
        if val._type_ is c_ubyte:
            ptr = cast(val, POINTER(c_ubyte))
        elif val._type_ is c_ushort:
            ptr = cast(val, POINTER(c_ushort))
        elif val._type_ is c_uint:
            ptr = cast(val, POINTER(c_uint))
        else:
            print('Warning! non supported array type {}'.format(val._type_))
            return ret

        for i in range(len(val)):
            ret += '{:02x} '.format(ptr[i])
            if i & 15 == 15:
                ret += '\n'
        return ret

    def getDict(self):
        ret = {}
        for field in self._fields_:
            val = getattr(self, field[0])
            if isinstance(val, buffer):
                ret[field[0]] = val.getDict()
                continue
            if isinstance(val, Array):
                # buffer array
                if isinstance(val[0], buffer):
                    out = []
                    for i in val:
                        out.append(i.getDict())
                    ret[field[0]] = out
                    continue
                # 2d array
                if isinstance(val[0], Array):
                    out = []
                    for i in val:
                        out.append(self.formatArray(i))
                    ret[field[0]] = out
                    continue
                ret[field[0]] = self.formatArray(val)
                continue
            ret[field[0]] = val
        return ret

class base:
    def parse(self, type, raw):
        if type in self.dispatch:
            return self.dispatch[type](raw)
        return self.formatData(raw)

    def formatData(self, raw):
        ret = ''
        i = 0
        while i <len(raw):
            ret += '{:02x} '.format(raw[i])
            i += 1
            if i & 15 == 15:
                ret += '\n'
        return ret

class mpeg2(base):
    def __init__(self):
        self.dispatch = {
            'PictureParameter': self.picture,
            'IQMatrix': self.IQMatrix,
            'SliceParameter': self.slice}

    def picture(self, raw):
        class PictureParameter(buffer):
            _pack_ = 1
            _fields_ = [
                ('horizontal_size', c_ushort),
                ('vertical_size', c_ushort),
                ('forward_reference_picture', c_uint),
                ('backward_reference_picture', c_uint),
                ('picture_coding_type', c_uint),
                ('f_code', c_uint),
                ('intra_dc_precision', c_uint, 2),
                ('picture_structure', c_uint, 2),
                ('top_field_firt', c_uint, 1),
                ('frame_pred_frame_dct', c_uint, 1),
                ('concealment_motion_vectors', c_uint, 1),
                ('q_scale_type', c_uint, 1),
                ('intra_vlc_format', c_uint, 1),
                ('alternate_scan', c_uint, 1),
                ('repeat_first_field', c_uint, 1),
                ('progressive_frame', c_uint, 1),
                ('is_first_field', c_uint, 1)]
        param = PictureParameter.from_buffer_copy(raw)
        return param.getDict()

    def slice(self, raw):
        class SliceParameter(buffer):
            _pack_ = 1
            _fields_ = [
                ('slice_data_size', c_uint),
                ('slice_data_offset', c_uint),
                ('slice_data_flag', c_uint),
                ('macroblock_offset', c_uint),
                ('slice_horizontal_position', c_uint),
                ('slice_vertical_position', c_uint),
                ('quantiser_scale_code', c_uint),
                ('intra_slice_flag', c_uint)]
        param = SliceParameter.from_buffer_copy(raw)
        return param.getDict()

    def IQMatrix(self, raw):
        class IQMatrix(buffer):
            _pack_ = 1
            _fields_ = [
                ('load_intra_quantiser_matrix', c_uint),
                ('load_non_intra_quantiser_matrix', c_uint),
                ('load_chroma_intra_quantiser_matrix', c_uint),
                ('load_chroma_non_intra_quantiser_matrix', c_uint),
                ('intra_quantiser_matrix', c_ubyte*64),
                ('non_intra_quantiser_matrix', c_ubyte*64),
                ('chroma_intra_quantiser_matrix', c_ubyte*64),
                ('chroma_non_intra_quantiser_matrix', c_ubyte*64)]
        param = IQMatrix.from_buffer_copy(raw)
        return param.getDict()

class mpeg4(base):
    pass

class PictureH264(buffer):
    _pack_ = 1
    _fields_ = [
        ('picture_id', c_uint),
        ('frame_idx', c_uint),
        ('flags', c_uint),
        ('TopFieldOrderCnt', c_uint),
        ('BottomFieldOrderCnt', c_uint),
        ('va_reserved', c_uint*4)]

class h264(base):

    def __init__(self):
        self.dispatch = {
            'PictureParameter': self.PictureParameter,
            'IQMatrix': self.IQMatrix,
            'SliceParameter': self.SliceParameter,
            'EncSequenceParameter': self.EncSequenceParameter,
            'EncSliceParameter': self.EncSliceParameter,
            'EncPictureParameter': self.EncPictureParameter}

    def PictureParameter(self, raw):
        class PictureParameter(buffer):
            _pack_ = 1
            _fields_ = [
                ('CurrPic', PictureH264),
                ('ReferenceFrames', PictureH264*16),
                ('picture_width_in_mbs_minus1', c_ushort),
                ('picture_height_in_mbs_minus1', c_ushort),
                ('bit_depth_luma_minus8', c_ubyte),
                ('bit_depth_chroma_minus8', c_ubyte),
                ('num_ref_frames', c_ubyte),
                ('chroma_format_idc', c_uint, 2),
                ('residual_colour_transform_flag', c_uint, 1),
                ('gaps_in_frame_num_value_allowed_flag', c_uint, 1),
                ('frame_mbs_only_flag', c_uint, 1),
                ('mb_adaptive_frame_field_flag', c_uint, 1),
                ('direct_8x8_inference_flag', c_uint, 1),
                ('MinLumaBiPredSize8x8', c_uint, 1),
                ('log2_max_frame_num_minus4', c_uint, 4),
                ('pic_order_cnt_type', c_uint, 2),
                ('log2_max_pic_order_cnt_lsb_minus4', c_uint, 4),
                ('delta_pic_order_always_zero_flag', c_uint, 1),
                ('num_slice_groups_minus1', c_ubyte),
                ('slice_group_map_type', c_ubyte),
                ('slice_group_change_rate_minus1', c_ushort),
                ('pic_init_qp_minus26', c_ubyte),
                ('pic_init_qs_minus26', c_ubyte),
                ('chroma_qp_index_offset', c_ubyte),
                ('second_chroma_qp_index_offset', c_ubyte),
                ('entropy_coding_mode_flag', c_uint, 1),
                ('weighted_pred_flag', c_uint, 1),
                ('weighted_bipred_idc', c_uint, 2),
                ('transform_8x8_mode_flag', c_uint, 1),
                ('field_pic_flag', c_uint, 1),
                ('constrained_intra_pred_flag', c_uint, 1),
                ('pic_order_present_flag', c_uint, 1),
                ('deblocking_filter_control_present_flag', c_uint, 1),
                ('redundant_pic_cnt_present_flag', c_uint, 1),
                ('reference_pic_flag', c_uint, 1),
                ('frame_num', c_ushort)]
        param = PictureParameter.from_buffer_copy(raw)
        return param.getDict()

    def IQMatrix(self, raw):
        class IQMatrix(buffer):
            _pack_ = 1
            _fields_ = [
                ('ScalingList4x4', (c_ubyte*16)*4),
                ('ScalingList8x8', (c_ubyte*64)*2)]
        param = IQMatrix.from_buffer_copy(raw)
        return param.getDict()

    def SliceParameter(self, raw):
        class SliceParameter(buffer):
            _pack_ = 1
            _fields_ = [
                ('slice_data_size', c_uint),
                ('slice_data_offset', c_uint),
                ('slice_data_flag', c_uint),
                ('slice_data_bit_offset', c_ushort),
                ('first_mb_in_slice', c_ushort),
                ('slice_type', c_ubyte),
                ('direct_spatial_mv_pred_flag', c_ubyte),
                ('num_ref_idx_l0_active_minus1', c_ubyte),
                ('num_ref_idx_l1_active_minus1', c_ubyte),
                ('cabac_init_idc', c_ubyte),
                ('slice_qp_delta', c_ubyte),
                ('disable_deblocking_filter_idc', c_ubyte),
                ('slice_alpha_c0_offset_div2', c_ubyte),
                ('slice_beta_offset_div2', c_ubyte),
                ('RefPicList0', PictureH264*32),
                ('RefPicList1', PictureH264*32),
                ('luma_log2_weight_denom', c_ubyte),
                ('chroma_log2_weight_denom', c_ubyte),
                ('luma_weight_l0_flag', c_ubyte),
                ('luma_weight_l0', c_ushort*32),
                ('luma_offset_l0', c_ushort*32),
                ('chroma_weight_l0_flag', c_ubyte),
                ('chroma_weight_l0', (c_ushort*32)*2),
                ('chroma_offset_l0', (c_ushort*32)*2),
                ('luma_weight_l1_flag', c_ubyte),
                ('luma_weight_l1', c_ushort*32),
                ('luma_offset_l1', c_ushort*32),
                ('chroma_weight_l1_flag', c_ubyte),
                ('chroma_weight_l1', (c_ushort*32)*2),
                ('chroma_offset_l1', (c_ushort*32)*2)]
        param = SliceParameter.from_buffer_copy(raw)
        return param.getDict()

    def EncSliceParameter(self, raw):
        class EncSliceParameter(buffer):
            _pack_ = 1
            _fields_ = [
                ('macroblock_address', c_uint),
                ('num_macroblocks', c_uint),
                ('macroblock_info', c_uint),
                ('slice_type', c_ubyte),
                ('pic_parameter_set_id', c_ubyte),
                ('idr_pic_id', c_ushort),
                ('pic_order_cnt_lsb', c_ushort),
                ('delta_pic_order_cnt_bottom', c_uint),
                ('delta_pic_order_cnt', c_uint*2),
                ('direct_spatial_mv_pred_flag', c_ubyte),
                ('num_ref_idx_active_override_flag', c_ubyte),
                ('num_ref_idx_l0_active_minus1', c_ubyte),
                ('num_ref_idx_l1_active_minus1', c_ubyte),
                ('RefPicList0', PictureH264*32),
                ('RefPicList1', PictureH264*32),
                ('luma_log2_weight_denom', c_ubyte),
                ('chroma_log2_weight_denom', c_ubyte),
                ('luma_weight_l0_flag', c_ubyte),
                ('luma_weight_l0', c_ushort*32),
                ('luma_offset_l0', c_ushort*32),
                ('chroma_weight_l0_flag', c_ubyte),
                ('chroma_weight_l0', (c_ushort*32)*2),
                ('chroma_offset_l0', (c_ushort*32)*2),
                ('luma_weight_l1_flag', c_ubyte),
                ('luma_weight_l1', c_ushort*32),
                ('luma_offset_l1', c_ushort*32),
                ('chroma_weight_l1_flag', c_ubyte),
                ('chroma_weight_l1', (c_ushort*32)*2),
                ('chroma_offset_l1', (c_ushort*32)*2),
                ('cabac_init_idc', c_ubyte),
                ('slice_qp_delta', c_ubyte),
                ('disable_deblocking_filter_idc', c_ubyte),
                ('slice_alpha_c0_offset_div2', c_ubyte),
                ('slice_beta_offset_div2', c_ubyte)]
        param = EncSliceParameter.from_buffer_copy(raw)
        return param.getDict()

    def EncSequenceParameter(self, raw):
        class EncSequenceParameter(buffer):
            _pack_ = 1
            _fields_ = [
                ('seq_parameter_set_id', c_ubyte),
                ('level_idc', c_ubyte),
                ('intra_period', c_uint),
                ('intra_idr_period', c_uint),
                ('ip_period', c_uint),
                ('bits_per_second', c_uint),
                ('max_num_ref_frames', c_uint),
                ('picture_width_in_mbs', c_ushort),
                ('picture_height_in_mbs', c_ushort),
                ('chroma_format_idc', c_uint, 2),
                ('frame_mbs_only_flag', c_uint, 1),
                ('mb_adaptive_frame_field_flag', c_uint, 1),
                ('seq_scaling_matrix_present_flag', c_uint, 1),
                ('direct_8x8_inference_flag', c_uint, 1),
                ('log2_max_frame_num_minus4', c_uint, 4),
                ('pic_order_cnt_type', c_uint, 2),
                ('log2_max_pic_order_cnt_lsb_minus4', c_uint, 4),
                ('delta_pic_order_always_zero_flag', c_uint, 1),
                ('bit_depth_luma_minus8', c_ubyte),
                ('bit_depth_chroma_minus8', c_ubyte),
                ('num_ref_frames_in_pic_order_cnt_cycle', c_ubyte),
                ('offset_for_non_ref_pic', c_uint),
                ('offset_for_top_to_bottom_field', c_uint),
                ('offset_for_ref_frame', c_uint*256),
                ('frame_cropping_flag', c_ubyte),
                ('frame_crop_left_offset', c_uint),
                ('frame_crop_right_offset', c_uint),
                ('frame_crop_top_offset', c_uint),
                ('frame_crop_bottom_offset', c_uint),
                ('vui_parameters_present_flag', c_ubyte),
                ('aspect_ratio_info_present_flag', c_uint, 1),
                ('timing_info_present_flag', c_uint, 1),
                ('bitstream_restriction_flag', c_uint, 1),
                ('log2_max_mv_length_horizontal', c_uint, 5),
                ('log2_max_mv_length_vertical', c_uint, 5),
                ('fixed_frame_rate_flag', c_uint, 1),
                ('low_delay_hrd_flag', c_uint, 1),
                ('motion_vectors_over_pic_boundaries_flag', c_uint, 1),
                ('aspect_ratio_idc', c_ubyte),
                ('sar_width', c_uint),
                ('sar_height', c_uint),
                ('num_units_in_tick', c_uint),
                ('time_scale', c_uint)]
        param = EncSequenceParameter.from_buffer_copy(raw)
        return param.getDict()

    def EncPictureParameter(self, raw):
        class EncPictureParameter(buffer):
            _pack_ = 1
            _fields_ = [
                ('CurrPic', PictureH264),
                ('ReferenceFrames', PictureH264*16),
                ('coded_buf', c_uint),
                ('pic_parameter_set_id', c_ubyte),
                ('seq_parameter_set_id', c_ubyte),
                ('last_picture', c_ubyte),
                ('frame_num', c_ushort),
                ('pic_init_qp', c_ubyte),
                ('num_ref_idx_l0_active_minus1', c_ubyte),
                ('num_ref_idx_l1_active_minus1', c_ubyte),
                ('chroma_qp_index_offset', c_ubyte),
                ('second_chroma_qp_index_offset', c_ubyte),
                ('idr_pic_flag', c_uint, 1),
                ('reference_pic_flag', c_uint, 2),
                ('entropy_coding_mode_flag', c_uint, 1),
                ('weighted_pred_flag', c_uint, 1),
                ('weighted_bipred_idc', c_uint, 2),
                ('constrained_intra_pred_flag', c_uint, 1),
                ('transform_8x8_mode_flag', c_uint, 1),
                ('deblocking_filter_control_present_flag', c_uint, 4),
                ('redundant_pic_cnt_present_flag', c_uint, 1),
                ('pic_order_present_flag', c_uint, 1),
                ('pic_scaling_matrix_present_flag', c_uint, 1)]
        param = EncPictureParameter.from_buffer_copy(raw)
        return param.getDict()

class vc1(base):
    pass

class h263(base):
    pass

class jpeg(base):
    pass

class vp8(base):
    pass

class hevc(base):
    pass

class vp9(base):
    pass

class av1(base):
    pass

class protected(base):
    pass
