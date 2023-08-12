#include "tainter.h"
bool findInstructionIn(Instruction* ik1, vector<struct InstInfo*> &info_list1){
	for (auto p1 = info_list1.begin(); p1 != info_list1.end(); p1++){
		struct InstInfo* k1 = *p1;
		if(k1->InstPtr == ik1) return true;
	}
	return false;
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
	startAnalysis(gvlist, false, false);		//Taint Analysis with no implicit data-flow, and no implicit control-flow

	string output_file = var_file.substr(0, var_file.find_last_of(".")) + "-ValueRelationship-records.dat";
	ofstream fout(output_file, ios::app);
	for (auto i = gvlist.begin(), e = gvlist.end(); i != e; i++){
		for (auto j = i+1; j != e; j++){
			struct GlobalVariableInfo *gv_info1 = *i, *gv_info2 = *j;
			vector<struct InstInfo*> info_list1 = gv_info1->getExplicitDataFlow();
			vector<struct InstInfo*> info_list2 = gv_info2->getExplicitDataFlow();

			for (auto p1 = info_list1.begin(); p1 != info_list1.end(); p1++)
				for (auto p2 = info_list2.begin(); p2 != info_list2.end(); p2++){
					struct InstInfo *k1 = *p1, *k2 = *p2;
					if (k1->InstPtr != k2->InstPtr) continue;
					if(isa<CmpInst>(k1->InstPtr)){
						Instruction *ik1 = (Instruction*) k1->InstPtr->getOperand(0);
						Instruction *ik2 = (Instruction*) k1->InstPtr->getOperand(1);
						if( (findInstructionIn(ik1, info_list1)&&findInstructionIn(ik2, info_list2))
							|| (findInstructionIn(ik1, info_list2)&&findInstructionIn(ik2, info_list1))){						
							fout<<"GlobalVariable Name: "<<gv_info1->NameInfo->getNameAsString()<<"  "<<gv_info2->NameInfo->getNameAsString()<<"\n";
							fout<<getAsString(k1->InstPtr)<<"\nInstLoc: "<<k1->InstLoc.toString()<<"\n\n";
						}
					}
				}
		}
    }
	fout.close();
	fout.clear();
	return 0;
}