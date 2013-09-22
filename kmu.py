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

help='''Usage: kmu -a|d|l|h -k|m|u|U [package_string]
    ****** Operations: ********
    -a, --add: 	Add an ActObject
    -d, --delete: 	Delete an ActObject
    -l, --list: 	List an ActObject
    -c, --clean: 	Clean local resources
    -h, --help: 	Print this message
    ****** Objects: ********
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

class PortageObject(object):
    """Generic object for portage files.
    """
    def __init__(self, target):
        """
        """
        self.path = {
            'k': "/etc/portage/package.keywords/keywords",
            'm': "/etc/portage/package.mask/mask",
            'u': "/etc/portage/package.use/use",
            'U': "/etc/portage/package.unmask/unmask",
            None: '/usr/portage/distfiles'
            }.get(target, None)

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
        print(help)

    def Action(self, action, args):
        func = {
            'a' : self.__add_obj__,
            'd' : self.__del_obj__,
            'l' : self.__list_obj__,
            'c' : self.__clean_obj__,
            'h' : self.__usage__
            }.get(action, lambda x : print("Unkown usage, showing help\n\n"+help))(args)
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
        result = ""
        for item in self.contents:
            result += item.strip("\n") + "\n"
        try:
            open(self.path, "w").write(result)
        except IOError as e:
            print("failed to write to file: %s, reason: %s.\n"%(self.path, e.strerror))
            sys.exit(2)

    def __add_obj__(self, args):
        """
        Arguments:
        - `args`:
        """
        if not args:
            print("Adding operation needs arguments, showing usage\n\n")
            self.Help()
            sys.exit(1)
        item = " "
        item.join(args)
        print("Adding %s to %s"%(item, self.path))
        self.contents.append(item)
        self.__dump__()

    def __del_obj__(self, args):
        """
        Arguments:
        - `args`:
        """
        if not args:
            print("Adding operation needs arguments, showing usage\n\n")
            self.Help()
            sys.exit(1)

        pass

    def __list_obj__(self, args):
        """
        Arguments:
        - `args`:
        """
        if not self.contents:
            print("No entries in %s\n"%self.path)
            sys.exit(1)

        print("Listing %s contains: %s"%( self.path, " ".join(args) if args else "all item"))
        if args:
            result=[]
            for entry in self.contents:
                for item in args:
                    if item in entry:
                        print("%s --- %s"%(item, entry))
                        result.append(entry)
            if result:
                print(result)
                print("\nTotal %d entries found, as follows\n\n%s"%(
                    len(result), "".join(result)))
            else:
                print("\n No entry found.\n")
        else:
            item = ""
            print(item.join(self.contents))

        pass

    def __clean_obj__(self, args):
        """
        Arguments:
        - `args`:
        """
        pass

    def __usage__(self, args):
        """
        Arguments:
        - `args`:
        """
        pass

class USEPortageObject(PortageObject):

    def __init__(self, path):
        """
        """
        PortageObject.__init__(self, path)
        pass

    def __parse__(self):
        """
        """
        pass
    def __add_obj__(self, args):
        """
        Arguments:
        - `args`:
        """

class DistPortageObject(PortageObject):
    """
    """

    def __init__(self, path):
        """
        """
        PortageObject.__init__(self, path)
        pass

    def __clean_obj(self, args):
        """
        Arguments:
        - `args`:
        """
        pass


if __name__ == '__main__':

    #TODO: Usage argparse to do the parsing.
    args = sys.argv
    if len(args) == 1:
        usage()
        sys.exit(1)

    app = args.pop(0)
    commbo = args.pop(0)
    if (not commbo.startswith('-')) or \
        (len(commbo) != 2 and len(commbo) != 3):
        usage()
        sys.exit(1)

    action = commbo[1]
    target = None if len(commbo) == 2 else commbo[2]
    executer = {
        'u' : USEPortageObject,
        None : DistPortageObject
    }.get(target, PortageObject)(target)

    executer.Action(action, args)

# Editor modelines

# Local Variables:
# c-basic-offset: 4
# tab-width: 4
# indent-tabs-mode: nil
# End:

# vim: set noet ts=4 sw=4:
