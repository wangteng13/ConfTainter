# coding=utf-8

import os
import random
import sys

MuatetionNumInOneTest = 5
MuatetionNumForOneConfig = 10
Software = 'MySQL'
Software = str(sys.argv[1])

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

# create an updateString for MySQL
# set global xx = ;
# set session xx = ;
def createUpdateString(ConfName, ConfValue, Property):
	# Property: global is 0, session is 1, both is 2
	UpdateString = "#####@@ This is Inserted by TestMytater! @@#####\nset " 

	if str(Property) == str(0): UpdateString = UpdateString + "global "
	else: UpdateString = UpdateString + "session "
	UpdateString = UpdateString + ConfName + " = " + str(ConfValue) + ";"

	#print UpdateString
	return UpdateString

# check whether to insert an updateString just before the con stmt 
def checkCanInsert(con, Software):
	con = con.replace("\n","").replace("\r","")
	if Software == "MySQL":
		if len(con)>=2 and con[0]!='#' and con[0]!='\n' and con[0]!='\r':
			return True
		return False

# insert the UpdateString into TestContent at pos
# return the string
def insertAt(UpdateString, pos, TestContent):
	res = ""
	for i in range(len(TestContent)):
		res = res + TestContent[i] + ";\n"
		if pos == i:
			res = res + UpdateString + "\n"
	return res


# create newTest with the UpdateString
# insert times are limited by MuatetionNumInOneTest
def createNewTest(OriginalTest, UpdateString):
	TestContent = readfile("./"+Software + "/TestSuit/" + OriginalTest)
	TestContent = TestContent.split(';\n')

	result = []
	temp = []
	for i in range(0,len(TestContent)):
		con = TestContent[i]
		# can not insert stmt before these stmts
		if i+1<len(TestContent) and TestContent[i+1].lower().find("show warnings")!=-1: continue
		if i+1<len(TestContent) and TestContent[i+1].lower().find("show errors")!=-1: continue
		if i+1<len(TestContent) and TestContent[i+1].lower().find("get diagnostics")!=-1: continue
		if checkCanInsert(con, Software):
			temp.append(i)

	if len(temp) <= MuatetionNumInOneTest:
		for i in range(len(temp)):
			result.append(insertAt(UpdateString, temp[i], TestContent))
	else:
		step = len(temp)/MuatetionNumInOneTest
		st = 0
		end = step
		for i in range(MuatetionNumInOneTest):
			pos = random.randint(st, end)
			if pos >= len(temp): break
			result.append(insertAt(UpdateString, temp[pos], TestContent))
			st = st + step
			end = end + step

	return result
	
# check whether the Test is related with configA
def isConfRelatedTest(config, TestSuit):
	return True

# TestSuit is like user.test
# return the name before '.test'
def modify(TestSuit):
	res = TestSuit.split('.')
	return res[0]

def makeUpdateStringList(config):
	print config
	res = []
	for i in range(len(config[2])):
		if i >= MuatetionNumForOneConfig: break;
		UpdateString = createUpdateString(config[0], str(config[2][i]), config[1])
		res.append(UpdateString)
	return res
	

# read the ConfSpecListfile content, and get speclist
# [confname, Property, v1, v2, v3]
def getConfSpecList(ConfSpecList):
	res = []
	ConfSpecList = ConfSpecList.replace("\r", "")
	ConfSpecList = ConfSpecList.split('\n')
	for spec in ConfSpecList:
		sp = []
		valuelist = []
		temp = spec.split(',')
		for i in range(len(temp)):
			tv = temp[i]
			tv = tv.replace(' ', '')
			if i==0 or i==1: sp.append(tv)
			else: valuelist.append(tv)
		sp.append(valuelist)
		if len(sp) <=2: continue
		res.append(sp)
	return res


def main():
	TestSuitList = readfile("./"+Software + "/TestSuitList.txt")
	TestSuitList = TestSuitList.replace("\r","").split('\n')
	ConfSpecList = readfile("./"+Software + "/ConfSpecList.txt")
	ConfSpecList = getConfSpecList(ConfSpecList)

	for config in ConfSpecList:
		for TestSuit in TestSuitList:
			if len(TestSuit) < 1: continue
			if isConfRelatedTest(config, TestSuit):
				UpdateStringList = makeUpdateStringList(config)
				for j in range(len(config[2])):
					if j >= MuatetionNumForOneConfig: break
					uv = config[2][j]
					UpdateString = createUpdateString(config[0], str(config[2][j]), config[1])
					NewTestList = createNewTest(TestSuit, UpdateString)

					configDir = "./" + Software + "/NewTestSuit/" + config[0]
					configTestDir = "./" + Software + "/NewTestSuit/" + config[0] + "/" + TestSuit.replace(".test","").replace("\r","")
					if os.path.exists("./" + Software + "/NewTestSuit/") == False: os.mkdir("./" + Software + "/NewTestSuit/")
					if os.path.exists(configDir) == False: os.mkdir(configDir)
					if os.path.exists(configTestDir) == False: os.mkdir(configTestDir)

					NewTestNum = 0
					for NewTest in NewTestList:
						writefile(NewTest, configTestDir + "/" + modify(TestSuit) + "_UpdateWith_" + str(config[2][j]) + "_mutated_" + str(NewTestNum) + ".test", "w")
						NewTestNum = NewTestNum + 1


main()




