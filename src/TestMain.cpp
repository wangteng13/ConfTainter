#include "TInfer.h"

int main(int argc, char** argv){
	string ir_file = string(argv[1]);	//input the ir path
	string var_file = string(argv[2]);	//input the variable mapping path
	std::vector<struct ConfigVariableNameInfo *> config_names;
	if (!readConfigVariableNames(var_file, config_names)) exit(1);
	
	std::unique_ptr<llvm::Module> module;
	LLVMContext context; SMDiagnostic Err;
	buildModule(module, context, Err, ir_file);	//build the llvm module based on ir
	std::vector<struct GlobalVariableInfo *> gvlist = getGlobalVariableInfo(module, config_names);	//conduct Configuration Variable Mapping
	startAnalysis(gvlist, true, true);		//Taint Analysis with indirect data-flow, and indirect control-flow

	string output_file = var_file.substr(0, var_file.find_last_of(".")) + "-ControlDependency-records.dat";
	ofstream fout(output_file, ios::app);
	
	for (auto i = gvlist.begin(), e = gvlist.end(); i != e; i++)
	{
		struct GlobalVariableInfo *gv_info = *i;
		vector<struct InstInfo *> ExplicitDataFlow = gv_info->getExplicitDataFlow();
		llvm::outs() <<"getExplicitDataFlow size= "<<ExplicitDataFlow.size()<<"\n";
		vector<struct InstInfo *> ImplicitDataFlow = gv_info->getImplicitDataFlow();
		llvm::outs() <<"getImplicitDataFlow size= "<<ImplicitDataFlow.size()<<"\n";
	}
	return 0;
}

