# coding=utf-8

import os
import sys

MuatetionNumInOneTest = 5
MuatetionNumForOneConfig = 5
Software = 'MySQL'
Software = str(sys.argv[1])

BugList = []

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

# split the string with '\n', and remove the null element
def mySplit(mystring):
	mystring = mystring.split('\n')
	mytest = [i for i in mystring if len(i) > 0]
	return mytest

# check whether the two result lists are same
# return value 
# 0  : Failed
# 1  : Successed
def checkTheLists(MutatedTestResult, OriginalTestResult, UpdateString):
	len1 = len(MutatedTestResult)
	len2 = len(OriginalTestResult)
	print len1,len2

	j = 0
	for i in range(len1):
		if MutatedTestResult[i].find("#####@@ This is Inserted by TestMytater! @@#####")!=-1: continue
		if MutatedTestResult[i].find(UpdateString)!=-1: continue
		
		if j>=len2 : return [0,"length is different",str(len2)]
		if MutatedTestResult[i] != OriginalTestResult[j]:
			return [0,OriginalTestResult[j],MutatedTestResult[i]]
		j = j+1
 
	return [1, "",""]

def findtheprestmt(TestResult, OperationList):
	pos = 0
	if len(OperationList) == 0: return -1

	for i in range(len(TestResult)):
		if TestResult[i].replace("\t","").replace(" ","").find(OperationList[pos].replace("\t","").replace(" ","")) != -1:
			pos = pos + 1
		if pos == len(OperationList):
			#print "nxt: ",TestResult[i]
			return i
	return -1

# check whether the two result files are same after the UpdateString
# return value 
# -1 : ERROR or Warning
# 0  : Failed
# 1  : Successed
# Note: maybe there are some false postive
def checkResult(MutatedTestResult, OriginalTestResult, UpdateString, config, OperationList):
	print "checkResult,",MutatedTestResult, OriginalTestResult
	MutatedTestResultFile = MutatedTestResult
	OriginalTestResultFile = OriginalTestResult

	UpdateString = UpdateString.replace('\n', '')
	MutatedTestResult = readfile(MutatedTestResult)
	OriginalTestResult = readfile(OriginalTestResult)

	MutatedTestResult = mySplit(MutatedTestResult)
	OriginalTestResult = mySplit(OriginalTestResult)


	pos1 = findtheprestmt(MutatedTestResult, OperationList)
	pos2 = findtheprestmt(OriginalTestResult, OperationList)
	print pos1, pos2

	if pos1 == -1:
		print "ERROR: Can not find UpdateString and NXT in MutatedTestResult!\n"
		return -1
	if pos2 == -1:
		print "ERROR: Can not find TheNextStmt in OriginalTestResult!\n"
		return -1
	res = checkTheLists(MutatedTestResult[pos1+1:], OriginalTestResult[pos2+1:], UpdateString)

	if res[0] == 0:
		BugList.append([config, OriginalTestResultFile, MutatedTestResultFile, res[1],res[2]])
	print res[1]
	print res[2]
	return res[0]

# for mysql
def getOperationListFromTestResult(config, ResultFile):
	UpdateString = getUpdateStringFromTestResult(config, ResultFile)
	Dir = "./"+Software+"/NewTestSuit/"+config+"/"
	pos = ResultFile.find("_StartWith_")
	TestFile = Dir + ResultFile[:pos] + ".test"
	ppos = ResultFile.find("_UpdateWith_")
	TestDir = Dir + ResultFile[:ppos] + "/" + ResultFile[:pos] + ".test"
	
	test = readfile(TestDir).replace("\r","")
	test = test.split('\n')
	test = [i for i in test if len(i) > 0 and i[len(i)-1]==';' and i[0]!='#' and i.find("delimiter")==-1]

	res = []
	for i in range(len(test)):
		if test[i].find(UpdateString) == -1:
			res.append(test[i])
		if test[i].find(UpdateString) != -1:
			if i+1 < len(test):
				res.append(test[i+1])
			break

	if len(res) > 20: return res[len(res)-20:]
	return res

def getUpdateStringFromTestResult(config, ResultFile):
	Dir = "./"+Software+"/NewTestSuit/"+config+"/"
	pos = ResultFile.find("_StartWith_")
	TestFile = Dir + ResultFile[:pos] + ".test"
	ppos = ResultFile.find("_UpdateWith_")
	TestDir = Dir + ResultFile[:ppos] + "/" + ResultFile[:pos] + ".test"
	
	test = readfile(TestDir).replace("\r","")
	test = mySplit(test)
	pos = -1
	for i in range(len(test)):
		if test[i].find('#####@@ This is Inserted by TestMytater! @@#####')!=-1:
			pos = i
	if pos > -1 and pos < len(test):
		return test[pos+1]
	else:
		return "ERROR when find UpdateString"

def isOriginalTest(ResultFile):
	if ResultFile.find("_UpdateWith_") == -1:
		return True
	else:
		return False

def getStartWithFromTestResult(ResultFile):
	pos = ResultFile.find("_StartWith_")
	Value = ResultFile[pos:]
	Value = Value.replace("_StartWith_", "")
	Value = Value.replace(".result", "")
	return Value

def getUpdateWithFromTestResult(ResultFile):
	pos = ResultFile.find("_UpdateWith_")
	Value = ResultFile[pos:]
	Value = Value.replace("_UpdateWith_", "")

	pos = Value.find("_mutated_")
	Value = Value[:pos]
	return Value

def checkOneTestResult(config, oneTest, oneTest_path):
	ResultFiles = os.listdir(oneTest_path)
	for ResultFile1 in ResultFiles: 
		if isOriginalTest(ResultFile1) == True:
			for ResultFile2 in ResultFiles: 
				if isOriginalTest(ResultFile2) == False:
					StartWith1 = getStartWithFromTestResult(ResultFile1)
					StartWith2 = getStartWithFromTestResult(ResultFile2)
					UpdateWith2 = getUpdateWithFromTestResult(ResultFile2)
					if StartWith1==UpdateWith2 and StartWith2 != UpdateWith2:
						UpdateString = getUpdateStringFromTestResult(config, ResultFile2)
						OperationList = getOperationListFromTestResult(config, ResultFile2)
						print "UpdateString=",UpdateString
						res = checkResult(oneTest_path+"/"+ResultFile2, oneTest_path+"/"+ResultFile1, UpdateString, config, OperationList)
						

def mainChecker():
	BugList = []

	Dir = "./" + Software + "/TestResult/"
	dir_or_files = os.listdir(Dir)
	for dir_file in dir_or_files:
		config_path = os.path.join(Dir, dir_file)
		if os.path.isdir(config_path):
			config = dir_file
			TestResultDirs = os.listdir(config_path)
			for oneTest in TestResultDirs:
				oneTest_path = os.path.join(config_path, oneTest)
				print oneTest_path
				if os.path.isdir(oneTest_path):
					print "check ", oneTest_path
 					checkOneTestResult(config, oneTest, oneTest_path)


def recordBugList(Path):
	res = ""
	for item in BugList:
		res = res + str(item) + "\n"
	writefile(res, Path, "w+")


mainChecker()
recordBugList("./alarm.txt")
