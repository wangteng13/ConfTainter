import os
import subprocess
import shutil


def listdir(path, list_name):
    for file in os.listdir(path):
        file_path = os.path.join(path, file)
        if os.path.isfile(file_path) and file_path.endswith(".test"):
            list_name.append(file)

def writefile(content, filepath, di):
    fp = open(filepath, di)
    fp.write(content)
    return fp.close()


path = "./TestSuit/"
res = []
listdir(path, res)
strr = ""

for tt in res:
	strr = strr + str(tt) + "\n"

writefile(strr, "./SuitList.txt","w+" )