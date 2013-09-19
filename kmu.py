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

from argparse import *
import os
import sys


func   = None # Function pointer to be executed.
target = None # Target to be operated.
target_list = {'k': 'Keyword',
               'm': 'Mask',
               'u': 'Use',
               'U': 'Unmask'}
def usage():
    """Show usage
    """
    print("Showing usge:\n\n")
    get_parser().print_help()

def add_obj(args):
    print("Adding %s"%target_list.get(target))
    if not args:
        print ("Add action requires a targe!\n")
        usage()
        sys.exit(1)


def list_obj(args):
    print(args)
    print(target)
    pass

def delete_obj(args):
    print(args)
    print(target)
    pass

def clean_obj(args):
    print(args)
    print(target)
    pass

class ActionAction(Action):
    def __call__(self, parser, namespace, values, option_string=None):
        # set global function pointer directly!
        global func
        setattr(namespace, self.dest, values)
        if self.dest == 'add':
            func = add_obj
        elif self.dest == 'list':
            func = list_obj
        elif self.dest == 'delete':
            func = delete_obj
        elif self.dest =='clean':
            func = clean_obj
        else:
            print("Unknown action: %s\n"%values)
            get_parser().print_help()
            sys.exit(1)

        global target
        if values not in target_list.keys():
            print("Unknown target: %s\n"%values)
            get_parser().print_help()
            sys.exit(2)
        else:
            target = values

def get_parser():
    """Return arg parser.
    """
    parser = ArgumentParser("kmu",
                             description="Manage your KEYWORD, MASK and USE",
                             epilog='''****** Examples: ******
    List all keywords stored in /etc/portage/package.keyword:
        kmu -lk any_string
    Add a keyword into /etc/portage/package.keyword:
        kmu -ak any_string
    Delete keyword entry which includes xxx
        kmu -du any_string

''',
                             formatter_class=RawDescriptionHelpFormatter)

    group = parser.add_mutually_exclusive_group()
    group.add_argument("-a", "--add", action=ActionAction,  help="Delete an object.",)
    group.add_argument("-d", "--delete", action=ActionAction,help="Add an object.")
    group.add_argument("-l", "--list",   action=ActionAction,help="List object(s).")
    group.add_argument("-c", "--clean",   action=ActionAction,help="clean object(s).")

    parser.add_argument("content", nargs="*")
    return parser

if __name__ == '__main__':

    argp = get_parser()

    if len(sys.argv) == 1:
        argp.print_help()
        sys.exit(0)

    opts = argp.parse_args()

    if func:
        func(opts.__getattribute__('content'))

    # for k in ['add', 'delete', 'list']:
    #     if opts.get(k):




# Editor modelines

# Local Variables:
# c-basic-offset: 4
# tab-width: 4
# indent-tabs-mode: nil
# End:

# vim: set noet ts=4 sw=4:
