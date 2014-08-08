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
objs=['k', 'm', 'u', 'U']

class FooAction(argparse.Action):
    def __init__(self, option_strings, dest, nargs=None, **kwargs):
        if nargs is not None:
            raise ValueError("nargs not allowed")
        super(FooAction, self).__init__(option_strings, dest, **kwargs)
        pass
    def __call__(self, parser, namespace, values, option_string=None):
        if namespace.__dict__.get('action'):
            print("Conflict actions: %s vs %s, showing usage...\n"%(
                self.dest, getattr(namespace, 'action')))
            parser.print_help()
            sys.exit(1)

        setattr(namespace, 'action', self.dest)
        setattr(namespace, 'target', values)

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
            None: '/usr/portage/distfiles'
            }.get(opts.target, None)

        if self.path and os.getenv("EPREFIX"):
            #Used by gentoo prefix(MacOsX).
            self.path = os.path.join(os.getenv("EPREFIX"), self.path)

        self.contents = []
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

        os.makedirs(dirn, exist_ok=True)
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
        if not self.opts.args:
            print("deleting operation needs arguments, showing usage\n\n")
            self.Help()
            sys.exit(1)

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
        return "".join(self.contents)


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

class UseRecord:
    def __init__(self, entry):
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

    def merge(self):
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
        """
        Arguments:
        - `args`:
        """
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


class DistPortageObject(PortageObject):
    """
    """

    def __init__(self, path):
        """
        """
        PortageObject.__init__(self, path)
        pass

    def __clean_obj(self):
        """
        Arguments:
        - `args`:
        """
        pass

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=desc, epilog=elog,
                                     formatter_class=argparse.RawDescriptionHelpFormatter,)
    parser.add_argument('-a', '--add', metavar='OBJ',
                        action=FooAction,
                        choices=objs,
                        help='add content to OBJECT')
    parser.add_argument('-d', '--delete', metavar='OBJ',
                        action=FooAction,
                        choices=objs, help='delete content from an OBJECT')
    parser.add_argument('-l', '--list', metavar='OBJ',
                        action=FooAction,
                        choices=objs, help='list content of an OBJECT')
    # parser.add_argument('-c', '--clean', nargs='?')
    parser.add_argument('args', nargs=argparse.REMAINDER, metavar='content',
                        help='contents to be add/delete to OBJECT')

    opts = parser.parse_args(sys.argv[1:])

    if opts.action != 'clean':
        executer = {
            'u' : UsePortageObject,
            None : DistPortageObject
        }.get(opts.target, PortageObject)(opts)
        executer.Action()
    else:
        print("Clean is not implemented yet, you can try C version kmu instead...")

# Editor modelines

# Local Variables:
# c-basic-offset: 4
# tab-width: 4
# indent-tabs-mode: nil
# End:

# vim: set noet ts=4 sw=4:
