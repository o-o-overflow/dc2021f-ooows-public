#!/usr/bin/env python3
import argparse
import collections
import itertools
import logging
import os
import re
import struct
import sys
import typing

import bitstruct
import lark

l = logging.getLogger("assembler")

GRAMMAR_FILE = os.path.dirname(os.path.realpath(__file__)) + '/grammar.lark'

RELATIVE = 0
ABSOLUTE = 1

NO_ROT = 0
ANOTHER_NO_ROT = 1
LEFT_EIGHT = 2
LEFT_SIXTEEN = 3

NO_TOKEN = 0
CTX_SWAP = 1
SIG_DONE = 2

READ = 0
WRITE = 1

SHIFT_LEFT = 0
SHIFT_RIGHT = 1

PLUS = 0
MINUS = 1
BACKWARDS_MINUS = 2
SECOND = 3
BIT_NOT_SECOND = 4
AND = 5
OR = 6
XOR = 7
PLUS_CARRY = 8
ALU_SHIFT_LEFT = 9
ALU_SHIFT_RIGHT = 10
PLUS_IF_SIGN = 11
PLUS_FOUR = 12
PLUS_EIGHT = 13
PLUS_SIXTEEN = 14

ALU = 0
ALU_SHF = 1
DBL_SHF = 2
BR = 3
BR_EQ = 4
BR_NEQ = 5
BR_LESS = 6
BR_LESS_EQ = 7
BR_GREATER = 8
BR_GREATER_EQ = 9
BR_EQ_COUNT = 10
BR_NEQ_COUNT = 11
BR_BSET = 12
BR_BCLR = 13
BR_EQ_BYTE = 14
BR_NEQ_BYTE = 15
BR_EQ_CTX = 16
BR_NEQ_CTX = 17
BR_INP_STATE = 18
BR_NOT_SIGNAL = 19
RTN = 20
CSR = 21
FAST_WR = 22
LOCAL_CSR_RD = 23
RX_PKT = 24
R_FIFO_RD = 25
SCRATCH = 26
RAM = 27
T_FIFO_WR = 28
IMMED = 29
IMMED_B0 = 30
IMMED_B1 = 31
IMMED_B2 = 32
IMMED_B3 = 33
IMMED_W0 = 34
IMMED_W1 = 35
LD_FIELD = 36
LD_FIELD_W_CLR = 37
LOAD_ADDR = 38
CTX_ARB = 39
NOP = 40
HASH1_48 = 41
HASH2_48 = 42
HASH3_48 = 43
HASH1_64 = 44
HASH2_64 = 45
HASH3_64 = 46

INSTRUCTION_BYTES = 5

Register = collections.namedtuple('Register', ['name', 'type', 'number'])
InstructionType = collections.namedtuple('InstructionType', ['name', 'to_binary', 'to_inst'])
Opcode = collections.namedtuple('Opcode', ['name', 'num', 'instruction_type'])
AluType = collections.namedtuple('AluType', ['repr', 'num'])

ImmediateInstruction = collections.namedtuple('ImmediateInstruction',
                                              ['opcode', 'dst', 'ival', 'rot', 'label'])
AluInstruction = collections.namedtuple('AluInstruction',
                                        ['opcode', 'dst', 'src_1', 'type', 'src_2', 'shift', 'num_shift', 'label'])

MemoryInstruction = collections.namedtuple('MemoryInstruction',
                                           ['opcode', 'direction', 'xfer', 'addr_1', 'addr_2', 'count', 'token', 'label'])
BranchInstruction = collections.namedtuple('BranchInstruction',
                                           ['opcode', 'src_1', 'src_2', 'target', 'label'])
LoadInstruction = collections.namedtuple('LoadInstruction',
                                         ['opcode', 'dst', 'mask', 'src', 'rot', 'label'])
FifoInstruction = collections.namedtuple('FifoInstruction',
                                         ['opcode', 'size', 'addr_1', 'addr_2', 'token', 'label'])
NoArgInstruction = collections.namedtuple('NoArgInstruction',
                                          ['opcode', 'label'])
CsrInstruction = collections.namedtuple('CsrInstruction', ['opcode', 'dst', 'csr_num', 'label'])




_INSTRUCTION_TYPES = [
    InstructionType('immediate',
                    lambda inst: bitstruct.pack('<u6u1u8u16u2p7',
                                                inst.opcode,
                                                inst.dst.type,
                                                inst.dst.number,
                                                inst.ival,
                                                inst.rot,
                                                ),
                    lambda op, arg_list, token=None: ImmediateInstruction(opcode=op.num,
                                                                          dst=arg_list[0],
                                                                          ival=arg_list[1],
                                                                          rot=arg_list[2] if len(arg_list) == 3 else NO_ROT,
                                                                          label=None)),
    InstructionType('alu',
                    lambda inst: bitstruct.pack('<u6u1u8u1u8u4u1u8u1u2',
                                                inst.opcode,
                                                inst.dst.type,
                                                inst.dst.number,
                                                inst.src_1.type,
                                                inst.src_1.number,
                                                inst.type,
                                                inst.src_2.type,
                                                inst.src_2.number,
                                                inst.shift,
                                                inst.num_shift),
                    lambda op, arg_list, token=None: AluInstruction(opcode=op.num,
                                                                    dst=arg_list[0],
                                                                    src_1=arg_list[1],
                                                                    type=arg_list[2],
                                                                    src_2=arg_list[3],
                                                                    shift=arg_list[4] if len(arg_list) >= 5 else SHIFT_LEFT,
                                                                    num_shift=arg_list[5] if len(arg_list) >= 6 else 0,
                                                                    label=None)),

    InstructionType('memory',
                    lambda inst: bitstruct.pack('<u6u1u1u8u1u8u1u8u4u2',
                                                inst.opcode,
                                                inst.direction,
                                                inst.xfer.type,
                                                inst.xfer.number,
                                                inst.addr_1.type,
                                                inst.addr_1.number,
                                                inst.addr_2.type,
                                                inst.addr_2.number,
                                                inst.count,
                                                inst.token if inst.token else NO_TOKEN),

                    lambda op, arg_list, token=None: MemoryInstruction(opcode=op.num,
                                                                       direction=arg_list[0],
                                                                       xfer=arg_list[1],
                                                                       addr_1=arg_list[2],
                                                                       addr_2=arg_list[3],
                                                                       count=arg_list[4],
                                                                       token=token,
                                                                       label=None)),
    InstructionType('branch',
                    lambda inst: bitstruct.pack('<u6u1u8u1u8u12p4',
                                                inst.opcode,
                                                inst.src_1.type,
                                                inst.src_1.number,
                                                inst.src_2.type,
                                                inst.src_2.number,
                                                inst.target),
                    lambda op, arg_list, token=None: BranchInstruction(opcode=op.num,
                                                                       src_1=arg_list[0] if len(arg_list) > 1 else Register('reg_127', ABSOLUTE, 127),
                                                                       src_2=arg_list[1] if len(arg_list) > 1 else Register('reg_127', ABSOLUTE, 127),
                                                                       target=arg_list[2] if len(arg_list) > 1 else arg_list[0],
                                                                       label=None)),

    InstructionType('load',
                    lambda inst: bitstruct.pack('<u6u1u8u4u1u8u2p10',
                                                inst.opcode,
                                                inst.dst.type,
                                                inst.dst.number,
                                                inst.mask,
                                                inst.src.type,
                                                inst.src.number,
                                                inst.rot),
                    lambda op, arg_list, token=None: LoadInstruction(opcode=op.num,
                                                                     dst=arg_list[0],
                                                                     mask=arg_list[1],
                                                                     src=arg_list[2],
                                                                     rot=arg_list[3] if len(arg_list) == 4 else NO_ROT,
                                                                     label=None)),
    InstructionType('fifo',
                    lambda inst: bitstruct.pack('<u6u1u8u1u8u1u8u2p5',
                                                inst.opcode,
                                                inst.size.type,
                                                inst.size.number,
                                                inst.addr_1.type,
                                                inst.addr_1.number,
                                                inst.addr_2.type,
                                                inst.addr_2.number,
                                                inst.token if inst.token else NO_TOKEN),
                    lambda op, arg_list, token=None: FifoInstruction(opcode=op.num,
                                                                     size=arg_list[0],
                                                                     addr_1=arg_list[1],
                                                                     addr_2=arg_list[2],
                                                                     token=token,
                                                                     label=None)),
    InstructionType('no-arg',
                    lambda inst: bitstruct.pack('<u6p34',
                                                inst.opcode),
                    lambda op, arg_list, token=None: NoArgInstruction(opcode=op.num,
                                                                      label=None)),
    InstructionType('csr',
                    lambda inst: bitstruct.pack('<u6u1u8u16p9',
                                                inst.opcode,
                                                inst.dst.type,
                                                inst.dst.number,
                                                inst.csr_num),
                    lambda op, arg_list, token=None: CsrInstruction(opcode=op.num,
                                                                    dst=arg_list[0],
                                                                    csr_num=arg_list[1],
                                                                    label=None)),
]

INSTRUCTION_TYPES = { i.name: i for i in _INSTRUCTION_TYPES }

_OPCODES = [
    Opcode('ALU', ALU, INSTRUCTION_TYPES['alu']),
    Opcode('ALU_SHF', ALU_SHF, INSTRUCTION_TYPES['alu']),
    Opcode('DBL_SHF', DBL_SHF, INSTRUCTION_TYPES['alu']),
    Opcode('IMMED', IMMED, INSTRUCTION_TYPES['immediate']),
    Opcode('IMMED_B0', IMMED_B0, INSTRUCTION_TYPES['immediate']),
    Opcode('IMMED_B1', IMMED_B1, INSTRUCTION_TYPES['immediate']),
    Opcode('IMMED_B2', IMMED_B2, INSTRUCTION_TYPES['immediate']),
    Opcode('IMMED_B3', IMMED_B3, INSTRUCTION_TYPES['immediate']),
    Opcode('IMMED_W0', IMMED_W0, INSTRUCTION_TYPES['immediate']),
    Opcode('IMMED_W1', IMMED_W1, INSTRUCTION_TYPES['immediate']),
    Opcode('SCRATCH', SCRATCH, INSTRUCTION_TYPES['memory']),
    Opcode('RAM', RAM, INSTRUCTION_TYPES['memory']),
    Opcode('BR', BR, INSTRUCTION_TYPES['branch']),
    Opcode('BR=', BR_EQ, INSTRUCTION_TYPES['branch']),
    Opcode('BR!=', BR_NEQ, INSTRUCTION_TYPES['branch']),
    Opcode('BR<', BR_LESS, INSTRUCTION_TYPES['branch']),
    Opcode('BR<=', BR_LESS_EQ, INSTRUCTION_TYPES['branch']),
    Opcode('BR>', BR_GREATER, INSTRUCTION_TYPES['branch']),
    Opcode('BR>=', BR_GREATER_EQ, INSTRUCTION_TYPES['branch']),
    Opcode('BR=COUNT', BR_EQ_COUNT, INSTRUCTION_TYPES['branch']),
    Opcode('BR!=COUNT', BR_NEQ_COUNT, INSTRUCTION_TYPES['branch']),
    Opcode('BR_BSET', BR_BSET, INSTRUCTION_TYPES['branch']),
    Opcode('BR_BCLR', BR_BCLR, INSTRUCTION_TYPES['branch']),
    Opcode('BR=BYTE', BR_EQ_BYTE, INSTRUCTION_TYPES['branch']),
    Opcode('BR!=BYTE', BR_NEQ_BYTE, INSTRUCTION_TYPES['branch']),
    Opcode('BR=CTX', BR_EQ_CTX, INSTRUCTION_TYPES['branch']),
    Opcode('BR!=CTX', BR_NEQ_CTX, INSTRUCTION_TYPES['branch']),
    Opcode('BR_INP_STATE', BR_INP_STATE, INSTRUCTION_TYPES['branch']),
    Opcode('BR!SIGNAL', BR_NOT_SIGNAL, INSTRUCTION_TYPES['branch']),
    Opcode('RTN', RTN, INSTRUCTION_TYPES['branch']),
    Opcode('LD_FIELD', LD_FIELD, INSTRUCTION_TYPES['load']),
    Opcode('LD_FIELD_W_CLR', LD_FIELD_W_CLR, INSTRUCTION_TYPES['load']),
    Opcode('CSR', CSR, INSTRUCTION_TYPES['csr']),
    Opcode('RX_PKT', RX_PKT, INSTRUCTION_TYPES['fifo']),
    Opcode('R_FIFO_RD', R_FIFO_RD, INSTRUCTION_TYPES['fifo']),
    Opcode('T_FIFO_WR', T_FIFO_WR, INSTRUCTION_TYPES['fifo']),
    Opcode('NOP', NOP, INSTRUCTION_TYPES['no-arg']),
    Opcode('CTX_ARB', CTX_ARB, INSTRUCTION_TYPES['no-arg']),
]

OPCODES = { o.name: o for o in _OPCODES }

_ALU_TYPES = [
    AluType(repr="+", num=PLUS),
    AluType(repr="-", num=MINUS),
    AluType(repr="B-A", num=BACKWARDS_MINUS),
    AluType(repr="B", num=SECOND),
    AluType(repr="~B", num=BIT_NOT_SECOND),
    AluType(repr="&", num=AND),
    AluType(repr="|", num=OR),
    AluType(repr="^", num=XOR),
    AluType(repr="+CARRY", num=PLUS_CARRY),
    AluType(repr="<<", num=ALU_SHIFT_LEFT),
    AluType(repr=">>", num=ALU_SHIFT_RIGHT),
    AluType(repr="+IF-SIGNED", num=PLUS_IF_SIGN),
    AluType(repr="+4", num=PLUS_FOUR),
    AluType(repr="+8", num=PLUS_EIGHT),
    AluType(repr="+16", num=PLUS_SIXTEEN),
]
ALU_TYPES = { t.repr: t for t in _ALU_TYPES }

TOKENS = {
    'CTX_SWAP': CTX_SWAP,
    'SIG_DONE': SIG_DONE,
}

DIRECTIONS = {
    'READ': READ,
    'WRITE': WRITE,
}


class GenerateOpcodes(lark.visitors.Transformer):
    labels = dict()
    next_label = None
    instruction_num = 0

    def __init__(self):
        self.registers = {
            'reg_0': Register('reg_0', RELATIVE, 0),
            'reg_1': Register('reg_1', RELATIVE, 1),
            'reg_2': Register('reg_2', RELATIVE, 2),
            'reg_3': Register('reg_3', RELATIVE, 3),
            'reg_4': Register('reg_4', RELATIVE, 4),
            'reg_5': Register('reg_5', RELATIVE, 5),
            'reg_6': Register('reg_6', RELATIVE, 6),
            'reg_7': Register('reg_7', RELATIVE, 7),
            'reg_8': Register('reg_8', RELATIVE, 8),
            'reg_9': Register('reg_9', RELATIVE, 9),
            'reg_10': Register('reg_10', RELATIVE, 10),
            'reg_11': Register('reg_11', RELATIVE, 11),
            'reg_12': Register('reg_12', RELATIVE, 12),
            'reg_13': Register('reg_13', RELATIVE, 13),
            'reg_14': Register('reg_14', RELATIVE, 14),
            'reg_15': Register('reg_15', RELATIVE, 15),
            'reg_16': Register('reg_16', RELATIVE, 16),
            'reg_17': Register('reg_17', RELATIVE, 17),
            'reg_18': Register('reg_18', RELATIVE, 18),
            'reg_19': Register('reg_19', RELATIVE, 19),
            'reg_20': Register('reg_20', RELATIVE, 20),
            'reg_21': Register('reg_21', RELATIVE, 21),
            'reg_22': Register('reg_22', RELATIVE, 22),
            'reg_23': Register('reg_23', RELATIVE, 23),
            'reg_24': Register('reg_24', RELATIVE, 24),
            'reg_25': Register('reg_25', RELATIVE, 25),
            'reg_26': Register('reg_26', RELATIVE, 26),
            'reg_27': Register('reg_27', RELATIVE, 27),
            'reg_28': Register('reg_28', RELATIVE, 28),
            'reg_29': Register('reg_29', RELATIVE, 29),
            'reg_30': Register('reg_30', RELATIVE, 30),
            'reg_31': Register('reg_31', RELATIVE, 31),
            'write_0': Register('write_0', RELATIVE, 32),
            'write_1': Register('write_1', RELATIVE, 33),
            'write_2': Register('write_2', RELATIVE, 34),
            'write_3': Register('write_3', RELATIVE, 35),
            'write_4': Register('write_4', RELATIVE, 36),
            'write_5': Register('write_5', RELATIVE, 37),
            'write_6': Register('write_6', RELATIVE, 38),
            'write_7': Register('write_7', RELATIVE, 39),
            'write_8': Register('write_8', RELATIVE, 40),
            'write_9': Register('write_9', RELATIVE, 41),
            'write_10': Register('write_10', RELATIVE, 42),
            'write_11': Register('write_11', RELATIVE, 43),
            'write_12': Register('write_12', RELATIVE, 44),
            'write_13': Register('write_13', RELATIVE, 45),
            'write_14': Register('write_14', RELATIVE, 46),
            'write_15': Register('write_15', RELATIVE, 47),
            'read_0': Register('read_0', RELATIVE, 48),
            'read_1': Register('read_1', RELATIVE, 49),
            'read_2': Register('read_2', RELATIVE, 50),
            'read_3': Register('read_3', RELATIVE, 51),
            'read_4': Register('read_4', RELATIVE, 52),
            'read_5': Register('read_5', RELATIVE, 53),
            'read_6': Register('read_6', RELATIVE, 54),
            'read_7': Register('read_7', RELATIVE, 55),
            'read_8': Register('read_8', RELATIVE, 56),
            'read_9': Register('read_9', RELATIVE, 57),
            'read_10': Register('read_10', RELATIVE, 58),
            'read_11': Register('read_11', RELATIVE, 59),
            'read_12': Register('read_12', RELATIVE, 60),
            'read_13': Register('read_13', RELATIVE, 61),
            'read_14': Register('read_14', RELATIVE, 62),
            'read_15': Register('read_15', RELATIVE, 63),
            '@reg_0': Register('reg_0', ABSOLUTE, 0),
            '@reg_1': Register('reg_1', ABSOLUTE, 1),
            '@reg_2': Register('reg_2', ABSOLUTE, 2),
            '@reg_3': Register('reg_3', ABSOLUTE, 3),
            '@reg_4': Register('reg_4', ABSOLUTE, 4),
            '@reg_5': Register('reg_5', ABSOLUTE, 5),
            '@reg_6': Register('reg_6', ABSOLUTE, 6),
            '@reg_7': Register('reg_7', ABSOLUTE, 7),
            '@reg_8': Register('reg_8', ABSOLUTE, 8),
            '@reg_9': Register('reg_9', ABSOLUTE, 9),
            '@reg_10': Register('reg_10', ABSOLUTE, 10),
            '@reg_11': Register('reg_11', ABSOLUTE, 11),
            '@reg_12': Register('reg_12', ABSOLUTE, 12),
            '@reg_13': Register('reg_13', ABSOLUTE, 13),
            '@reg_14': Register('reg_14', ABSOLUTE, 14),
            '@reg_15': Register('reg_15', ABSOLUTE, 15),
            '@reg_16': Register('reg_16', ABSOLUTE, 16),
            '@reg_17': Register('reg_17', ABSOLUTE, 17),
            '@reg_18': Register('reg_18', ABSOLUTE, 18),
            '@reg_19': Register('reg_19', ABSOLUTE, 19),
            '@reg_20': Register('reg_20', ABSOLUTE, 20),
            '@reg_21': Register('reg_21', ABSOLUTE, 21),
            '@reg_22': Register('reg_22', ABSOLUTE, 22),
            '@reg_23': Register('reg_23', ABSOLUTE, 23),
            '@reg_24': Register('reg_24', ABSOLUTE, 24),
            '@reg_25': Register('reg_25', ABSOLUTE, 25),
            '@reg_26': Register('reg_26', ABSOLUTE, 26),
            '@reg_27': Register('reg_27', ABSOLUTE, 27),
            '@reg_28': Register('reg_28', ABSOLUTE, 28),
            '@reg_29': Register('reg_29', ABSOLUTE, 29),
            '@reg_30': Register('reg_30', ABSOLUTE, 30),
            '@reg_31': Register('reg_31', ABSOLUTE, 31),
            '@reg_32': Register('reg_32', ABSOLUTE, 32),
            '@reg_33': Register('reg_33', ABSOLUTE, 33),
            '@reg_34': Register('reg_34', ABSOLUTE, 34),
            '@reg_35': Register('reg_35', ABSOLUTE, 35),
            '@reg_36': Register('reg_36', ABSOLUTE, 36),
            '@reg_37': Register('reg_37', ABSOLUTE, 37),
            '@reg_38': Register('reg_38', ABSOLUTE, 38),
            '@reg_39': Register('reg_39', ABSOLUTE, 39),
            '@reg_40': Register('reg_40', ABSOLUTE, 40),
            '@reg_41': Register('reg_41', ABSOLUTE, 41),
            '@reg_42': Register('reg_42', ABSOLUTE, 42),
            '@reg_43': Register('reg_43', ABSOLUTE, 43),
            '@reg_44': Register('reg_44', ABSOLUTE, 44),
            '@reg_45': Register('reg_45', ABSOLUTE, 45),
            '@reg_46': Register('reg_46', ABSOLUTE, 46),
            '@reg_47': Register('reg_47', ABSOLUTE, 47),
            '@reg_48': Register('reg_48', ABSOLUTE, 48),
            '@reg_49': Register('reg_49', ABSOLUTE, 49),
            '@reg_50': Register('reg_50', ABSOLUTE, 50),
            '@reg_51': Register('reg_51', ABSOLUTE, 51),
            '@reg_52': Register('reg_52', ABSOLUTE, 52),
            '@reg_53': Register('reg_53', ABSOLUTE, 53),
            '@reg_54': Register('reg_54', ABSOLUTE, 54),
            '@reg_55': Register('reg_55', ABSOLUTE, 55),
            '@reg_56': Register('reg_56', ABSOLUTE, 56),
            '@reg_57': Register('reg_57', ABSOLUTE, 57),
            '@reg_58': Register('reg_58', ABSOLUTE, 58),
            '@reg_59': Register('reg_59', ABSOLUTE, 59),
            '@reg_60': Register('reg_60', ABSOLUTE, 60),
            '@reg_61': Register('reg_61', ABSOLUTE, 61),
            '@reg_62': Register('reg_62', ABSOLUTE, 62),
            '@reg_63': Register('reg_63', ABSOLUTE, 63),
            '@reg_64': Register('reg_64', ABSOLUTE, 64),
            '@reg_65': Register('reg_65', ABSOLUTE, 65),
            '@reg_66': Register('reg_66', ABSOLUTE, 66),
            '@reg_67': Register('reg_67', ABSOLUTE, 67),
            '@reg_68': Register('reg_68', ABSOLUTE, 68),
            '@reg_69': Register('reg_69', ABSOLUTE, 69),
            '@reg_70': Register('reg_70', ABSOLUTE, 70),
            '@reg_71': Register('reg_71', ABSOLUTE, 71),
            '@reg_72': Register('reg_72', ABSOLUTE, 72),
            '@reg_73': Register('reg_73', ABSOLUTE, 73),
            '@reg_74': Register('reg_74', ABSOLUTE, 74),
            '@reg_75': Register('reg_75', ABSOLUTE, 75),
            '@reg_76': Register('reg_76', ABSOLUTE, 76),
            '@reg_77': Register('reg_77', ABSOLUTE, 77),
            '@reg_78': Register('reg_78', ABSOLUTE, 78),
            '@reg_79': Register('reg_79', ABSOLUTE, 79),
            '@reg_80': Register('reg_80', ABSOLUTE, 80),
            '@reg_81': Register('reg_81', ABSOLUTE, 81),
            '@reg_82': Register('reg_82', ABSOLUTE, 82),
            '@reg_83': Register('reg_83', ABSOLUTE, 83),
            '@reg_84': Register('reg_84', ABSOLUTE, 84),
            '@reg_85': Register('reg_85', ABSOLUTE, 85),
            '@reg_86': Register('reg_86', ABSOLUTE, 86),
            '@reg_87': Register('reg_87', ABSOLUTE, 87),
            '@reg_88': Register('reg_88', ABSOLUTE, 88),
            '@reg_89': Register('reg_89', ABSOLUTE, 89),
            '@reg_90': Register('reg_90', ABSOLUTE, 90),
            '@reg_91': Register('reg_91', ABSOLUTE, 91),
            '@reg_92': Register('reg_92', ABSOLUTE, 92),
            '@reg_93': Register('reg_93', ABSOLUTE, 93),
            '@reg_94': Register('reg_94', ABSOLUTE, 94),
            '@reg_95': Register('reg_95', ABSOLUTE, 95),
            '@reg_96': Register('reg_96', ABSOLUTE, 96),
            '@reg_97': Register('reg_97', ABSOLUTE, 97),
            '@reg_98': Register('reg_98', ABSOLUTE, 98),
            '@reg_99': Register('reg_99', ABSOLUTE, 99),
            '@reg_100': Register('reg_100', ABSOLUTE, 100),
            '@reg_101': Register('reg_101', ABSOLUTE, 101),
            '@reg_102': Register('reg_102', ABSOLUTE, 102),
            '@reg_103': Register('reg_103', ABSOLUTE, 103),
            '@reg_104': Register('reg_104', ABSOLUTE, 104),
            '@reg_105': Register('reg_105', ABSOLUTE, 105),
            '@reg_106': Register('reg_106', ABSOLUTE, 106),
            '@reg_107': Register('reg_107', ABSOLUTE, 107),
            '@reg_108': Register('reg_108', ABSOLUTE, 108),
            '@reg_109': Register('reg_109', ABSOLUTE, 109),
            '@reg_110': Register('reg_110', ABSOLUTE, 110),
            '@reg_111': Register('reg_111', ABSOLUTE, 111),
            '@reg_112': Register('reg_112', ABSOLUTE, 112),
            '@reg_113': Register('reg_113', ABSOLUTE, 113),
            '@reg_114': Register('reg_114', ABSOLUTE, 114),
            '@reg_115': Register('reg_115', ABSOLUTE, 115),
            '@reg_116': Register('reg_116', ABSOLUTE, 116),
            '@reg_117': Register('reg_117', ABSOLUTE, 117),
            '@reg_118': Register('reg_118', ABSOLUTE, 118),
            '@reg_119': Register('reg_119', ABSOLUTE, 119),
            '@reg_120': Register('reg_120', ABSOLUTE, 120),
            '@reg_121': Register('reg_121', ABSOLUTE, 121),
            '@reg_122': Register('reg_122', ABSOLUTE, 122),
            '@reg_123': Register('reg_123', ABSOLUTE, 123),
            '@reg_124': Register('reg_124', ABSOLUTE, 124),
            '@reg_125': Register('reg_125', ABSOLUTE, 125),
            '@reg_126': Register('reg_126', ABSOLUTE, 126),
            '@reg_127': Register('reg_127', ABSOLUTE, 127),
            # Next register represents "don't care", and is used when
            # the register isn't used, so doesn't matter what it is
            '--': Register('reg_127', ABSOLUTE, 127),
        }

    def _flatten(self, tree):
        to_return = list(itertools.chain.from_iterable(tree))
        done = False
        while not done:
            if all(not isinstance(element, list) for element in to_return):
                done = True
            else:
                to_return = list(itertools.chain.from_iterable(to_return))
        return to_return

    def statements(self, tree):
        return tree

    def label_statement(self, tree):
        label = ''.join(self._flatten(tree[0].children)).upper()
        opcode, inst = tree[1]
        new_inst = inst._replace(label=label)
        return (opcode, new_inst)

    def shift(self, tree):
        the_shift = ''.join(self._flatten(tree)).upper()
        if the_shift == '<<':
            return SHIFT_LEFT
        elif the_shift == '>>':
            return SHIFT_RIGHT
        else:
            import ipdb; ipdb.set_trace()
            l.error("Should not go here")
            return None

    def statement(self, tree):
        tree = tree[0].children
        opcode_name = str(tree[0]).upper()

        arg_list = tree[1].children if len(tree) >= 2 else []

        token = None
        if len(tree) == 3:
            token = tree[-1]
        opcode = OPCODES[opcode_name]

        inst = opcode.instruction_type.to_inst(opcode, arg_list, token)

        return (opcode, inst)

    def csr_ref(self, tree):
        csr_ref = ''.join(self._flatten(tree)).upper()
        csr_num = int(csr_ref.split('_')[-1])
        return csr_num

    def direction(self, tree):
        direction = ''.join(self._flatten(tree)).upper()
        return DIRECTIONS[direction]

    def target(self, tree):
        target = ''.join(self._flatten(tree)).upper()
        return target

    def token(self, tree):
        token = ''.join(self._flatten(tree)).upper()
        return TOKENS[token]

    def alu_op_type(self, tree):
        name = str(''.join(self._flatten(tree[0]))).upper()
        type = ALU_TYPES[name]
        return type.num

    def name_to_register(self, name):
        return self.registers[name]

    def register(self, tree):
        name = str(''.join(self._flatten(tree[0].children)))
        return self.name_to_register(name)

    def literal_num(self, tree):
        num = ''.join(self._flatten(tree))
        return eval(num)

    def rot(self, tree):
        rot = ''.join(self._flatten(tree))
        if rot == '<<0':
            return ANOTHER_NO_ROT
        elif rot == '<<8':
            return LEFT_EIGHT
        elif rot == '<<16':
            return LEFT_SIXTEEN
        else:
            return NO_ROT

def extract_labels(opcode_insts):
    to_return = {}

    i = 0
    for (opcode, inst) in opcode_insts:
        if inst.label:
            if inst.label in to_return:
                l.error(f"{inst.label} already defined")
                sys.exit(-1)
            to_return[inst.label] = i
        i += 1

    return to_return

def resolve_targets(opcode_insts, targets):
    to_return = []

    for (opcode, inst) in opcode_insts:
        if hasattr(inst, 'target'):
            if type(inst.target) == int:
                continue
            resolved = targets[inst.target]
            new_inst = inst._replace(target=resolved)
            to_return.append((opcode, new_inst))
        else:
            to_return.append((opcode, inst))
    return to_return

def change_endianess_of_each_byte(binary):
    the_bytes = bytearray(binary)
    to_return = bytearray()
    for b in the_bytes:
        new_byte = int(f"{b:08b}"[::-1], 2)
        to_return.append(new_byte)
    return bytes(to_return)

def main(input_file, output_file):
    with open(GRAMMAR_FILE, 'r') as grammar:
        parser = lark.Lark(grammar, start='program')

    with open(input_file, 'r') as input:
        tree = parser.parse(input.read())


    opcode_insts = GenerateOpcodes().transform(tree)

    labels = extract_labels(opcode_insts)

    opcode_insts = resolve_targets(opcode_insts, labels)

    to_return = b''
    for (opcode, inst) in opcode_insts:
        binary = opcode.instruction_type.to_binary(inst)

        binary = change_endianess_of_each_byte(binary)

        assert(len(binary) == INSTRUCTION_BYTES)
        to_return += binary

    with open(output_file, 'wb') as f:
        f.write(to_return)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog="assembler")
    parser.add_argument("--debug", action="store_true", help="Enable debugging")
    parser.add_argument("--file", type=str, required=True, help="The file to assemble")
    parser.add_argument("--output", type=str, help="Where to write the binary output.")

    args = parser.parse_args()

    if args.debug:
        logging.basicConfig(level=logging.DEBUG)

    main(args.file, args.output or "output.bin")
