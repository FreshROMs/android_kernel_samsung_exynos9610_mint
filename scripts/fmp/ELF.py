#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Module ELF contains ELF, Symbol, Section classes for manipulation over ELF files.
It can parse, and change ELF file. This version works only with vmlinux and doesn't properly work with ELF that contains
UND symbols
"""

import subprocess
import re
import os
from Utils import Utils
from collections import OrderedDict

__author__ = "Vadym Stupakov"
__copyright__ = "Copyright (c) 2017 Samsung Electronics"
__credits__ = ["Vadym Stupakov"]
__version__ = "1.0"
__maintainer__ = "Vadym Stupakov"
__email__ = "v.stupakov@samsung.com"
__status__ = "Production"


class Symbol:
    def __init__(self, name=str(), sym_type=str(), bind=str(), visibility=str(), addr=int(), size=int(), ndx=str()):
        self.utils = Utils()
        self.name = str(name)
        self.type = str(sym_type)
        self.bind = str(bind)
        self.ndx = str(ndx)
        self.visibility = str(visibility)
        self.addr = self.utils.to_int(addr)
        self.size = self.utils.to_int(size)

    def __str__(self):
        return "name: '{}', type: '{}', bind: '{}', ndx: '{}', visibility: '{}', address: '{}', size: '{}'".format(
            self.name, self.type, self.bind, self.ndx, self.visibility, hex(self.addr), hex(self.size)
        )

    def __lt__(self, other):
        return self.addr <= other.addr


class Section:
    def __init__(self, name=str(), sec_type=str(), addr=int(), offset=int(), size=int()):
        self.utils = Utils()
        self.name = str(name)
        self.type = str(sec_type)
        self.addr = self.utils.to_int(addr)
        self.offset = self.utils.to_int(offset)
        self.size = self.utils.to_int(size)

    def __str__(self):
        return "name: '{}', type: '{}', address: '{}', offset: '{}', size: '{}'".format(
            self.name, self.type, hex(self.addr), hex(self.offset), hex(self.size)
        )

    def __lt__(self, other):
        return self.addr <= other.addr


class ELF:
    """
    Utils for manipulating over ELF
    """
    def __init__(self, elf_file, readelf_path="readelf"):
        self.__elf_file = elf_file
        self.utils = Utils()
        self.__readelf_path = readelf_path
        self.__sections = OrderedDict()
        self.__symbols = OrderedDict()
        self.__relocs = list()
        self.__re_hexdecimal = "\s*[0-9A-Fa-f]+\s*"
        self.__re_sec_name = "\s*[._a-zA-Z]+\s*"
        self.__re_type = "\s*[A-Z]+\s*"

    def __readelf_raw(self, options):
        """
        Execute readelf with options and print raw output
        :param options readelf options: ["opt1", "opt2", "opt3", ..., "optN"]
        :returns raw output
        """
        ret = subprocess.Popen(args=[self.__readelf_path] + options,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
        stdout, stderr = ret.communicate()
        if "readelf: Error: the PHDR segment is not covered by a LOAD segment" in stderr.decode("utf-8").strip():
            ret.returncode = 0
        if ret.returncode != 0:
            raise ChildProcessError(stderr.decode("utf-8") + stdout.decode("utf-8"))
        return stdout.decode("utf-8")

    def set_elf_file(self, elf_file):
        if os.path.abspath(self.__elf_file) != os.path.abspath(elf_file):
            self.__elf_file = os.path.abspath(elf_file)
            self.__sections.clear()
            self.__symbols.clear()
            self.__relocs.clear()

    def get_elf_file(self):
        return os.path.abspath(self.__elf_file)

    def get_sections(self):
        """"
        Execute -> parse -> transform to dict() readelf output
        :returns dict: {sec_addr : Section()}
        """
        if len(self.__sections) == 0:
            sec_header = self.__readelf_raw(["-SW",  self.__elf_file]).strip()
            secs = re.compile("^.*\[.*\](" + self.__re_sec_name + self.__re_type + self.__re_hexdecimal +
                              self.__re_hexdecimal + self.__re_hexdecimal + ")", re.MULTILINE)
            found = secs.findall(sec_header)
            for line in found:
                line = line.split()
                if len(line) == 5:
                    self.__sections[int(line[2], 16)] = Section(name=line[0], sec_type=line[1], addr=int(line[2], 16),
                                                                offset=int(line[3], 16), size=int(line[4], 16))
            self.__sections = OrderedDict(sorted(self.__sections.items()))
        return self.__sections

    def get_symbols(self):
        """"
        Execute -> parse -> transform to dict() readelf output
        :returns dict: {sym_addr : Symbol()}
        """
        if len(self.__symbols) == 0:
            sym_tab = self.__readelf_raw(["-sW",  self.__elf_file])
            syms = re.compile(r"^.*\d+:\s(.*$)", re.MULTILINE)
            found = syms.findall(sym_tab.strip())
            for line in found:
                line = line.split()
                if len(line) == 7:
                    size = line[1]
                    # This needs, because readelf prints sizes in hex if size is large
                    if size[:2].upper() == "0X":
                        size = int(size, 16)
                    else:
                        size = int(size, 10)
                    self.__symbols[int(line[0], 16)] = Symbol(addr=int(line[0], 16), size=size, sym_type=line[2],
                                                              bind=line[3], visibility=line[4], ndx=line[5],
                                                              name=line[6])
            self.__symbols = OrderedDict(sorted(self.__symbols.items()))
        return self.__symbols

    def get_relocs(self, start_addr=None, end_addr=None):
        """"
        :param start_addr: start address :int
        :param end_addr: end address: int
        :returns list: [reloc1, reloc2, reloc3, ..., relocN]
        """
        if len(self.__relocs) == 0:
            relocs = self.__readelf_raw(["-rW",  self.__elf_file])
            rel = re.compile(r"^(" + self.__re_hexdecimal + ")\s*", re.MULTILINE)
            self.__relocs = [self.utils.to_int(el) for el in rel.findall(relocs.strip())]

        if start_addr and end_addr is not None:
            ranged_rela = list()
            for el in self.__relocs:
                if self.utils.to_int(start_addr) <= self.utils.to_int(el) <= self.utils.to_int(end_addr):
                    ranged_rela.append(el)
            return ranged_rela
        return self.__relocs

    def get_symbol_by_name(self, sym_names):
        """
        Get symbol by_name
        :param sym_names: "sym_name" : str or list
        :return: Symbol() or [Symbol()]
        """
        if isinstance(sym_names, str):
            for addr, symbol_obj in self.get_symbols().items():
                if symbol_obj.name == sym_names:
                    return symbol_obj
        elif isinstance(sym_names, list):
            symbols = [self.get_symbol_by_name(sym_name) for sym_name in sym_names]
            return symbols
        else:
            raise ValueError
        return None

    def get_symbol_by_vaddr(self, vaddrs=None):
        """
        Get symbol by virtual address
        :param vaddrs: vaddr : int or list
        :return: Symbol() or [Symbol()]
        """
        if isinstance(vaddrs, int):
            if vaddrs in self.get_symbols():
                return self.get_symbols()[vaddrs]
            for addr, symbol_obj in self.get_symbols().items():
                if (addr + symbol_obj.size) >= vaddrs >= addr:
                    return symbol_obj
        elif isinstance(vaddrs, list):
            symbol = [self.get_symbol_by_vaddr(vaddr) for vaddr in vaddrs]
            return symbol
        else:
            raise ValueError
        return None

    def get_section_by_name(self, sec_names=None):
        """
        Get section by_name
        :param sec_names: "sec_name" : str or list
        :return: Section() or [Section()]
        """
        if isinstance(sec_names, str):
            for addr, section_obj in self.get_sections().items():
                if section_obj.name == sec_names:
                    return section_obj
        elif isinstance(sec_names, list):
            sections = [self.get_section_by_name(sec_name) for sec_name in sec_names]
            return sections
        else:
            raise ValueError
        return None

    def get_section_by_vaddr(self, vaddrs=None):
        """
        Get section by virtual address
        :param vaddrs: vaddr : int  or list
        :return: Section() or [Section()]
        """
        if isinstance(vaddrs, int):
            if vaddrs in self.get_sections():
                return self.get_sections()[vaddrs]
            for addr, section_obj in self.get_sections().items():
                if (addr + section_obj.size) >= vaddrs >= addr:
                    return section_obj
        elif isinstance(vaddrs, list):
            sections = [self.get_symbol_by_vaddr(vaddr) for vaddr in vaddrs]
            return sections
        else:
            raise ValueError
        return None

    def vaddr_to_file_offset(self, vaddrs):
        """
        Transform virtual address to file offset
        :param vaddrs: addr string or int or list
        :returns file offset or list
        """
        if isinstance(vaddrs, str) or isinstance(vaddrs, int):
            section = self.get_section_by_vaddr(vaddrs)
            return self.utils.to_int(vaddrs, 16) - section.addr + section.offset
        elif isinstance(vaddrs, list):
            return [self.vaddr_to_file_offset(vaddr) for vaddr in vaddrs]
        else:
            raise ValueError

    def read_data_from_vaddr(self, vaddr, size, out_file):
        with open(self.__elf_file, "rb") as elf_fp:
            elf_fp.seek(self.vaddr_to_file_offset(vaddr))
            with open(out_file, "wb") as out_fp:
                out_fp.write(elf_fp.read(size))
