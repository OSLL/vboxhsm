# Copyright (c) 2001, Stanford University
# All rights reserved.
#
# See the file LICENSE.txt for information on redistributing this software.

import sys

sys.path.append( "../glapi_parser" )
import apiutil


apiutil.CopyrightC()

print """/* DO NOT EDIT!  THIS CODE IS AUTOGENERATED BY unpack_extend.py */

#ifndef UNPACK_EXTEND_H
#define UNPACK_EXTEND_H 1

"""


#
# Print extern declarations for all special unpacker functions
#
for func_name in apiutil.AllSpecials( "unpacker" ):
	if "extpack" in apiutil.ChromiumProps(func_name):
		print 'extern void crUnpackExtend%s(void);' % func_name
	else:
		print 'extern void crUnpack%s(void);' % func_name

print """
#endif
"""


