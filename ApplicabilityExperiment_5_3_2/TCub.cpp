#include "tainter.h"
void dumpInstInfo(struct GlobalVariableInfo *gv_info, string output_file, struct InstInfo* inst_info){
    ofstream fout(output_file.c_str(), ios::app);

    fout<<inst_info->InstLoc.toString()<< " [" << getOriginalName(inst_info->InstPtr->getFunction()->getName().str()) <<"]\n";
    fout.close();
    for(auto i=inst_info->Successors.begin(), e=inst_info->Successors.end(); i!=e; i++){
        struct InstInfo* next = *i;
        if(next->add_tab)
            dumpInstInfo(gv_info, output_file, next);
        else
            dumpInstInfo(gv_info, output_file, next);
    }
	fout.close();
	fout.clear();
}

int main(int argc, char** argv){
	string ir_file = string(argv[1]);	//input the ir path
    string var_file = string(argv[2]);	//input the variable mapping path
	std::vector<struct ConfigVariableNameInfo *> config_names;
    if (!readConfigVariableNames(var_file, config_names)) exit(1);
	
	std::unique_ptr<llvm::Module> module;
	LLVMContext context; SMDiagnostic Err;
	buildModule(module, context, Err, ir_file);	//build the llvm module based on ir
	std::vector<struct GlobalVariableInfo *> gvlist = getGlobalVariableInfo(module, config_names);	//conduct Configuration Variable Mapping
	startAnalysis(gvlist, true, false);		//Taint Analysis with implicit data-flow, and no implicit control-flow

	string output_file = var_file.substr(0, var_file.find_last_of(".")) + "-TCub-records.dat";
	string output_content = "";
	for (auto i = gvlist.begin(), e = gvlist.end(); i != e; i++){
		struct GlobalVariableInfo *gv_info = *i;
		for(auto j=gv_info->InstInfoList.begin(), en=gv_info->InstInfoList.end(); j!=en; j++){
			struct InstInfo* inst_info = *j;
			dumpInstInfo(gv_info, output_file, inst_info);
		}	
    }
	return 0;
}
