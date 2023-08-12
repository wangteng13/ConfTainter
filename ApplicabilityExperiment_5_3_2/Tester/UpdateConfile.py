# coding=utf-8

import os
import subprocess
import shutil
import sys
from shutil import move


# read the file and return string
def readfile(filepath):
    fp = open(filepath, 'r')
    fileread = fp.read()
    fileread = fileread.replace("\r", "")
    fp.close()
    return fileread

# write the string into the file
def writefile(content, filepath, di):
    fp = open(filepath, di)
    fp.write(content)
    return fp.close()


def replacewith(lines, key, value):
	res = []
	flag = 0
	for line in lines:
		if line.find( str(key) ) != -1 and flag == 0:
			ss = str(key) + " " + str(value) + "\n"
			res.append(ss)
			flag = 1
		else:
			res.append(line)
	return res

def updateDefaultConfigFile(ConfigDir, key, value):
	try:
		with open( ConfigDir, 'r' ) as fp:
			lines = fp.readlines()
			lines = replacewith(lines, key, value)

		with open( ConfigDir+"_tmp", 'w' ) as fp:
			fp.writelines( lines )
		move(ConfigDir+"_tmp", ConfigDir)
	except IOError:
		print "update the DefaultConfigFile Error!"


if len(sys.argv) != 4:
	print "argv is not 4!"
else:
	file = sys.argv[1]
	key = sys.argv[2]
	value = sys.argv[3]
	updateDefaultConfigFile(file, key, value)


