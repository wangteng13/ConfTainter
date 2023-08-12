#include "tainter.h"
string getUniqueID(BasicBlock* bb) {
    Function*   func = bb->getParent();
    std::string uniqueID = "";
    StringRef    funcNameRef = func->getName();
    std::string  funcName    = funcNameRef.data();
    for(auto inst = bb->begin(); inst != bb->end(); inst++) {
        const DILocation* location = inst->getDebugLoc();
        if(location) {
            std::string directory = location->getDirectory(), filePath  = location->getFilename();
            int line = location->getLine();
            std::string firstInstName = directory + "/" + filePath + ":" + to_string(line);
            if(bb->hasName())
                uniqueID = funcName + "_" + firstInstName + "_" + bb->getName().data();
            else
                uniqueID = funcName + "_" + firstInstName + "_" + "NoName";
            return uniqueID;
        }
    }
}
bool recrusiveWriteInstBBID(struct GlobalVariableInfo *gv_info, string output_file, struct InstInfo* inst_info, set<string>& branchSet) {
    ofstream fout(output_file.c_str(), ios::app);
    string IR_str = getAsString(inst_info->InstPtr);
    if(judgeBranch(IR_str)) {
        BasicBlock* parent = inst_info->InstPtr->getParent();
        std::string uniqueID = getUniqueID(parent);
        if(branchSet.count(uniqueID) == 0)
            branchSet.emplace(uniqueID);
    }
    fout.close();
    fout.clear();

    bool ans = true;
    for(auto i=inst_info->Successors.begin(), e=inst_info->Successors.end(); i!=e; i++){
        struct InstInfo* next = *i;
        ans = ans && recrusiveWriteInstBBID(gv_info, output_file, next, branchSet);
        if(!ans) return ans;
    }
    return true;
}
bool recrusiveWriteFuncBBID(struct GlobalVariableInfo *gv_info, string output_file, struct FuncInfo* func_info, set<string>& branchSet){
    bool ans = true;
    for(auto i=func_info->ArgInstInfoList.begin(); i!=func_info->ArgInstInfoList.end(); i++) {
        struct InstInfo* inst_info = *i;
        ans = ans && recrusiveWriteInstBBID(gv_info, output_file, inst_info, branchSet);
        if(!ans) return ans;
    }
    for(auto i=func_info->InsideFuncInfoList.begin(); i!=func_info->InsideFuncInfoList.end(); i++) {
        struct FuncInfo* inner_func_info = *i;
        ans = ans && recrusiveWriteFuncBBID(gv_info, output_file, inner_func_info, branchSet);
        if(!ans) return ans;
    }
    return true;
}
bool writeBranchFuncBBID(struct GlobalVariableInfo *gv_info, string output_file, set<string>& branchSet) {
    bool ans = true;
    for(auto i=gv_info->FuncInfoList.begin(), e=gv_info->FuncInfoList.end(); i!=e; i++) {
        struct FuncInfo* func_info = *i;
        ans = ans && recrusiveWriteFuncBBID(gv_info, output_file, func_info, branchSet);
        if(!ans) return ans;
    }
    return true;
}
bool writeBranchInstBBID(struct GlobalVariableInfo *gv_info, string output_file, set<string>& branchSet) {
    bool ans = true;
    for(auto it = gv_info->InstInfoList.begin(); it != gv_info->InstInfoList.end(); it++) {
        struct InstInfo* inst_info = *it;
        ans = ans && recrusiveWriteInstBBID(gv_info, output_file, inst_info, branchSet);
        if(!ans) return ans;
    }
    return true;
}

int main(int argc, char** argv){
	string ir_file = string(argv[1]);	//input the ir path
    string var_file = string(argv[2]);	//input the variable mapping path
	string output_file = var_file.substr(0, var_file.find_last_of(".")) + "-Fuzzing-BBID.dat";
	std::vector<struct ConfigVariableNameInfo *> config_names;
    if (!readConfigVariableNames(var_file, config_names)) exit(1);
	
	std::unique_ptr<llvm::Module> module;
	LLVMContext context; SMDiagnostic Err;
	buildModule(module, context, Err, ir_file);	//build the llvm module based on ir
	std::vector<struct GlobalVariableInfo *> gvlist = getGlobalVariableInfo(module, config_names);	//conduct Configuration Variable Mapping
	startAnalysis(gvlist, true, false);		//Taint Analysis with implicit data-flow, and no implicit control-flow

	unordered_map<string, set<string>> hashmap;
    for (auto i = gvlist.begin(), e = gvlist.end(); i != e; i++){
        struct GlobalVariableInfo *gv_info = *i;
    	string config_name = gv_info->NameInfo->getNameAsString();
        if(hashmap.count(config_name) == 0) {
            set<string> branchSet;
            hashmap.emplace(config_name, branchSet);
        }
		writeBranchInstBBID(gv_info, output_file, hashmap[config_name]);
		writeBranchFuncBBID(gv_info, output_file, hashmap[config_name]);
    }
	return 0;
}