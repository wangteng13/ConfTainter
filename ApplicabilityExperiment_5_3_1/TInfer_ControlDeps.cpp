#include "tainter.h"
int main(int argc, char** argv){
	string ir_file = string(argv[1]);	//input the ir path
    string var_file = string(argv[2]);	//input the variable mapping path
	std::vector<struct ConfigVariableNameInfo *> config_names;
    if (!readConfigVariableNames(var_file, config_names)) exit(1);
	
	std::unique_ptr<llvm::Module> module;
	LLVMContext context; SMDiagnostic Err;
	buildModule(module, context, Err, ir_file);	//build the llvm module based on ir
	std::vector<struct GlobalVariableInfo *> gvlist = getGlobalVariableInfo(module, config_names);	//conduct Configuration Variable Mapping
	startAnalysis(gvlist, false, false);		//Taint Analysis with no implicit data-flow, and no implicit control-flow

	string output_file = var_file.substr(0, var_file.find_last_of(".")) + "-ControlDependency-records.dat";
	ofstream fout(output_file, ios::app);
	string output_content = "";
	int depsNum = 0;
	for (auto i = gvlist.begin(), e = gvlist.end(); i != e; i++){
		for (auto j = gvlist.begin(); j != e; j++) if(i!=j){
			struct GlobalVariableInfo *gv_info1 = *i, *gv_info2 = *j;
			vector<struct InstInfo*> info_list1 = gv_info1->getExplicitDataFlow();
			vector<struct InstInfo*> info_list2 = gv_info2->getExplicitDataFlow();
			int flag = 0;
			if((gv_info1->NameInfo->OriginalConfigName=="boolean" || gv_info1->NameInfo->OriginalConfigName=="enum"))
			for (auto p1 = info_list1.begin(); p1 != info_list1.end(); p1++){
				struct InstInfo *k1 = *p1;
				if (k1->isControllingInst && flag==0)
				for (auto p2 = info_list2.begin(); p2 != info_list2.end(); p2++){
					struct InstInfo *k2 = *p2;
					vector<BasicBlock*> taintedBBs = k1->getControllingBBs();
					BasicBlock* parent = k2->InstPtr->getParent();
					if(std::find(taintedBBs.begin(), taintedBBs.end(), parent) != taintedBBs.end()){
						flag = 1;
						output_content += "\n Control Deps between GlobalVariable Name: "
											+ gv_info1->NameInfo->getNameAsString() + "  " + gv_info2->NameInfo->getNameAsString() + "\n"
											+ getAsString(k1->InstPtr) + "\nInstLoc: " + k1->InstLoc.toString() + "\n"
											+ getAsString(k2->InstPtr) + "\nInstLoc: " + k2->InstLoc.toString() + "\n";
					}
				}
			}
			depsNum += flag;
		}
    }
	fout<<"Found "<<to_string(depsNum)<<" Control Deps.\n\n"<<output_content;
	fout.close();
	fout.clear();
	return 0;
}