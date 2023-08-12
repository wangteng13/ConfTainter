# coding=utf-8

import os
import random
import re
import sys

Logfile1 = str(sys.argv[1])
Logfile2 = str(sys.argv[1])

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

def getSet(Logfile):
	content = readfile(Logfile)
	content = content.replace("\r", "")
	content = content.split("\n")
	VariableList = []
	pattern = "Print \S+ in \S+ = \S+"
	for log in content:
		matchObj = re.match(pattern, log, 0)
		if matchObj:
			VariableList.append(log)

	MyDict = {}
	for log in VariableList:
		pos = log.find(" = ")
		if pos == -1:
			continue
		key = log[:pos]
		value = log[pos+3:]

		if MyDict.has_key(key):
			MyDict[key].add(value)
		else:
			MyDict[key] = set(value)
	return MyDict

def main():
	res = ""
	MyDict1 = getSet(Logfile1)
	MyDict2 = getSet(Logfile2)
	for it in MyDict1.keys():
		if MyDict2.has_key(it):
			if MyDict1[it] != MyDict2[it]:
				res = res + it + "\n" + str(MyDict1[it]) + "\n" + str(MyDict2[it]) + "\n\n"
	writefile(res, "./alarm2.txt", "w+")

main()
