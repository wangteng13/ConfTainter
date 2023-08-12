# coding=utf-8

import os
import json
import math
import random

GenerationNumForOneConfig = 5
Software = 'MySQL'
Software = str(sys.argv[1])
option_list = str(sys.argv[2])

BugList = []

# read the file and return string
def readfile(filepath):
    fp = open(filepath, 'r')
    fileread = fp.read()
    fp.close()
    return fileread

# write the string into the file
def writefile(content, filepath, di):
    fp = open(filepath, di)
    fp.write(content)
    return fp.close()

def randomStr(constraint):
	length = len(constraint)
	old = constraint
	for i in range(length):
		if constraint[i].isdigit() == True:
			constraint = constraint.replace(constraint[i], str((int(constraint[i])+random.randint(1,9))%10))
			if random.randint(1,9) >= 5:
				break
	if old == constraint:
		sta = random.randint(0,length-1)
		end = random.randint(sta,length-1)
		return old[sta:end]
	else:
		return constraint

def main1():
	with open('./option_list.json','r')as fp:
		json_data = json.load(fp)

	res = ""

	for i in json_data:
		key = json_data[i]['key']
		Property = json_data[i]['property']
		Type = json_data[i]['type']
		constraint = json_data[i]['constraint']

		item = key + ", " + Property

		if Type == 'BOOL' or Type == 'ENUM':
			constraint = constraint.replace('[', '').replace(']', '')
			item = item  + ", " + constraint + "\n"
		elif Type == 'NUM':
			constraint = constraint.replace('[', '').replace(']', '').replace(' ', '')
			constraint = constraint.split(',')
			low  = int(constraint[0])
			high = int(constraint[1])
			item = item  + ", " + str(low)
			if high-low >= math.pow(10, GenerationNumForOneConfig):
				step = 10
				for j in range(GenerationNumForOneConfig):
					item = item  + ", " + str(low+step)
					step = step * 10
				item = item  + ", " + str(high) + "\n"

			else:
				step = (high-low)/GenerationNumForOneConfig
				for j in range(GenerationNumForOneConfig):
					item = item  + ", " + str(low+step)
					step = step + (high-low)/GenerationNumForOneConfig
				item = item  + ", " + str(high) + "\n"
		elif Type == 'STR':
			constraint = constraint.replace('[', '').replace(']', '').replace(' ', '')
			item = item  + ", " + constraint
			for j in range(3):
				item = item  + ", " + randomStr(constraint)
			item = item + "\n"

		res = res + item

	writefile(res, "./"+Software+"/ConfSpecList.txt", "w")

def main2():
	
	json_data = readfile(option_list)
	json_data = json_data.split("\n")

	res = ""

	for item in json_data:
		tmp = item.split(',', 3)
		key = tmp[0]
		Property = tmp[1]
		Type = tmp[2]
		constraint = tmp[3].replace('"','')

		if Property=='Global': Property = 0
		elif  Property=='Session': Property = 1
		elif  Property=='Both': Property = 2

		item = key + ", " + str(Property)

		if Type == 'BOOL' or Type == 'ENUM':
			constraint = constraint.replace('[', '').replace(']', '')
			item = item  + ", " + constraint + "\n"
		elif Type == 'NUM':
			constraint = constraint.replace('[', '').replace(']', '').replace(' ', '')
			constraint = constraint.split(',')
			low  = int(constraint[0])
			high = int(constraint[1])
			item = item  + ", " + str(low)
			if high-low >= math.pow(10, GenerationNumForOneConfig):
				step = 10
				for j in range(GenerationNumForOneConfig):
					item = item  + ", " + str(low+step)
					step = step * 10
				item = item  + ", " + str(high) + "\n"

			else:
				step = (high-low)/GenerationNumForOneConfig
				for j in range(GenerationNumForOneConfig):
					item = item  + ", " + str(low+step)
					step = step + (high-low)/GenerationNumForOneConfig
				item = item  + ", " + str(high) + "\n"
		elif Type == 'STR':
			constraint = constraint.replace('[', '').replace(']', '').replace(' ', '')
			item = item  + ", " + constraint
			for j in range(3):
				item = item  + ", " + randomStr(constraint)
			item = item + "\n"

		res = res + item

	writefile(res, "./"+Software+"/ConfSpecList.txt", "w")

main2()