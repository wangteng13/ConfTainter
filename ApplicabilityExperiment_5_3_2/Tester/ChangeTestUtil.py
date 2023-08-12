# coding=utf-8

import os
import sys
import subprocess
import shutil
from shutil import move



if len(sys.argv) == 3:

	TestHelpDir = str(sys.argv[1])
	testName = str(sys.argv[2])

	try:
		with open( TestHelpDir, 'r' ) as fp:
			lines = fp.readlines()
			for line in lines:
				if "RedisTest" in line :
					new_line = "RedisTest/" + testName + "\n"
					lines[lines.index( line )] = new_line
					break

		with open( TestHelpDir, 'w' ) as fp:
			fp.writelines( lines )
	except IOError:
		print "change test_helper.tcl Error!"
