#!/usr/bin/python

# QuesoGLC
# A free implementation of the OpenGL Character Renderer (GLC)
# Copyright (c) 2002, 2004-2006, Bertrand Coconnier
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# $Id$

import sys, urllib, string

if len(sys.argv) > 1:
    numchars = int(sys.argv[1])
else:
    numchars = 256

print "Open URL..."
unicodata = urllib.urlopen("ftp://ftp.unicode.org/Public/UNIDATA/UnicodeData.txt")
print "Read data from URL..."
lignes = unicodata.readlines()
print "Close URL..."
unicodata.close()

print "Sort data..."
unicodeNameFromCode = {}
unicodeCodeFromName = {}
for s in lignes:
    liste = string.split(s, ';')
    code = eval('0x'+liste[0])
    name = liste[1]
    if name == '<control>':
        continue
    unicodeNameFromCode[code] = name
    unicodeCodeFromName[name] = code

liste = []
codes = unicodeNameFromCode.keys()
codes.sort()
vmax = max(codes[:numchars])
for c in codes[:numchars]:
    liste.append(unicodeNameFromCode[c])
liste.sort()
i = 0
codes = [-1]*(vmax+1)

sourceCode = ["/* This is automagically generated by buildDB.py */\n",
              '#include "internal.h"\n\n']

print "Write database.c"
db = open('database.c','w')
db.writelines(sourceCode)
db.write("const __GLCdataCodeFromName __glcCodeFromNameArray[] = {\n")
for name in liste:
    db.write('{ %d, "%s"},\n' % (unicodeCodeFromName[name], name))
    codes[unicodeCodeFromName[name]] = i
    i += 1
db.write("};\n")
db.write("const GLint __glcNameFromCodeArray[] = {\n")
for c in codes:
    db.write("%d,\n" % c)
db.write("};\n")
db.write("const GLint __glcMaxCode = %d;\n" % vmax)
db.write("const GLint __glcCodeFromNameSize = %d;\n" % len(liste))
db.close()

print "Success !!!"
