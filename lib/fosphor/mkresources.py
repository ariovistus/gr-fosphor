#!/usr/bin/env python

"""
Packs a given file list into a .c file to be included in the executable
directly.


Copyright (C) 2013 Sylvain Munaut

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

import sys


class ResourcePacker(object):

	def header(self):
		return [
			"/* AUTO GENERATED - DO NOT MODIFY BY HAND */",
			"#include \"resource_internal.h\"",
		]

	def _file_wrap(self, name, len_, data_lines):
		a = [
			"\t{",
			"\t\t\"%s\"," % name,
		]

		if len(data_lines) > 1:
			b = [ "\t\t" ]
			for l in data_lines:
				b.append("\t\t\t" + l)
			b[-1] += ","
		else:
			b = [ "\t\t.data = %s," % data_lines[0] ]

		c = [
			"\t\t%d," % len_,
			"\t},",
		]

		return a+b+c

	def _wrap_str(self, s):
		m = {
			'\n' : '\\n',
			'\t' : '\\t',
			'\0' : '\\0',
			'\\' : '\\\\',
			'"'  : '\\"',
		}

		sa = []
		for c in s:
			if c in m:
				sa.append(m[c])
			elif ord(c) < 32:
				sa.append('\\x%02x' % ord(c))
			else:
				sa.append(c)

		return ''.join(sa)

	def file_text(self, name, content):
		dl = ['\"' + self._wrap_str(l) + '\"' for l in content.splitlines(True)]
		return self._file_wrap(name, len(content), dl)

	def file_binary(self, name, content):
		dl = []
		for i in range(0, len(content), 16):
			l = content[i:i+16]
			s = '"'
			for c in l:
				s += '\\x%02x' % ord(c)
			s += '"'
			dl.append(s)
		return self._file_wrap(name, len(content), dl)
	
	def file_char_array(self, name, content, arrname):
		dl = []
		dl.append("char %s[] = {\n" % arrname)
		line = ""
		for i in xrange(len(content)):
			if len(line) > 70:
				dl.append(line + "\n")
				line = ""
			line += "0x%x," % ord(content[i])
		dl.append(line + "0 \n")
		dl.append("};\n");
		return dl
	def file_char_ref(self, name, content, arrname):
		return [
			"\t{",
			"\t\t\"%s\"," % name,
			"\t\t%s," % arrname,
			"\t\t%d," % len(content),
			"\t},",
		]


	def footer(self):
		return [
			"\t/* Guard */",
			"\t{ (void*)0 }",
			"};",
		]

	def process(self, filenames):
		import platform
		b = [
			"struct resource_pack __resources[] = {",
			]
		i = 0
		for f in filenames:
			fh = open(f, 'rb')
			c = fh.read()
			if platform.system() == "Windows" and len(c) > 65000:
				nom = "arr%s" % i
				bb = self.file_char_array(f, c, nom)
				b = bb + b
				b.extend(self.file_char_ref(f, c, nom))
				i += 1
			elif '\x00' in c:
				b.extend(self.file_binary(f, c))
			else:
				b.extend(self.file_text(f, c))
			fh.close()
		b.extend(self.footer())
		b = self.header() + b
		return b;


def main(self, *files):
	p = ResourcePacker()
	r = p.process(files)
	print '\n'.join(r)


if __name__ == '__main__':
	main(*sys.argv)
