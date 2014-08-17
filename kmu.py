#!/usr/bin/env python
######################################################################
## Copyright (C) 2011, 2013, Yang, Ying-chao
##
## Author:        Yang,Ying-chao <yangyingchao@gmail.com>
##
## This program is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License
## as published by the Free Software Foundation; either version 2
## of the License, or (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
##
## Description:   simple utility to manager keyword, use and masks.
##
##
#####################################################################
# -*- coding: utf-8 -*-

import os
import sys
import glob
import shutil
import argparse
import re

desc = '''Simple tool to manager keyword/(un)mask/use for Gentoo'''
elog = '''
Where OBJECT could be one of the following:
k, keyword: Accept a new keyword specified by package_string
m, mask: 	mask a new keyword specified by package_string
u, use: 	Modify or add new use to package_string
U, Umask: 	Unmask a package

****** Examples: ******
To list all keywords stored in /etc/portage/package.keyword:
	kmu -lk
To add a keyword into /etc/portage/package.keyword:
	kmu -ak
To delete keyword entry which includes xxx
	kmu -du xxx
'''
objs=['k', 'm', 'u', 'U', 'p']

class KmuArgAction(argparse.Action):
    def __init__(self, option_strings, dest, **kwargs):
        super(KmuArgAction, self).__init__(option_strings, dest, **kwargs)
        pass
    def __call__(self, parser, namespace, values, option_string=None):
        if namespace.__dict__.get('action'):
            print("Conflict actions: %s vs %s, showing usage...\n"%(
                self.dest, getattr(namespace, 'action')))
            parser.print_help()
            sys.exit(1)

        if self.dest == 'clean' and values is None:
            values = 'p'

        setattr(namespace, 'action', self.dest)
        setattr(namespace, 'target', values)

class Record(object):
    """Simple Record of entries.
    """

    def __init__(self, content):
        """

        Arguments:
        - `content`:
        """
        self._content = content
        self._name    = content
        self._keep    = True

    def __str__(self):
        """
        """
        return "%s"%(self._content if self._keep else "")

r = re.compile('(.+?)(?:[-_]((?:\d+[\.\-_])+))')
class PackageRecord(Record):
    """
    """

    def __init__(self, content):
        """
        """
        super(PackageRecord, self).__init__(content)
        self._fpath = content
        self._bname = os.path.basename(content)
        self._siblings = []
        self._size = os.path.getsize(content)
        self._delete = False
        if content.find("_checksum_failure_") != -1:
            self._key = "Temporary Files"
            self._delete = True
        elif content.find("patch") != -1 or content.find("diff") != -1:
            self._key = "Patches"
            self._delete = True
        else:
            global r
            res = r.match(self._bname)
            if res:
                self._key = res.group(1)
                self._v   = res.group(2).replace('-', '.').replace('_', '.').strip('.')
            else:
                self._key = "Unrecognized Files."
                self._delete = True # flag to delete

    def __str__(self):
        """
        """
        return self._fpath;

    def __lt__(self, other):
        """

        Arguments:

        - `other`:
        """
        vs1 = self._v.split('.')
        vs2 = other._v.split('.')

        lc = len(vs1)
        lo = len(vs2)

        for i in range(max(lc, lo)):
            if i >= lc:
                return True
            elif i >= lo:
                return False
            else:
                l = int(vs1[i])
                o = int(vs2[i])
                if l == o:
                    continue
                return l < o
        return False

class PackageContainer(object):
    """
    """

    def __init__(self, p):
        """
        """
        self._label = p._key
        if not p._delete:
            self._current  = p
            self._del_list = []
        else:
            self._current = None
            self._del_list = [p]
        pass

    def AddPackage(self, p):
        """
        Add package into this container..
        """
        if not p._delete:
            if p < self._current:
                self._del_list.append(p)
            else:
                self._del_list.append(self._current)
                self._current = p
        else:
            self._del_list.append(p)

    def ClearOldOnes(self, idx):
        """
        """
        lst = []
        sz  = 0
        for item in self._del_list:
            sz += item._size
            lst.append(item._fpath)

        if lst:
            print(" %03d: %s"%(idx, self._label))
            print("   KEEP: %s"%self._current)
            print("   DEL : %s\n"%("\n         ".join(lst)))
        return (lst, sz)

class PortageObject(object):
    """Generic object for portage files.
    """
    def __init__(self, opts):
        """
        """
        self.opts   = opts
        self.path   = {
            'k': "/etc/portage/package.keywords/keywords",
            'm': "/etc/portage/package.mask/mask",
            'u': "/etc/portage/package.use/use",
            'U': "/etc/portage/package.unmask/unmask",
            'p': '/usr/portage/distfiles',
            None: '/usr/portage/distfiles'
            }.get(opts.target, None)

        if self.path and os.getenv("EPREFIX"):
            #Used by gentoo prefix(MacOsX).
            self.path = os.path.join(os.getenv("EPREFIX"), self.path)

        try:
            self.contents = open(self.path).readlines()
            self.__parse__()
        except :
            self.contents=[]

    def Help(self):
        """
        """
        parser.print_help()

    def Action(self):
        func = {
            'add' : self.__add_obj__,
            'delete' : self.__del_obj__,
            'list' : self.__list_obj__,
            'clean' : self.__clean_obj__,
            }.get(self.opts.action,
                  lambda x : parser.print_help())()
        pass

    def __str__(self):
        """
        """
        sep=""
        return sep.join(self.contents)

    def __parse__(self):
        """
        """
        self.records = {}
        for content in self.contents:
            content = content.strip()
            if content.startswith('#'):
                continue #skip comments.
            record = Record(content)
            self.records[record._name] = record

        pass

    def __get_portage_dir__(self):
        # todo: add overlays.
        path = "/usr/portage"
        if os.getenv("EPREFIX"):
            path.join(os.getenv("EPREFIX"), path)
        return [path]

    def __validate_item(item):
        """Validate item, returns an item that can be recognized by portage.

        Arguments:
        - `item`: item to be validate.
        """
        first  = None
        second = None
        # if '/' in item:
        #     cmps=item.split('/')
        #     fist   = cmps[0]
        #     second = cmps[1]
        # else:
        #     second = item

        # portage_dirs = __get_portage_dir__()
        # if first:
        #     candidates = []
        #     for path in portage_dirs:
        #         candidates.extends(glob.glob(os.path.join(path, "*")))
        return item

    def __dump__(self):
        """write contents back to disk file
        """
        if self.path is None:
            print("Can't decide where to write files.!")
            sys.exit(1)

        dirn = os.path.dirname(self.path)
        if os.path.exists(dirn) and not os.path.isdir(dirn):
            self.__merge_from_file(dirn)
            os.shutil.copy(dirn, dirn+"bakup")
            os.remove(dirn)

        if not os.access(dirn, os.F_OK):
            os.makedirs(dirn)
        try:
            open(self.path, "w").write(self.__str__())
        except IOError as e:
            print("failed to write to file: %s, reason: %s.\n"%(self.path, e.strerror))
            sys.exit(2)

    def __add_obj__(self):
        """
        Arguments:
        """
        if not self.opts.args:
            print("Adding operation needs arguments, showing usage\n\n")
            self.Help()
            sys.exit(1)

        item = " ".join(self.opts.args).strip() + "\n"
        print("Adding %s to %s"%(item, self.path))
        self.contents.append(item)
        self.__dump__()

    def __del_obj__(self):
        """
        Arguments:
        """
        args = self.opts.args
        if not args:
            print("deleting operation needs arguments, showing usage\n\n")
            self.Help()
            sys.exit(1)

        # todo:
        operands = []
        for item in args:
            for record in self.records.values():
                if record._content.find(item) != -1:
                    record._keep = False
                    operands.append(record)
        if len(operands) > 1:
            print("Going to delete multiple records: \n\t%s,\ncontinue?\n"%(
                "\n\t".join(map(lambda X: X.__str__(), operands))))

            if sys.stdin.readline().strip().lower() != 'y':
                print("Operation aborted..\n")
                sys.exit(1)

        self.__dump__()
        pass

    def __list_obj__(self):
        """
        Arguments:
        """
        if not self.contents:
            print("No entries in %s\n"%self.path)
            sys.exit(1)

        print("Listing %s contains: %s"%( self.path, " ".join(self.opts.args) if self.opts.args else "all item"))
        if self.opts.args:
            result=set()
            for entry in self.contents:
                for item in self.opts.args:
                    if item in entry:
                        result.add(entry)
            if result:
                print("\nTotal %d entries found, as follows\n\n%s"%(
                    len(result), "".join(result)))
            else:
                print("\n No entry found.\n")
        else:
            item = ""
            print(item.join(self.contents))

        pass

    def __clean_obj__(self):
        """
        Arguments:
        - `args`:
        """
        pass

    def __usage__(self):
        """
        Arguments:
        - `args`:
        """
        pass

    def __str__(self):
        """
        """
        return "\n".join(map(lambda X: X.__str__(), self.records))


class UseFlag:
    def __init__(self, record):
        if record.startswith('-'):
            self._use = record[1:]
            self._sign   = '-'
        else:
            self._use = record
            self._sign   = ''

    def __str__(self):
        return self._sign+self._use

    def __hash__(self):
        return hash(self._use)

class UseRecord(Record):
    def __init__(self, entry):
        Record.__init__(self, entry)
        cmps = entry.split()
        self._name = cmps.pop(0);
        self._flags = {}
        for e in cmps:
            flag = UseFlag(e)
            self._flags[flag._use] = flag

    def __str__(self):
        result = self._name
        for flag in self._flags.values():
            result += " " + str(flag)
        return result

    def merge(self, args):
        for e in args:
            nflag = UseFlag(e)
            self._flags[nflag._use] = nflag

class UsePortageObject(PortageObject):
    def __init__(self, opts):
        """
        """
        PortageObject.__init__(self, opts)
        pass

    def __parse__(self):
        self.records = {}
        for entry in self.contents:
            record = UseRecord(entry)
            self.records[record._name] = record
        pass

    def __add_obj__(self):
        args = self.opts.args
        if not args or len(args) < 1:
            print("Adding operation needs arguments, showing usage\n\n")
            self.Help()
            sys.exit(1)
        print(self.__str__())

        # Check if use entry exists in self.records, merge it if exists!
        record = self.records.pop(args[0], None)
        if record is None:
            record = UseRecord(" ".join(args))
        else:
            record.merge(args[1:])
        self.records[record._name] = record

        self.__dump__()
        print(self.__str__())


    def __str__(self):
        result = ""
        for record in self.records.values():
            result += str(record) + "\n"
        return result

def stringify_size(s):
    P = 1024
    if s < P:
        return "%d bytes"%s
    OP = P
    P *= 1024
    if s < P:
        return "%d KB"%(s/OP)
    else:
        return "%.02f MB"%(float(s)/P)


class DistPortageObject(PortageObject):
    """
    """

    def __init__(self, path):
        """
        """
        PortageObject.__init__(self, path)
        self._packages = {}
        for root, dirs, files in os.walk(self.path):
            for fn in files:
                fpath = os.path.join(root, fn)
                p = PackageRecord(fpath)
                pc = self._packages.get(p._key)
                if pc is None:
                    pc = PackageContainer(p)
                    self._packages[p._key] = pc
                else:
                    pc.AddPackage(p)
        pass

    def __clean_obj__(self):
        """
        Arguments:
        - `args`:
        """
        f_size  = 0
        f_list  = []
        idx = 0

        print("Checking old packages....\n")

        for pc in self._packages.values():
            (lst, sz) = pc.ClearOldOnes(idx)
            f_list.extend(lst)
            f_size += sz
            if lst:
                idx += 1

        if f_list:
            print("\nGoing to deleted %d files,  %s disk spaces will be freed.\n"%(
                len(f_list), stringify_size(f_size)))
            print("Continue? (Y/N)\n")

            if sys.stdin.readline().strip().lower() != 'y':
                print("Operation aborted..\n")

            try:
                for item in f_list:
                    os.remove(item)
            except:
                print("Failed to remove files: %s\n"%(sys.exc_info()))
            else:
                print("Finished in cleaning packages...\n")
        else:
            print("No old package detected...\n")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=desc, epilog=elog,
                                     formatter_class=argparse.RawDescriptionHelpFormatter,)
    parser.add_argument('-a', '--add', metavar='OBJ',
                        action=KmuArgAction,
                        choices=objs,
                        help='add content to OBJECT')
    parser.add_argument('-d', '--delete', metavar='OBJ',
                        action=KmuArgAction,
                        choices=objs, help='delete content from an OBJECT')
    parser.add_argument('-l', '--list', metavar='OBJ',
                        action=KmuArgAction,
                        choices=objs, help='list content of an OBJECT')
    parser.add_argument('-c', '--clean',
                        action=KmuArgAction, nargs='?',
                        choices=objs, help='clean up content')
    parser.add_argument('args', nargs=argparse.REMAINDER, metavar='content',
                        help='contents to be add/delete to OBJECT')

    if len(sys.argv) == 1:
        print("Missing arguments, showing help..\n")
        parser.print_help()
        sys.exit(1)

    opts = parser.parse_args(sys.argv[1:])

    if not opts.__dict__.get('target') and opts.action != 'clean':
        print("Wrong usage, showing help...\n")
        parser.print_help()
        sys.exit(1)


    executer = {
        'u' : UsePortageObject,
        'p' : DistPortageObject,
        None : DistPortageObject
        }.get(opts.target, PortageObject)(opts)
    executer.Action()

# Editor modelines

# Local Variables:
# c-basic-offset: 4
# tab-width: 4
# indent-tabs-mode: nil
# End:

# vim: set noet ts=4 sw=4:
