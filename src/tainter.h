#ifndef TAINTER_H
#define TAINTER_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm-c/IRReader.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/ADT/BreadthFirstIterator.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/ADT/StringRef.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <list>
#include <string>
#include <sstream>
#include <ctime>
#include <cxxabi.h>
#include <typeinfo> //for 'typeid' to work  
#include <unistd.h>
#include <unordered_map>


using namespace llvm;
using namespace std;


#define _ERROR_LEVEL        0
#define _WARNING_LEVEL      1
#define _DEBUG_LEVEL        2
#define _REDUNDENCY_LEVEL   3 


extern unsigned debug_level;
extern Module* M;

#define MY_DEBUG( level, x)  if( debug_level >= level ) x

#define NO_OFFSET   1024*1024   // represent Single Variable without offset similar to struct-like
#define ERR_OORANGE     1024*1024   // represent out of range error in iterate vector


bool readConfigVariableNames( std::string, std::vector< struct ConfigVariableNameInfo* >&);
vector<string> splitWithTag( string, string);
bool findConfigVariable(std::vector<struct ConfigVariableNameInfo* >, GlobalVariable*, std::vector<struct GlobalVariableInfo*>&);
bool handleDIType(DIType*, std::vector<struct ConfigVariableNameInfo* >, GlobalVariable*, std::vector<struct GlobalVariableInfo*>&);
void handleUser(Value*, struct GlobalVariableInfo *, struct InstInfo* , unsigned);
void handleInstruction(Value*, struct GlobalVariableInfo *, struct InstInfo* , unsigned);
bool findVisitedInstruction(struct InstInfo* , struct InstInfo*);
struct SrcLoc getSrcLoc(Instruction*);
bool comesBefore(Instruction* , Instruction* );
void collectBFSReachableBB(vector<BasicBlock*>&, BasicBlock*);
void collectBFSReachableBB(vector<BasicBlock *>&, BasicBlock*, BasicBlock*);
void collectDFSReachableBB( vector<BasicBlock*>&, BasicBlock*);

vector<User*> getSequenceUsers(Value*);
void printSequenceUsers(Value*);
vector<Use*> getSequenceUses_OnlyGEPins(Value*);
unsigned findLastOf(Instruction*, vector<struct LSPair*>);
string getOriginalName(string);
string getStructTypeStrFromPrintAPI(Type*);
vector<unsigned> getIndexFromGEPO(GEPOperator*);
bool isMatchedGEPOperator(GEPOperator*, struct GlobalVariableInfo*, int level);
void printTabs(unsigned);
unsigned getFuncArgIndex(CallBase*, Value*);
void traceFunction(struct FuncInfo*, uint level);
unsigned isFuncInfoRecorded(struct FuncInfo*, vector<struct FuncInfo*>);
void traceUser( Value*, struct FuncInfo*, struct InstInfo*);
string getAsString(Value*);
string getClassType(Value*);

// provide api for scalablity
int buildModule(std::unique_ptr<llvm::Module> &module, LLVMContext &context, SMDiagnostic &Err, string ir_file);
struct ConfigVariableNameInfo* readConfigVariableNameByOne(string buf_str);
std::vector<struct GlobalVariableInfo *> getGlobalVariableInfo(std::unique_ptr<llvm::Module> &module, string var_string);
std::vector<struct GlobalVariableInfo *> getGlobalVariableInfo(std::unique_ptr<llvm::Module> &module, std::vector<struct ConfigVariableNameInfo *> config_names);
int startAnalysis(std::vector<struct GlobalVariableInfo *> gv_info_list, bool isAnalysisImplicitData, bool isAnalysisImplicitControl);

struct LSPair
{
    StoreInst* _StoreInst;
    LoadInst* _LoadInst;
    Value* StoreAddr;

    LSPair() {}

    LSPair(StoreInst* _store_inst){
        this->_StoreInst = _store_inst;
        this->_LoadInst = nullptr;
        this->StoreAddr = _store_inst->getPointerOperand();
    }
};


struct SrcLoc
{
    std::string Filename;
    std::string Dirname;
    unsigned Line;
    unsigned Col;

    unsigned Count; // used to record the visit times of this SrcLoc, mainly consider dirname/filename:line, no col.

    SrcLoc() {}

    SrcLoc(std::string _filename, std::string _dirname, unsigned _line, unsigned _col)
    {
        this->Filename = _filename;
        this->Dirname = _dirname;
        this->Line = _line;
        this->Col = _col;
        this->Count = 1;
    }

    void operator = (SrcLoc _loc)
    {
        this->Filename = _loc.Filename;
        this->Dirname = _loc.Dirname;
        this->Line = _loc.Line;
        this->Col = _loc.Col;
    };
    
    bool operator == (SrcLoc _loc)
    {
        if(this->Filename == _loc.Filename &&
           this->Dirname == _loc.Dirname &&
           this->Line == _loc.Line &&
           this->Col == _loc.Col)
        {
            return true;
        }
        return false;
    };

    // is a valid location in source file?
    bool isValid()
    {
        if( this->Filename.length() == 0 || 
            this->Dirname.length() == 0 || 
            this->Line == 0 || this->Col == 0)
        {
            return false;
        }
        return true;
    }

    void print(unsigned tabs)
    {
        for(unsigned i=0; i<tabs; i++)
            llvm::outs()<<"  ";
          
        if(Dirname.length() == 0)
            llvm::outs()<<                       this->Filename << ":" << this->Line << "\n";
        else
            llvm::outs()<< this->Dirname <<"/"<< this->Filename << ":" << this->Line << "\n";
    }

    string toString()
    {
        if(Dirname.length() == 0)
            return                   this->Filename+":"+std::to_string(this->Line)+":"+std::to_string(this->Col);
        else
            return this->Dirname+"/"+this->Filename+":"+std::to_string(this->Line)+":"+std::to_string(this->Col);
    }

    bool dirHasString(string keyword)
    {
        std::size_t found = this->Dirname.find(keyword);
        if(found != std::string::npos)
            return true;
        else
            return false;
    }

    bool filenameHasString(string keyword)
    {
        std::size_t found = this->Filename.find(keyword);
        if(found != std::string::npos)
            return true;
        else
            return false;
    }

};


enum VariableType
{
    SINGLE,
    STRUCT,
    CLASS,
    FIELD
};


struct ConfigVariableNameInfo
{
    VariableType VarType;
    //for SINGLE Type
    string SingleName;
    //for STRUCT Type
    vector<string> StructName;
    //for CLASS Type
    vector<string> ClassName;
    //for FIELD Type
    vector<string> FieldName;

    string OriginalConfigName;
    bool isDynamicConfigurable;
    string DynamicConfigurableStatus;

    ConfigVariableNameInfo()
    {
        this->VarType = SINGLE;
        this->SingleName = "";
        this->StructName.clear();
        this->ClassName.clear();
        this->FieldName.clear();
        this->OriginalConfigName = "";
        this->DynamicConfigurableStatus = "";
    }

    string getNameAsStringPrefix()
    {
        string name;
        if(this->VarType == SINGLE)
            name = this->SingleName;
        else if( this->VarType == STRUCT)
            name = this->StructName[0];
        else if( this->VarType == CLASS)
            name = this->ClassName[0];
        else if( this->VarType == FIELD)
            name = this->FieldName[0];
        else
            name = "UNSUPPORTED type";
        return name;
    }

	string getConfNameAsString()
	{
		return this->OriginalConfigName;
	}

    string getNameAsString()
    {
        if(this->VarType == SINGLE)
            return this->SingleName;
        else if( this->VarType == STRUCT)
        {
            string name = this->StructName[0];
            for(unsigned i=1; i<this->StructName.size(); i++)
            {
                name = name+"."+this->StructName[i];
            }
            return name;
        }
        else if( this->VarType == CLASS)
        {
            string name = this->ClassName[0];
            for(unsigned i=1; i<this->ClassName.size(); i++)
            {
                name = name+"."+this->ClassName[i];
            }
            return name;
        }
        else if( this->VarType == FIELD)
        {
            string name = this->FieldName[0];
            for(unsigned i=1; i<this->FieldName.size(); i++)
            {
                name = name+"."+this->FieldName[i];
            }
            return name;
        } else {
            return "UNSUPPORTED type";
        }
    }

	string getDetailNameAsString()
	{
		return this->getNameAsString()+"(" + this->getConfNameAsString() + ")";
	}

    void operator = (ConfigVariableNameInfo _name_info)
    {
        this->VarType = _name_info.VarType;
        this->SingleName = _name_info.SingleName;
        this->StructName = _name_info.StructName;
        this->ClassName = _name_info.ClassName;
        this->FieldName = _name_info.FieldName;
    };

    bool isValid()
    {
        if( this->SingleName.length() == 0 ||
            this->StructName.size() == 0 ||
            this->ClassName.size() == 0 ||
            this->FieldName.size() == 0 )
        {
            return false;
        } else 
            return true;
    }

    void print(unsigned tabs)
    {
        for(unsigned i=0; i<tabs; i++)
            llvm::outs()<<"  ";
        llvm::outs()<<this->getNameAsString()<<"\n";
    }
};


enum InfluenceLevelType
{
    STRONG,
    WEAK
};


struct InstInfo
{
    Instruction* InstPtr;
    struct SrcLoc InstLoc;
    InfluenceLevelType InfluenceLevel;

    bool add_tab = true;
    bool isDirect = true;

    vector<struct InstInfo *> Successors;
    vector<struct InstInfo *> IndirectSuccessors;
    struct InstInfo* Predecessor;

    /// for Control Flow information.
    bool isControllingInst;
    vector<BasicBlock*> ControllingBBs;
    vector<BasicBlock*> weakControllingBBs;

    vector<Function*> ControllingFuncs;

    InstInfo() {this->isDirect = true;}

    InstInfo(Instruction* _inst, struct SrcLoc _loc)
    {
        this->InstPtr = _inst;
        this->InstLoc = _loc;
        this->InfluenceLevel = STRONG;
        this->isControllingInst = false;
		this->isDirect = true;
    }

    InstInfo(Instruction* _inst, struct SrcLoc _loc, bool _add_tab)
    {
        this->InstPtr = _inst;
        this->InstLoc = _loc;
        this->InfluenceLevel = STRONG;
        this->isControllingInst = false;
        this->add_tab = _add_tab;
		this->isDirect = true;
    }

    void setControllingBBs(vector<BasicBlock*> _bbs)
    {
        for(unsigned i=0; i<_bbs.size(); i++)
			if(std::find(this->ControllingBBs.begin(), this->ControllingBBs.end(), _bbs[i]) == this->ControllingBBs.end())
        	{
            	this->ControllingBBs.push_back(_bbs[i]);
        	}
    }
	vector<BasicBlock*> getControllingBBs()
	{
		return this->ControllingBBs;
	}
	vector<BasicBlock*> getImplicitControllingBBs()
	{
		return this->weakControllingBBs;
	}
	vector<Function*> getControlledFunction()
	{
		return this->ControllingFuncs;
	}

    void addControllingFuncs(Function* _func)
    {
        this->ControllingFuncs.push_back(_func);
    }

    bool operator ==(InstInfo* _inst_info)
    {
        if( this->InstPtr == _inst_info->InstPtr)
            return true;
        return false;
    }
};


enum InfluenceStatus
{
    UNDEFINE,
    YES,
    NO
};


struct FuncInfo
{
    Function* Ptr;
    string FuncName;
    unsigned ArgIndex;    // which arg of this function is the User.

    InfluenceStatus hasInsideDataFlowInfluence;    // Whether the ArgIndex's argument has dataflow to return inside the Function.

    std::vector<string> CallLocList;

    vector<struct InstInfo*> ArgInstInfoList;   // the instruction list about the dataflow of target Argument.
    vector<struct FuncInfo*> InsideFuncInfoList;    // if there are other calls inside this function, we record it

    FuncInfo() {}

    FuncInfo(Function* _ptr, string _name, unsigned _index, string _callloc)
    {
        this->Ptr = _ptr;
        this->FuncName = _name;
        this->ArgIndex = _index;
        this->hasInsideDataFlowInfluence = UNDEFINE;
        this->CallLocList.clear();
        this->CallLocList.push_back(_callloc);
    }

    void print(unsigned tabs)
    {
        for(unsigned i=0; i<tabs; i++)
            llvm::outs()<<"  ";
        llvm::outs()<<this->FuncName<<"\n";
    }

    void printDetail(unsigned tabs)
    {
        for(unsigned i=0; i<tabs; i++)
            llvm::outs()<<"  ";
        llvm::outs()<<this->FuncName<<"\n";
        for(unsigned j=0; j<this->CallLocList.size(); j++)
        {
            for(unsigned i=0; i<tabs+1; i++)
                llvm::outs()<<"  ";
            llvm::outs()<<this->CallLocList[j]<<"\n";
        }
    }
};


struct GlobalVariableInfo
{
    struct ConfigVariableNameInfo* NameInfo;
    GlobalVariable* Ptr;
    string GlobalVariableType;  // name info of 
    vector<unsigned> Offsets;   // indicate the offset of struct field in GEPOperator

    // every first-user (which is usually an instruction) of this gv(configuration variable)
    std::vector<struct InstInfo *> InstInfoList;

    // all tainted called function starting from each occurance (according to each one in 
    // `InstInfoList`) of gv.
    std::vector<struct FuncInfo *> FuncInfoList;

    /// TODO: add a mapping between `InstInfoList` and `FuncInfoList` to be more human-friendly.

    // ALL GEPinstruction to make the anaylze field-sensitive. Trade accuracy for completeness.
    // That is to say, some taint by GEPinstruction anaylze may be false postive.
    vector< pair< pair<Type *, vector<int>> , InstInfo* >> GEPInfoList; 

    std::vector<GlobalVariable*> InfluencedGVList;

    // for every element in `InstInfoList`, it has a set of tainted (or influenced) function.
    // These functions may need no further traing. They are regarded as the final point for 
    // the forward tainting.
    std::map<string, vector<Function *>> InfluencedFuncList;

    // this is for the recording process of `InfluencedFuncList`
    Function* currentGVStartingFunc = nullptr;
    string currentGVStartingFuncName;

    // for every element in `InstInfoList`, it has a parent function (->getParent()->getParent()), 
    // stored here.
    std::vector<Function*> CallerFunctionList;

    GlobalVariableInfo() {}

    GlobalVariableInfo(struct ConfigVariableNameInfo* _name_info, GlobalVariable* _ptr, vector<unsigned> _offsets)
    {
        this->NameInfo = _name_info;
        this->Ptr = _ptr;
        if(_ptr)
            this->GlobalVariableType = getStructTypeStrFromPrintAPI(_ptr->getType());
        for(auto it=_offsets.begin(); it!=_offsets.end(); it++)
        {
            this->Offsets.push_back(*it);
        }
        this->InstInfoList.clear();
        this->FuncInfoList.clear();
    }

    void collectCallerFunction()
    {
        for(unsigned i=0; i<this->InstInfoList.size(); i++)
        {
            struct InstInfo* inst_info = InstInfoList[i];
            Function* func = inst_info->InstPtr->getFunction();
            if( ! func)
                continue;
            if( std::find(this->CallerFunctionList.begin(), this->CallerFunctionList.end(), func) == this->CallerFunctionList.end())
            {
                this->CallerFunctionList.push_back(func);
            }
        }
    }

    vector< std::pair< Function*, Function*> > collectOutsideCallerFunctions()
    {
        vector<std::pair<Function*, Function*>> outside_caller_func_list;
        for(auto I=M->begin(); I!=M->end(); I++)
        {
            Function* func = &*I;
            if(! func)
                continue;

            for(auto i=inst_begin(func); i!=inst_end(func); i++)
            {
                Instruction* inst = &*i;
                if(CallBase* call = dyn_cast<CallBase>(inst))
                {
                    Function* cur_func = call->getCalledFunction();
                    for(auto it=this->CallerFunctionList.begin(); it!=this->CallerFunctionList.end(); it++)
                    {
                        Function* inside_func = *it;
                        if( inside_func == cur_func)
                        {
                            outside_caller_func_list.push_back(std::pair< Function*, Function*>(inside_func, func));
                        }
                    }
                }
            }
        }
        return outside_caller_func_list;
    }


    void printNameInfo(unsigned tabs)
    {
        for(unsigned i=0; i<tabs; i++)
            llvm::outs()<<"  ";
        llvm::outs()<<"GlobalVariable Name: "<<this->NameInfo->getNameAsString();
        
        if( ! this->Offsets.empty() )
            llvm::outs()<<"\tOffset: ";
        for(unsigned i=0; i<this->Offsets.size(); i++)
        {
            if( this->Offsets[i] != NO_OFFSET)
                llvm::outs()<<this->Offsets[i]<<" ";
        }
        llvm::outs()<<"\n";
    }


    void dumpInstInfo(string output_file, struct InstInfo* inst_info, unsigned level)
    {
        
        ofstream fout(output_file.c_str(), ios::app);
        if(! fout)
        {
            llvm::outs()<<"Open output file "<<output_file<<" FAILED!\n";
            return;
        }

        // write pointer OR control-flow arrow.
        if(!inst_info->add_tab){
            for(unsigned i=0; i<level; i++)
                fout<<"\t";
            fout << "↓\t↓\t↓\t↓\t↓ control-flow OR field-sensitive-dependency (no `ComesBefore` relation cross functions.)\n";
        }

        // write instruction to file
        for(unsigned i=0; i<level; i++)
            fout<<"\t";
        fout<<getAsString(inst_info->InstPtr);
        if(CallBase* call = dyn_cast<CallBase>(inst_info->InstPtr))
            fout<<"  <===== Function Call.\n";
        else
            fout<<"\n";

        // write source location to file
        for(unsigned i=0; i<level; i++)
            fout<<"\t";
        fout<<inst_info->InstLoc.toString()<< " [" << getOriginalName(inst_info->InstPtr->getFunction()->getName().str()) <<"]\n";
        fout.close();

        // write other instructions
        for(auto i=inst_info->Successors.begin(), e=inst_info->Successors.end(); i!=e; i++)
        {
            struct InstInfo* next = *i;
            if(next->add_tab){
                this->dumpInstInfo( output_file, next, level+1);
            } else { 
                this->dumpInstInfo( output_file, next, level);
            }
        }
    }


    void dumpFuncInfo(string output_file, struct FuncInfo* func_info, unsigned level)
    {
        ofstream fout(output_file.c_str(), ios::app);
        if(! fout)
        {
            llvm::outs()<<"Open output file "<<output_file<<" FAILED!\n";
            return;
        }

        for(unsigned i=0; i<level; i++)
            fout<<"\t";
        fout<<func_info->FuncName<<" "<<func_info->ArgIndex<<"\n";
        for(unsigned j=0; j<func_info->CallLocList.size(); j++)
        {
            for(unsigned i=0; i<level+1; i++)
                fout<<"\t";
            fout<<func_info->CallLocList[j]<<"\n";
        }
        fout.close();
        
        for(auto i=func_info->ArgInstInfoList.begin(); i!=func_info->ArgInstInfoList.end(); i++)
        {
            struct InstInfo* inst_info = *i;
            this->dumpInstInfo( output_file, inst_info, level+1);
        }
        
        for(auto i=func_info->InsideFuncInfoList.begin(); i!=func_info->InsideFuncInfoList.end(); i++)
        {
            struct FuncInfo* inner_func_info = *i;
            this->dumpFuncInfo( output_file, inner_func_info, level+1);
        }
    }


    bool writeToFile(string output_file)
    {
        ofstream fout(output_file.c_str(), ios::app);
        if(! fout)
        {
            llvm::outs()<<"Open output file "<<output_file<<" FAILED!\n";
            return false;
        }

        fout<<"\n\n\n";
        fout<<"GlobalVariable Name: "<<this->NameInfo->getNameAsString();

        for(unsigned i=0; i<this->Offsets.size(); i++)
        {
            fout<<"\tOffset: ";
            if( this->Offsets[i] != NO_OFFSET)
                fout<<this->Offsets[i]<<" ";
        }
        fout<<"\n";

        fout<<"\n\tCaller Functions: \n";
        if( this->CallerFunctionList.empty() )
            this->collectCallerFunction();
        for(auto it=this->CallerFunctionList.begin(); it!=this->CallerFunctionList.end(); it++)
        {
            Function* func = *it;
            fout<<"\t\t"<<getOriginalName(func->getName())<<"\n";
        }

        fout<<"\n\tTainted Functions (group by Caller-Functions): \n\n";
        for (auto const& x : this->InfluencedFuncList) {
            fout<<"\t\t"<< x.first <<"\n";
            set<string> s;
            for (auto const& y : x.second) {
                s.insert(getOriginalName(y->getName()));
            }    
            for (auto const& z : s) {
                fout<<"\t\t\t\t"<<z<<"\n";
            }
            fout<<"\n";
        }

        fout<<"\n\tCalled Functions: \n";
        fout.close();
        for(auto i=this->FuncInfoList.begin(), e=this->FuncInfoList.end(); i!=e; i++)
        {
            struct FuncInfo* func_info = *i;
            this->dumpFuncInfo( output_file, func_info, 2);
        }

        fout.open(output_file.c_str(), ios::app);
        fout<<"\n\tCalled Chain:"<<"\n";
        fout.close();

        for(auto i=this->InstInfoList.begin(), e=this->InstInfoList.end(); i!=e; i++)
        {
            struct InstInfo* inst_info = *i;
            this->dumpInstInfo( output_file, inst_info, 2);
        }

        fout.open(output_file.c_str(), ios::app);
        fout<<"\n\tRelated GlobalVariables: \n";
        for(auto i=this->InfluencedGVList.begin(); i!=this->InfluencedGVList.end(); i++)
        {
            GlobalVariable* gv = *i;
            fout<<"\t\t"<<getAsString(gv)<<"\n";
        }
        fout.close();

        return true;
    }

	bool isInList(vector<struct InstInfo *> &InstList, InstInfo* inst_info)
	{
		bool res = false;
		for(auto i=InstList.begin(), e=InstList.end(); i!=e; i++)
        {
            struct InstInfo* tmp_info = *i;
			if(tmp_info->InstPtr == inst_info->InstPtr) return res = true;
        }
		return res;
	}
	vector<struct InstInfo *> getDataFlow()
    {
		vector<struct InstInfo *> InstList;
		for(auto i=this->InstInfoList.begin(), e=this->InstInfoList.end(); i!=e; i++)
        {
            struct InstInfo* inst_info = *i;
            dfsAllInstInfo(InstList, inst_info);
        }
		return InstList;
    }
    void dfsAllInstInfo(vector<struct InstInfo *> &InstList, InstInfo* inst_info)
    {
		if(!isInList(InstList, inst_info))
			InstList.push_back(inst_info);
		for(auto i=inst_info->Successors.begin(), e=inst_info->Successors.end(); i!=e; i++)
        {
            struct InstInfo* next = *i;
	    	dfsAllInstInfo(InstList, next);
        }
		return ;
    }

    vector<struct InstInfo *> getExplicitDataFlow()
    {
		vector<struct InstInfo *> InstList;
		for(auto i=this->InstInfoList.begin(), e=this->InstInfoList.end(); i!=e; i++)
        {
            struct InstInfo* inst_info = *i;
            dfsInstInfo(InstList, inst_info);
        }
		return InstList;
    }
    void dfsInstInfo(vector<struct InstInfo *> &InstList, InstInfo* inst_info)
    {
		if(inst_info->isDirect == false)
	    	return ;
		if(!isInList(InstList, inst_info))
			InstList.push_back(inst_info);

		for(auto i=inst_info->Successors.begin(), e=inst_info->Successors.end(); i!=e; i++)
        {
            struct InstInfo* next = *i;
	    	dfsInstInfo(InstList, next);
        }
		return ;
    }

    vector<struct InstInfo *> getImplicitDataFlow()
    {
		vector<struct InstInfo *> InstList;
		for(auto i=this->InstInfoList.begin(), e=this->InstInfoList.end(); i!=e; i++)
        {
            struct InstInfo* inst_info = *i;
            dfsImplicitInstInfo(InstList, inst_info);
        }
		return InstList;
    }
    void dfsImplicitInstInfo(vector<struct InstInfo *> &InstList, InstInfo* inst_info)
    {
		if(inst_info->isDirect == false)
		if(!isInList(InstList, inst_info))
	    	InstList.push_back(inst_info);

		for(auto i=inst_info->Successors.begin(), e=inst_info->Successors.end(); i!=e; i++)
        {
            struct InstInfo* next = *i;
	    	dfsImplicitInstInfo(InstList, next);
        }
		return ;
    }

    bool writeStatistics(string stat_file, bool initialize)
    {
        ofstream stat_f(stat_file.c_str(), ios::app);
        if(! stat_f)
        {
            llvm::outs()<<"Open statistics file "<<stat_file<<" FAILED!\n";
            return false;
        }

        if( initialize )
        {
            stat_f<<"Config Name, ";
            stat_f<<"Config Variable Name, ";
            stat_f<<"Is Reconfigurable, ";
            stat_f<<"Caller Function Number, ";
            stat_f<<"Strong Called Function Number, ";
            stat_f<<"Weak Called Function Number, ";
            stat_f<<"Inside Called Function Dataflow Instruction Number, ";
            stat_f<<"Inside Called Function Dataflow Line Number, ";
            stat_f<<"Strong Related Dataflow Instruction Number, ";
            stat_f<<"Strong Related Dataflow Line Number, ";
            stat_f<<"Weak Related Dataflow Instruction Number, ";
            stat_f<<"Weak Related Dataflow Line Number, ";
            stat_f<<"Control Condition Number, ";
            stat_f<<"Related Control BasicBlock Number, ";
            stat_f<<"Related Control Instruction Number, ";
            stat_f<<"Related Control Line Number\n";
            
            stat_f.close();
            return true;
        }

        /// TAG: Config Name
        stat_f<<this->NameInfo->OriginalConfigName<<", ";

        /// TAG: Config Variable Name
        stat_f<<this->NameInfo->getNameAsString()<<", ";
        
        /// TAG: Is Reconfigurable
        stat_f<<this->NameInfo->isDynamicConfigurable<<" "<<this->NameInfo->DynamicConfigurableStatus<<", ";


        /// TAG: Caller Function Number
        stat_f<<this->CallerFunctionList.size()<<", ";


        /// TAG: Strong Called Function Number
        vector<string> strong_related_func_list;
        for(auto it=this->FuncInfoList.begin(); it!=this->FuncInfoList.end(); it++)
        {
            struct FuncInfo* func_info = *it;
            if( std::find(strong_related_func_list.begin(),strong_related_func_list.end(), func_info->FuncName) == strong_related_func_list.end())
                strong_related_func_list.push_back(func_info->FuncName);
        }
        stat_f<<strong_related_func_list.size()<<", ";
        

        /// TAG: Weak Called Function Number
        vector<string> weak_related_func_list;
        queue<struct FuncInfo*> Q;
        for(auto it=this->FuncInfoList.begin(); it!=this->FuncInfoList.end(); it++)
        {
            struct FuncInfo* func_info = *it;
            for(unsigned i=0; i<func_info->InsideFuncInfoList.size(); i++)
            {
                Q.push(func_info->InsideFuncInfoList[0]);
            }
            while( ! Q.empty() )
            {   
                struct FuncInfo* func_info = Q.front();
                weak_related_func_list.push_back(func_info->FuncName);
        
                for(unsigned i=0; i<func_info->InsideFuncInfoList.size(); i++)
                {
                    struct FuncInfo* inside_func_info = func_info->InsideFuncInfoList[i];

                    bool searchFlag = false;
                    for(unsigned i=0; i<Q.size(); ++i) {
                        if(Q.front() == inside_func_info)
                            searchFlag = true;
                        Q.push(Q.front());
                        Q.pop();
                    }
                    if(std::find(weak_related_func_list.begin(), weak_related_func_list.end(), inside_func_info->FuncName) != weak_related_func_list.end())
                        searchFlag = true;
                    if(searchFlag == false)
                        Q.push(inside_func_info);
                }
                Q.pop();
            }
        }
        stat_f<<weak_related_func_list.size()<<", ";


        /// TAG: Inside Called Function Dataflow Instruction Number
        vector<struct InstInfo*> inside_inst_info_list;
        queue<struct FuncInfo*> Q1;
        for(auto it=this->FuncInfoList.begin(); it!=this->FuncInfoList.end(); it++)
        {
            struct FuncInfo* func_info = *it;
            Q1.push(func_info);
            while( ! Q1.empty() )
            {   
                struct FuncInfo* func_info = Q1.front();
                
                for(auto ite=func_info->ArgInstInfoList.begin(); ite!=func_info->ArgInstInfoList.end(); ite++)
                {
                    inside_inst_info_list.push_back(*ite);
                }

                for(unsigned i=0; i<func_info->InsideFuncInfoList.size(); i++)
                {
                    struct FuncInfo* inside_func_info = func_info->InsideFuncInfoList[i];
                    Q1.push(inside_func_info);
                }
                Q1.pop();
            }
        }
        stat_f<<inside_inst_info_list.size()<<", ";
        
        /// TAG: Inside Called Function Dataflow Line Number
        vector<struct SrcLoc> inside_related_lines;
        inside_related_lines.clear();
        for(auto ii=inside_inst_info_list.begin(); ii!=inside_inst_info_list.end(); ii++)
        {
            struct InstInfo* inst_info = *ii;
            bool flag_matched = false;
            for(unsigned c=0; c<inside_related_lines.size(); c++)
            {
                if(inside_related_lines[c].Filename == inst_info->InstLoc.Filename &&
                   inside_related_lines[c].Dirname == inst_info->InstLoc.Dirname &&
                   inside_related_lines[c].Line == inst_info->InstLoc.Line)
                {
                    flag_matched = true;
                    break;
                }
            }
            if(flag_matched == false)
            {
                inside_related_lines.push_back(inst_info->InstLoc);
            }
        }
        stat_f<<inside_related_lines.size()<<", ";
        

        /// TAG: Strong Related Dataflow Instruction Number
        vector<struct InstInfo*> strong_related_inst_list;
        for(auto it=this->InstInfoList.begin(); it!=this->InstInfoList.end(); it++)
        {
            struct InstInfo* inst_info = *it;
            if(inst_info->InfluenceLevel ==  STRONG)
                strong_related_inst_list.push_back(inst_info);
        }
        stat_f<<strong_related_inst_list.size()<<", ";
        
        /// TAG: Strong Related Dataflow Line Number
        vector<struct SrcLoc> strong_related_lines;
        strong_related_lines.clear();
        for(auto ii=strong_related_inst_list.begin(); ii!=strong_related_inst_list.end(); ii++)
        {
            struct InstInfo* inst_info = *ii;
            bool flag_matched = false;
            for(unsigned c=0; c<strong_related_lines.size(); c++)
            {
                if(strong_related_lines[c].Filename == inst_info->InstLoc.Filename &&
                   strong_related_lines[c].Dirname == inst_info->InstLoc.Dirname &&
                   strong_related_lines[c].Line == inst_info->InstLoc.Line)
                {
                    flag_matched = true;
                    break;
                }
            }
            if(flag_matched == false)
            {
                strong_related_lines.push_back(inst_info->InstLoc);
            }
        }
        stat_f<<strong_related_lines.size()<<", ";


        /// TAG: Weak Related Dataflow Instruction Number
        vector<struct InstInfo*> weak_related_inst_list;
        for(auto it=this->InstInfoList.begin(); it!=this->InstInfoList.end(); it++)
        {
            struct InstInfo* inst_info = *it;
            if(inst_info->InfluenceLevel ==  WEAK)
                weak_related_inst_list.push_back(inst_info);
        }
        stat_f<<weak_related_inst_list.size()<<", ";

        /// TAG: Weak Related Dataflow Line Number
        vector<struct SrcLoc> weak_related_lines;
        weak_related_lines.clear();
        for(auto ii=weak_related_inst_list.begin(); ii!=weak_related_inst_list.end(); ii++)
        {
            struct InstInfo* inst_info = *ii;
            bool flag_matched = false;
            for(unsigned c=0; c<weak_related_lines.size(); c++)
            {
                if(weak_related_lines[c].Filename == inst_info->InstLoc.Filename &&
                   weak_related_lines[c].Dirname == inst_info->InstLoc.Dirname &&
                   weak_related_lines[c].Line == inst_info->InstLoc.Line)
                {
                    flag_matched = true;
                    break;
                }
            }
            if(flag_matched == false)
            {
                weak_related_lines.push_back(inst_info->InstLoc);
            }
        }
        stat_f<<weak_related_lines.size()<<", ";


        /// TAG: Control Condition Number
        vector<struct InstInfo*> branch_inst_list;
        queue<struct InstInfo*> QQ;
        for(auto it=this->InstInfoList.begin(); it!=this->InstInfoList.end(); it++)
        {
            struct InstInfo* inst_info = *it;
            QQ.push(inst_info);
            while( ! QQ.empty() )
            {   
                struct InstInfo* inst_info = QQ.front();
                if( inst_info->isControllingInst && std::find(branch_inst_list.begin(), branch_inst_list.end(), inst_info) == branch_inst_list.end())
                    branch_inst_list.push_back(inst_info);

                for(unsigned i=0; i<inst_info->Successors.size(); i++)
                {
                    struct InstInfo* inside_inst_info = inst_info->Successors[i];
                    QQ.push(inside_inst_info);
                }
                QQ.pop();
            }
        }
        stat_f<<branch_inst_list.size()<<", ";


        /// TAG: Related Control BasicBlock Number
        unsigned bb_cnt = 0;
        for(unsigned i=0; i<branch_inst_list.size(); i++)
        {
            bb_cnt += branch_inst_list[i]->ControllingBBs.size();
        }
        stat_f<<bb_cnt<<", ";

        /// TAG: Related Control Instruction Number
        vector<Instruction*> control_inst_list;
        for(unsigned i=0; i<branch_inst_list.size(); i++)
        {
            for(auto it=branch_inst_list[i]->ControllingBBs.begin(); it!=branch_inst_list[i]->ControllingBBs.end(); it++)
            {
                BasicBlock* bb = *it;
                for (BasicBlock::iterator ite = bb->begin(); ite != bb->end(); ite++)
                {
                    Instruction* inst = &*ite;
                    bool flag_recorded_inst = false;
                    for(auto j=control_inst_list.begin(); j!=control_inst_list.end(); j++)
                    {
                        Instruction* cur_inst = *j;
                        if(cur_inst == inst)
                        {
                            flag_recorded_inst = true;
                            break;
                        }
                    }
                    if(flag_recorded_inst == false)
                        control_inst_list.push_back(inst);
                }
            }
        }
        stat_f<<control_inst_list.size()<<", ";
        
        /// TAG: Related Control Line Number
        vector<struct SrcLoc> control_inst_related_lines;
        control_inst_related_lines.clear();
        for(auto it=control_inst_list.begin(); it!=control_inst_list.end(); it++)
        {
            Instruction* inst = *it;
            struct SrcLoc srcloc = getSrcLoc(inst);
            if( ! srcloc.isValid())
                continue;
            
            bool flag_recorded = false;
            for(auto ii=control_inst_related_lines.begin(); ii!=control_inst_related_lines.end(); ii++)
            {
                struct SrcLoc _srcloc = *ii;
                if(srcloc.Dirname == _srcloc.Dirname &&
                   srcloc.Filename == _srcloc.Filename &&
                   srcloc.Line == _srcloc.Line)
                {
                    flag_recorded = true;
                    break;
                }
            }
            if( flag_recorded == false)
                control_inst_related_lines.push_back(srcloc);
        }
        stat_f<<control_inst_related_lines.size()<<"\n";

        return true;
    }

    bool writeKeyInfo(string output_file)
    {
        ofstream fout(output_file.c_str(), ios::app);
        if(! fout)
        {
            llvm::outs()<<"Open output file "<<output_file<<" FAILED!\n";
            return false;
        }
        fout<<"\n\n\n";
        fout<<"GlobalVariable Name: "<<this->NameInfo->getNameAsString();
        for(unsigned i=0; i<this->Offsets.size(); i++)
        {
            fout<<"\tOffset: ";
            if( this->Offsets[i] != NO_OFFSET)
                fout<<this->Offsets[i]<<" ";
        }
        fout<<"\t";
        
        if(this->NameInfo->isDynamicConfigurable)
            fout<<"Yes "<<this->NameInfo->DynamicConfigurableStatus<<"\n";
        else
            fout<<"No "<<this->NameInfo->DynamicConfigurableStatus<<"\n";

        fout<<"\n\tCaller Functions: \n";
        if( this->CallerFunctionList.empty() )
            this->collectCallerFunction();
        for(auto it=this->CallerFunctionList.begin(); it!=this->CallerFunctionList.end(); it++)
        {
            fout<<"\t\t"<<getOriginalName((*it)->getName().str())<<"\t"<<(*it)->getName().str()<<"\n";
        }

        
        fout<<"\n\tCalled Functions: \n";
        for(auto i=this->FuncInfoList.begin(), e=this->FuncInfoList.end(); i!=e; i++)
        {
            struct FuncInfo* func_info = *i;
            fout<<"\t\t"<<func_info->FuncName<<" "<<func_info->ArgIndex<<"\n";
        }

        fout<<"\n\tInfluenced GlobalVariables: \n";
        for(auto it=this->InfluencedGVList.begin(); it!=this->InfluencedGVList.end(); it++)
        {
            fout<<"\t\t"<<getAsString(*it)<<"\n";
        }


        fout<<"\n\tRelated Branches: \n";
        vector<pair<struct InstInfo*, unsigned>> branch_inst_list;
        queue<struct InstInfo*> QQ;
        for(auto it=this->InstInfoList.begin(); it!=this->InstInfoList.end(); it++)
        {
            struct InstInfo* inst_info = *it;
            QQ.push(inst_info);
            while( ! QQ.empty() )
            {   
                struct InstInfo* inst_info = QQ.front();
                if( inst_info->isControllingInst)
                {
                    bool flag_matched = false;
                    for(unsigned c=0; c<branch_inst_list.size(); c++)
                    {
                        if( branch_inst_list[c].first == inst_info)
                        {
                            flag_matched = true;
                            break;
                        }
                    }
                    if( flag_matched == false)
                    {
                        branch_inst_list.push_back(pair<struct InstInfo*, unsigned>(inst_info, inst_info->ControllingBBs.size()));
                    }
                }

                for(unsigned i=0; i<inst_info->Successors.size(); i++)
                {
                    struct InstInfo* inside_inst_info = inst_info->Successors[i];
                    QQ.push(inside_inst_info);
                }
                QQ.pop();
            }
        }
        vector<pair<struct SrcLoc, unsigned>> branch_related_lines;
        for(auto ii=branch_inst_list.begin(); ii!=branch_inst_list.end(); ii++)
        {
            unsigned flag_matched_idx = ERR_OORANGE;
            unsigned adding_cnt = 0;
            for(unsigned c=0; c<branch_related_lines.size(); c++)
            {
                if(branch_related_lines[c].first.Filename == (*ii).first->InstLoc.Filename &&
                   branch_related_lines[c].first.Dirname == (*ii).first->InstLoc.Dirname &&
                   branch_related_lines[c].first.Line == (*ii).first->InstLoc.Line)
                {
                    flag_matched_idx = c;
                    adding_cnt = (*ii).second;
                    break;
                }
            }
            if(flag_matched_idx == ERR_OORANGE)
            {
                branch_related_lines.push_back(pair<struct SrcLoc, unsigned>((*ii).first->InstLoc,(*ii).second));
            } else {
                branch_related_lines[flag_matched_idx].second += adding_cnt;
            }
        }
        for(unsigned i=0; i<branch_related_lines.size(); i++)
            fout<<"\t\t"<<branch_related_lines[i].first.toString()<<"\n\t\t\t"<<branch_related_lines[i].second<<"\n";

        return true;
    }
};




#endif
