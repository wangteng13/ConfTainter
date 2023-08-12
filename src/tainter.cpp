//#include "DLCallGraph.h"
#include "tainter.h"

/*
    Function:   switch to control output information;
    Value:      0   :   error   :   _ERROR_LEVEL
                1   :   warning :   _WARNING_LEVEL
                2   :   debug   :   _DEBUG_LEVEL
                3   :   redundency: _REDUNDENCY_LEVEL
*/
unsigned debug_level = _ERROR_LEVEL;

Module *M = nullptr;
struct DLCallGraph *DLCG;

vector<BitCastOperator *> verifiedStructTypeList; // vec to record the cast the turn Type %struct.redisServer into Type %struct.redisServer.xxx

vector<Instruction *> visitedLoadStoreInstList; // record visited Load/Store Inst.
vector<User *> visitedStructGVCases;            // if a User is matched with a structual GV, we don't need to revisit it for other GVs.

vector<string> CommonLibFunctions;

vector<
    pair<pair<Type *,
              vector<int>>,
         Value *>>
    GEPTypeOffsetInstList; // record ALL GEPinstruction to make the anaylze field-sensitive. A little stupid but work.

bool isSubStr(string longstr, string str)
{
    int index = longstr.find(str);
    if (index != string::npos)
    {
        return true;
    }
    else
    {
        return false;
    }
}


/// Fetch the instrcution who use the result a GEP inst. who fetches the type "Type*" with offsets "vector<int>"
Value *FetchValue4FurtherFollow(Type *type, vector<int> *indice, Value *get_inst)
{
    // pair<pair<Type *, vector<int>>, Value*> * GEPInfo){
    Value *match_value = nullptr;
    for (vector<pair<pair<Type *, vector<int>>, Value *>>::iterator i = GEPTypeOffsetInstList.begin(); i != GEPTypeOffsetInstList.end(); i++)
    {
        if (i->first.first != type) // Type* not match
            continue;
        if (i->first.second != *indice) // offset not match
            continue;
        if (i->second == get_inst) // this gep_ins has been recorded before, by using the "addr" of store_ins
            continue;
        match_value = i->second;
    }
    return match_value;
};


/// Fetch the instrcution who use the result a GEP inst. who fetches the type "Type*" with offsets "vector<int>"
vector<Value *> FetchValue4FurtherFollow2(string type, vector<int> *indice)
{
    vector<Value *> res;
    // pair<pair<Type *, vector<int>>, Value*> * GEPInfo){
    Value *match_value = nullptr;
    for (vector<pair<pair<Type *, vector<int>>, Value *>>::iterator i = GEPTypeOffsetInstList.begin(); i != GEPTypeOffsetInstList.end(); i++)
    {
        //string fulltype = "%struct." + type;
        string fulltype = '.' + type;
        std::string type_str;
        llvm::raw_string_ostream rso(type_str);
        i->first.first->print(rso);
        // **** DEBUG ****
        //llvm::outs() << rso.str() << "  " << fulltype << "\n";
        if (!isSubStr(rso.str(), fulltype)) // Type* not match 
        //rso.str() != fulltype.isSubStr())
            continue;

        llvm::outs() << "  Type matched, with offsets: [ ";
        for (auto each:i->first.second)
            llvm::outs() << each << " ";
        llvm::outs() << "]   Target offsets: [ ";
        for (auto each2:*indice)
            llvm::outs() << each2 << " ";
        llvm::outs() << "], if equal to target, found.\n";
        
        if (i->first.second != *indice) // offset not match
            continue;
        match_value = i->second;
        res.push_back(match_value);
    }
    return res;
};

void initCommonLibFunctions()
{
    /// PostgreSQL's log functions.
    CommonLibFunctions.push_back("errmsg");
    CommonLibFunctions.push_back("elog_finish");
    CommonLibFunctions.push_back("errdetail");
    CommonLibFunctions.push_back("errcontext_msg");
    CommonLibFunctions.push_back("write_stderr");
    CommonLibFunctions.push_back("errdetail_plural");
    CommonLibFunctions.push_back("errfinish");
    CommonLibFunctions.push_back("errhint");
    CommonLibFunctions.push_back("errmsg_internal");
    /// glibc string functions.
    CommonLibFunctions.push_back("snprintf");
    CommonLibFunctions.push_back("psprintf");
    /// Other functions?
    CommonLibFunctions.push_back("appendStringInfo");
	CommonLibFunctions.push_back("xstrdup");
}

vector<pair<CallBase *, Function *> *> getCallerAndCallInst(Function *f)
{

    if (!f)
        llvm::outs() << "ERROR! Argument of `getCallerAndCallInst` should not be nullptr.\n";

    vector<pair<CallBase *, Function *> *> res;
    CallGraph *CG = new CallGraph(*M);

    for (auto IT = bf_begin(CG), EI = bf_end(CG); IT != EI; IT++)
    {

        if (!IT->getFunction())
            continue;

        for (CallGraphNode::iterator calledNode_IT = IT->begin(); calledNode_IT != IT->end(); calledNode_IT++)
        {

            if (!calledNode_IT->first)
                continue;

            if (CallBase *cb = llvm::dyn_cast<CallBase>(calledNode_IT->first))
            {

                if (calledNode_IT->second->getFunction() && calledNode_IT->second->getFunction() == f)

                    res.push_back(new pair<CallBase *, Function *>(cb, IT->getFunction()));
            }
            else
            {

                llvm::outs() << "STRAGE. NOT a CallBase for the calledNode_IT->first" << getClassType(calledNode_IT->first) << "\n";
            }
        }
    }
    return res;
}

void printCallers(vector<pair<CallBase *, Function *> *> a, uint level)
{

    for (vector<pair<CallBase *, Function *> *>::iterator i = a.begin(); i != a.end(); i++)
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[" << getOriginalName((*i)->second->getName().str()) << "] ");
        MY_DEBUG(_DEBUG_LEVEL, (*i)->first->print(llvm::outs()));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n");
    }
}

vector<string> splitWithTag(string str, string tag)
{
    vector<string> names;
    while (str.find(tag) != string::npos)
    {
        names.push_back(str.substr(0, str.find(tag)));
        str = str.substr(str.find(tag) + tag.length(), str.length() - str.find(tag) - tag.length());
    }
    names.push_back(str);
    return names;
}

bool readConfigVariableNames(std::string var_file, std::vector<struct ConfigVariableNameInfo *> &config_names)
{
    config_names.clear();
    fstream fin(var_file.c_str(), ios::in);
    if (!fin)
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "open var file failed.\n");
        return false;
    }
    std::string buf_str;
    while (getline(fin, buf_str))
    {
        if (buf_str == "")
            continue;
        std::istringstream iss(buf_str);
        std::string type, var_name, origin_config_name, isDynConfig;
        iss >> type >> var_name >> origin_config_name >> isDynConfig;
        struct ConfigVariableNameInfo *config_name = new ConfigVariableNameInfo();
        config_name->OriginalConfigName = origin_config_name;
        if (type == "SINGLE")
        {
            config_name->VarType = SINGLE;
            config_name->SingleName = var_name;
        }
        else if (type == "STRUCT")
        {
            config_name->VarType = STRUCT;
            config_name->StructName = splitWithTag(var_name, ".");
        }
        else if (type == "CLASS")
        {
            config_name->VarType = CLASS;
            config_name->ClassName = splitWithTag(var_name, "::");
        }
        else if (type == "FIELD")
        {
            config_name->VarType = FIELD;
            config_name->FieldName = splitWithTag(var_name, ".");
        }

        config_name->DynamicConfigurableStatus = isDynConfig;
        if (isDynConfig == "Yes" ||
            isDynConfig == "user" ||
            isDynConfig == "superuser" ||
            isDynConfig == "backend" ||
            isDynConfig == "superuser-backend" ||
            isDynConfig == "sighup")
        {
            config_name->isDynamicConfigurable = true;
        }
        else if (isDynConfig == "No" ||
                 isDynConfig == "internal" ||
                 isDynConfig == "postmaster")
        {
            config_name->isDynamicConfigurable = false;
        }

        config_names.push_back(config_name);
    }
    fin.close();

    MY_DEBUG(
        _DEBUG_LEVEL,
        llvm::outs() << "\n\n\n******************Output read in config name info:******************\n\n\n\n";
        for (auto i = config_names.begin(), e = config_names.end(); i != e; i++) {
            llvm::outs() << (*i)->getNameAsString() << "\n";
        } llvm::outs()
        << "\n\n\n******************Output end******************\n\n\n\n";)

    return true;
}

struct SrcLoc getSrcLoc(Instruction *inst)
{
    string filename = "";
    string dirname = "";
    unsigned line = 0;
    unsigned col = 0;

    if (DILocation *Loc = inst->getDebugLoc())
    {
        line = Loc->getLine();
        col = Loc->getColumn();
        filename = Loc->getFilename();
        dirname = Loc->getDirectory();
    }

    struct SrcLoc srcloc(filename, dirname, line, col);
    return srcloc;
}


void remove_char(string &str, char ch)
{
    for (auto it = str.begin(); it < str.end(); it++)
    {
        if (*it == ch)
        {
            str.erase(it);
            it--;
        }
    }
}

string getStructTypeStrFromPrintAPI(Type *cur_type)
{
    std::string str_buf;
    llvm::raw_string_ostream rso(str_buf);
    cur_type->print(rso);
    std::string type_name = rso.str();
    if (type_name.find("struct") == string::npos && type_name.find("class") == string::npos)
        return "";

    while (type_name.find("*") != string::npos)
    {
        remove_char(type_name, '*');
    }

    return type_name;
}

void printTabs(unsigned level)
{
    for (unsigned i = 0; i < level; i++)
        llvm::outs() << "  ";
    // llvm::outs()<<"\t";
}

string getClassType(Value *value)
{
    if (llvm::isa<Argument>(value))
        return "Argument";
    else if (llvm::isa<BasicBlock>(value))
        return "BasicBlock";
    else if (llvm::isa<InlineAsm>(value))
        return "InlineAsm";
    else if (llvm::isa<MetadataAsValue>(value))
        return "MetadataAsValue";
    else if (llvm::isa<User>(value))
    {
        if (llvm::isa<Constant>(value))
        {
            if (llvm::isa<BlockAddress>(value))
                return "User::Constant::BlockAddress";
            else if (llvm::isa<ConstantAggregate>(value))
            {
                if (llvm::isa<ConstantArray>(value))
                    return "User::Constant::ConstantAggregate::ConstantArray";
                else if (llvm::isa<ConstantStruct>(value))
                    return "User::Constant::ConstantAggregate::ConstantStruct";
                else if (llvm::isa<ConstantVector>(value))
                    return "User::Constant::ConstantAggregate::ConstantVector";
                else
                    return "User::Constant::ConstantAggregate";
            }
            else if (llvm::isa<ConstantData>(value))
            {
                if (llvm::isa<ConstantFP>(value))
                    return "User::Constant::ConstantData::ConstantFP";
                else if (llvm::isa<ConstantInt>(value))
                    return "User::Constant::ConstantData::ConstantInt";
                else if (llvm::isa<ConstantPointerNull>(value))
                    return "User::Constant::ConstantData::ConstantPointerNull";
                else if (llvm::isa<ConstantTokenNone>(value))
                    return "User::Constant::ConstantData::ConstantTokenNone";
                else
                    return "User::Constant::ConstantData";
            }
            else if (llvm::isa<ConstantExpr>(value))
            {
                return "User::Constant::ConstantExpr";
            }
            else if (llvm::isa<GlobalValue>(value))
                return "User::Constant::GlobalValue";
            else
                return "User::Constant";
        }
        else if (llvm::isa<DerivedUser>(value))
            return "User::DerivedUser";
        else if (Instruction *inst = llvm::dyn_cast<Instruction>(value))
            return "User::Instruction::" + string(inst->getOpcodeName());
        else if (llvm::isa<Operator>(value))
        {
            if (llvm::isa<BitCastOperator>(value))
                return "User::Operator::BitCastOperator";
            // else if(llvm::isa<AddrSpaceCastOperator>(value))
            //     return "User::Operator::AddrSpaceCastOperator";
            else if (llvm::isa<GEPOperator>(value))
                return "User::Operator::GEPOperator";
            else if (llvm::isa<PtrToIntOperator>(value))
                return "User::Operator::PtrToIntOperator";
            else if (llvm::isa<ZExtOperator>(value))
                return "User::Operator::ZExtOperator";
            else if (llvm::isa<FPMathOperator>(value))
                return "User::Operator::FPMathOperator";
            else if (llvm::isa<OverflowingBinaryOperator>(value))
                return "User::Operator::OverflowingBinaryOperator";
            else if (llvm::isa<PossiblyExactOperator>(value))
                return "User::Operator::PossiblyExactOperator";
            else
                return "User::Operator";
        }
        else
        {
            return "User";
        }
    }
    else
    {
        return "Unknown";
    }
}

string getAsString(Value *value)
{
    std::string str_buf;
    llvm::raw_string_ostream rso(str_buf);
    value->print(rso);
    std::string ret_str = rso.str();
    while (ret_str[0] == ' ')
        ret_str = ret_str.substr(1);
    return ret_str;
}

vector<unsigned> getIndexFromGEPO(GEPOperator *gepo)
{
    /*
        This situation happends when handle on a array with index, like "tour[i] = tour[j];" in geqo_recombination.c:56;
            %42 = getelementptr i32, i32* %39, i64 %41
        or pointer offset, like, "OldestVisibleMXactId = OldestMemberMXactId + MaxOldestSlot;" in multixact.c:1857 in postgresql, where "OldestMemberMXactId" and "OldestVisibleMXactId" both are pointer of MultiXactId, a typedef of uint32 finally.
            %80 = getelementptr i32, i32* %75, i64 %79
        Since handling of config variables in structures seems not like this, so we don't consider it.
    */
    vector<unsigned> vec;
    for (unsigned i = 1; i < gepo->getNumOperands(); i++)
    {
        Value *op = gepo->getOperand(i);
        if (ConstantInt *CI = dyn_cast<ConstantInt>(op))
        {
            uint64_t idx = CI->getZExtValue();
            vec.push_back(idx);
        }
        else
        {
            vec.push_back(ERR_OORANGE);
        }
    }
    return vec;
}

unsigned getFuncArgIndex(CallBase *call, Value *cur_value)
{
    for (unsigned i = 0; i < call->getNumArgOperands(); i++)
    {
        Value *arg = call->getArgOperand(i);
        if (arg == cur_value) // how to judge they are equal?
        {
            return i;
        }
    }
    return ERR_OORANGE;
}

/// Demangle the function name
/// Input: mangling name
/// output: demangling name
string getOriginalName(string manglingName)
{
    if (manglingName.empty())
        return manglingName;

    if (manglingName.find("\01_") != string::npos)
    {
        manglingName = manglingName.substr(manglingName.find('_') + 1);
    }

    string name;
    int status;
    char *realname;
    realname = abi::__cxa_demangle(const_cast<char *>(manglingName.data()), 0, 0, &status);
    if (!realname)
        return manglingName;
    name = realname;
    name = name.substr(0, name.find_first_of('<'));
    name = name.substr(0, name.find_first_of('('));

    return name;
}

bool handleDIType(DIType *di_type, 
                  std::vector<struct ConfigVariableNameInfo *> config_names, 
                  GlobalVariable *gv, 
                  std::vector<struct GlobalVariableInfo *> &gv_info_list, 
                  unsigned config_names_idx, // max value of what?
                  vector<unsigned> offsets, 
                  unsigned level)
{

    if (!di_type)
        return false;

    bool ret_flag = false;

    if (config_names_idx != ERR_OORANGE && 
                   level != ERR_OORANGE)
    {
        MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "FullName: " << config_names[config_names_idx]->getNameAsString() << "\n");
        if (level < config_names[config_names_idx]->StructName.size())
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Current_name " << config_names[config_names_idx]->StructName[level] << "\n");
        MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Offsets: ");
        for (unsigned i = 0; i < offsets.size(); i++)
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << offsets[i] << " ");
        }
        MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "\n");
    }

    if (DIBasicType *basic_type = dyn_cast<DIBasicType>(di_type))
    {
        if (config_names_idx != ERR_OORANGE && config_names[config_names_idx]->VarType != SINGLE)
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Comes here 3\n");
            ret_flag = true;
            struct GlobalVariableInfo *gv_info = new GlobalVariableInfo(config_names[config_names_idx], gv, offsets);
            gv_info_list.push_back(gv_info);
        }
        else 
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "basic type gv name: " << gv->getName() << " -> " << getOriginalName(gv->getName()) << "\n");
            for (unsigned i = 0; i < config_names.size(); i++)
            {
                if (config_names[i]->VarType == STRUCT || config_names[i]->VarType == FIELD)
                    continue;

                if (getOriginalName(gv->getName().str()) == config_names[i]->SingleName)
                {
                    ret_flag = true;
                    struct GlobalVariableInfo *gv_info = new GlobalVariableInfo(config_names[i], gv, offsets);
                    gv_info_list.push_back(gv_info);
                }
            }
        }
    }

    else if (DIDerivedType *derived_type = dyn_cast<DIDerivedType>(di_type))
    {
        if (derived_type->getTag() == llvm::dwarf::DW_TAG_member)
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "In DIDerivedType - member tag\n");

            if (config_names_idx == ERR_OORANGE || level == ERR_OORANGE)
                return false;

            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Name : " << derived_type->getName() << "\n");
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "level : " << level << " size : " << config_names[config_names_idx]->StructName.size() << "\n");

            if ((config_names[config_names_idx]->VarType == STRUCT &&
                 derived_type->getName().str() == config_names[config_names_idx]->StructName[level]) ||
                (config_names[config_names_idx]->VarType == CLASS &&
                 derived_type->getName().str() == config_names[config_names_idx]->ClassName[level]))
            {
                MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Comes here 1\n");
                if ((config_names[config_names_idx]->VarType == STRUCT &&
                     config_names[config_names_idx]->StructName.size() > level + 1) ||
                    (config_names[config_names_idx]->VarType == CLASS &&
                     config_names[config_names_idx]->ClassName.size() > level + 1))
                {
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Comes here 2\n");
                    DIType *di_type = derived_type->getBaseType();
                    return handleDIType(di_type, config_names, gv, gv_info_list, config_names_idx, offsets, level);
                }
                else
                {
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Comes here 3\n");
                    ret_flag = true;
                    struct GlobalVariableInfo *gv_info = new GlobalVariableInfo(config_names[config_names_idx], gv, offsets);
                    gv_info_list.push_back(gv_info);
                }
            }
        }

        else if (derived_type->getTag() == llvm::dwarf::DW_TAG_typedef ||
                 derived_type->getTag() == llvm::dwarf::DW_TAG_pointer_type ||
                 derived_type->getTag() == llvm::dwarf::DW_TAG_const_type ||
                 derived_type->getTag() == llvm::dwarf::DW_TAG_restrict_type || // seems not useful, but we still consider it.
                 derived_type->getTag() == llvm::dwarf::DW_TAG_atomic_type ||   // seems not useful, but we still consider it.
                 derived_type->getTag() == llvm::dwarf::DW_TAG_volatile_type)
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "In DIDerivedType - other tag\n");
            DIType *type = derived_type->getBaseType();
            return handleDIType(type, config_names, gv, gv_info_list, config_names_idx, offsets, level);
        }
        else
        {
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Unhandled Tag in DIDerivedType: " << derived_type->getTag() << "\n");
        }
    }

    else if (DICompositeType *compose_type = dyn_cast<DICompositeType>(di_type))
    {
        MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "In DICompositeType\n");
        if (compose_type->getTag() == llvm::dwarf::DW_TAG_structure_type)
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "struct type compose_type name: " << compose_type->getName() << "\n");
            if (config_names_idx == ERR_OORANGE)
            {
                for (unsigned i = 0; i < config_names.size(); i++)
                {

                    if (config_names[i]->VarType != STRUCT)
                        continue;

                    if (level == 0 && getOriginalName(compose_type->getName().str()) != config_names[i]->StructName[level])
                        continue;

                    unsigned offset = 0;
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "DW_TAG_structure_type elements size: " << compose_type->getElements().size() << "\n");
                    for (auto ite = compose_type->getElements().begin(); ite != compose_type->getElements().end(); ite++, offset++)
                    {
                        DINode *di_node = *ite;
                        MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Here we iterate the elements\n");
                        MY_DEBUG(_REDUNDENCY_LEVEL, di_node->print(llvm::outs()));
                        MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "\n");
                        MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "current offset = " << offset << "\n");

                        offsets.push_back(offset);
                        ret_flag = true;

                        if (isa<DIType>(di_node))
                        {
                            handleDIType(dyn_cast<DIType>(di_node), config_names, gv, gv_info_list, i, offsets, level + 1);
                        }
                        offsets.pop_back();
                    }
                }
            }
            else
            {
                if (level == 0 && getOriginalName(compose_type->getName().str()) != config_names[config_names_idx]->StructName[level])
                    return ret_flag;

                unsigned offset = 0;
                MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "DW_TAG_structure_type elements size: " << compose_type->getElements().size() << "\n");
                for (auto ite = compose_type->getElements().begin(); ite != compose_type->getElements().end(); ite++, offset++)
                {
                    DINode *di_node = *ite;
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Here we iterate the elements\n");
                    MY_DEBUG(_REDUNDENCY_LEVEL, di_node->print(llvm::outs()));
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "\n");
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "current offset = " << offset << "\n");

                    offsets.push_back(offset);
                    ret_flag = true;

                    if (isa<DIType>(di_node))
                    {
                        handleDIType(dyn_cast<DIType>(di_node), config_names, gv, gv_info_list, config_names_idx, offsets, level + 1);
                    }
                    offsets.pop_back();
                }
            }
        }
        else if (compose_type->getTag() == llvm::dwarf::DW_TAG_class_type)
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "class type compose_type name: " << compose_type->getName() << "\n");
            for (unsigned i = 0; i < config_names.size(); i++)
            {

                if (config_names[i]->VarType != CLASS)
                    continue;

                // MY_DEBUG( _DEBUG_LEVEL, llvm::outs()<<"config_name: "<<config_name.ClassName<<"::"<<config_name.FieldName<<"\n");
                if (getOriginalName(compose_type->getName().str()) != config_names[i]->ClassName[level])
                    continue;

                unsigned offset = 0;
                MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "DW_TAG_class_type elements size: " << compose_type->getElements().size() << "\n");
                for (auto ite = compose_type->getElements().begin(), e = compose_type->getElements().end(); ite != e; ite++, offset++)
                {
                    DINode *di_node = *ite;
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Here we iterate the elements\n");
                    MY_DEBUG(_REDUNDENCY_LEVEL, di_node->print(llvm::outs()));
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "\n");
                    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "current offset = " << offset << "\n");

                    offsets.push_back(offset);
                    ret_flag = true;

                    if (isa<DIType>(di_node))
                    {
                        handleDIType(dyn_cast<DIType>(di_node), config_names, gv, gv_info_list, i, offsets, level + 1);
                    }
                }
            }
        }
        else if (compose_type->getTag() == llvm::dwarf::DW_TAG_enumeration_type)
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "enum type compose_type name: " << compose_type->getName() << "\n");
            DIType *base_type = compose_type->getBaseType();
            handleDIType(base_type, config_names, gv, gv_info_list, config_names_idx, offsets, level);
        }
        else if (compose_type->getTag() == llvm::dwarf::DW_TAG_array_type)
        {
            DIType *base_type = compose_type->getBaseType();
            handleDIType(base_type, config_names, gv, gv_info_list, config_names_idx, offsets, level);
        }
    }
    return ret_flag;
}

/**
 * @brief Find in if `gv` matches any one in the list `config_names`.
 *          If yes, new a `gv` and store it to `gv_info_list`
 *        CORE: struct GlobalVariableInfo* gv_info = new GlobalVariableInfo(config_names[config_names_idx], gv, offsets);
                gv_info_list.push_back(gv_info);
 *
 * @param config_names
 * @param gv
 * @param gv_info_list
 * @return true
 * @return false
 */
bool findConfigVariable(std::vector<struct ConfigVariableNameInfo *> config_names, 
                        GlobalVariable *gv, 
                        std::vector<struct GlobalVariableInfo *> &gv_info_list)
{
    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Find whether it is a Config Variable.\n");
    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "GlobalVariable:\n\t");
    MY_DEBUG(_REDUNDENCY_LEVEL, gv->print(llvm::outs()));
    MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "\n");

    // Usually DIGlobalVariableExpression only has 2 elements: a DIGlobalVariable and a DIExpression
    SmallVector<DIGlobalVariableExpression *, 2> GVEs;
    gv->getDebugInfo(GVEs);
    if (GVEs.empty())
        return false;
    DIGlobalVariable *di_gv = GVEs[0]->getVariable();

    vector<unsigned> offsets;
    if (handleDIType(di_gv->getType(), config_names, gv, gv_info_list, ERR_OORANGE, offsets, 0))
        return true;

    return false;
}

void collectDFSReachableBB(vector<BasicBlock *> &children, BasicBlock *bb)
{
    children.push_back(bb);
    for (auto it = succ_begin(bb); it != succ_end(bb); ++it)
    {
        BasicBlock *succ = *it;
        if (std::find(children.begin(), children.end(), succ) == children.end())
        {
            // children.push_back(succ);
            collectDFSReachableBB(children, succ);
        }
    }
}

void collectBFSReachableBB(vector<BasicBlock *> &children, BasicBlock *bb){
    
    queue<BasicBlock *> Q;
    Q.push(bb);
    while (!Q.empty())
    {
        BasicBlock *block = Q.front();
        children.push_back(block);

        for (auto it = succ_begin(block); it != succ_end(block); ++it)
        {
            BasicBlock *succ = *it;

            // to find if `succ` already in Q (i.e., detect loop with 2-3 nodes)
            bool searchFlag = false;
            for (unsigned i = 0; i < Q.size(); ++i)
            {
                if (Q.front() == succ)
                    searchFlag = true;

                // the only way to iterate the queue.
                Q.push(Q.front());
                Q.pop();
            }
            // to find if `succ` already in `children` (i.e., detect loop with 4-INF nodes)
            if (std::find(children.begin(), children.end(), succ) != children.end())
                searchFlag = true;

            if (searchFlag == false)
                Q.push(succ);
        }
        Q.pop();
    }
}

void expandToAllReachableBB(vector<BasicBlock *> &childrens)
{
    if (childrens.size() != 2 || childrens[0] == childrens[1])
    {
        llvm::outs() << "[ERROR] getOperand(0)/(1), num_op=" << childrens.size() << " (expected: 2), or something wrong here.\n";
        return;
    }
    queue<BasicBlock *> Q;
    Q.push(childrens[0]);
    Q.push(childrens[1]);
    while (!Q.empty())
    {
        BasicBlock *block = Q.front();
        childrens.push_back(block);

        for (auto it = succ_begin(block); it != succ_end(block); ++it)
        {
            BasicBlock *succ = *it;

            bool searchFlag = false;
            for (unsigned i = 0; i < Q.size(); ++i)
            {
                if (Q.front() == succ)
                    searchFlag = true;
                Q.push(Q.front());
                Q.pop();
            }
            if (std::find(childrens.begin(), childrens.end(), succ) != childrens.end())
                searchFlag = true;
            if (searchFlag == false)
                Q.push(succ);
        }
        Q.pop();
    }
}

void collectBFSReachableBB(vector<BasicBlock *> &children, BasicBlock *bb, BasicBlock *dest)
{
    queue<BasicBlock *> Q;
    Q.push(bb);
    while (!Q.empty())
    {
        BasicBlock *block = Q.front();
        children.push_back(block);

        for (auto it = succ_begin(block); it != succ_end(block); ++it)
        {
            BasicBlock *succ = *it;
            if (succ == dest)
                continue;

            bool searchFlag = false;
            for (unsigned i = 0; i < Q.size(); ++i)
            {
                if (Q.front() == succ)
                    searchFlag = true;
                Q.push(Q.front());
                Q.pop();
            }
            if (std::find(children.begin(), children.end(), succ) != children.end())
                searchFlag = true;
            if (searchFlag == false)
                Q.push(succ);
        }
        Q.pop();
    }
}

/**
 *
 *          Argument* arg = func_info->Ptr->getArg(func_info->ArgIndex);
 *          traceUser(arg, func_info, nullptr);
 * @param cur_value ___|      |          |
 * @param func_info __________|          |
 * @param prev_inst_info ________________|
 */
void traceUser(Value *cur_value, struct FuncInfo *func_info, struct InstInfo *prev_inst_info, uint level)
{
    MY_DEBUG( _DEBUG_LEVEL,  printTabs(level+1));
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[== " << __func__ << " ==]\n");
    /**********************************************************************************************
     **                                                ____ llvm::GlobalValue ____ llvm::GlobalObject ___ llvm:Function
     **                                               /
     **                            ____ llvm::Constant
     **                           /
     **    llvm:Value --- llvm:User ------llvm::DerivedUser
     **          \              / \_____
     **           \             \       llvm::Instruction
     **      llvm:Argument       \____
     **                               llvm::Operator
     **
     ********************************************************************************************/

    vector<User *> UserVec = getSequenceUsers(cur_value);
    unsigned user_ite_cnt = 0;
    if(UserVec.size() == 0){
        MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
        MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "[Ret] Failed to find any user of ");
        MY_DEBUG(_WARNING_LEVEL, cur_value->print(llvm::outs()));
        return;
    }


    for (auto i = UserVec.rbegin(), e = UserVec.rend(); i != e; i++)
    {
        User *cur_user = *i;

        MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
        MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Current User: level " << level << "\n");
        MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
        MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "  ↳");
        MY_DEBUG(_WARNING_LEVEL, cur_user->print(llvm::outs()));
        if (isa<Instruction>(cur_user))
        {
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "\t");
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << getSrcLoc(dyn_cast<Instruction>(cur_user)).toString() << "\n");
        }
        else
        {
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "\n");
        }

        if (Instruction *cur_inst = dyn_cast<Instruction>(cur_user))
        {
            if (prev_inst_info && cur_inst == prev_inst_info->InstPtr)
            {
                MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "[Continue to next user] `Previous ins` == `Current ins`. Mostly because tracing user of *addr* of a storeIns: [" << getClassType(cur_inst) << "]\n");
                continue;
            }

            struct SrcLoc srcloc = getSrcLoc(cur_inst);
            struct InstInfo *inst_info = new InstInfo(cur_inst, srcloc);

            /// NOTE: If current instruction is before the previous one
            ///       (within same function), we should not record.
            if (prev_inst_info && comesBefore(inst_info->InstPtr, prev_inst_info->InstPtr))
            {
                MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "[Continue to next user] current instruction is before the previous one within same function:" << getClassType(cur_inst) << "]\n");
                MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                MY_DEBUG(_WARNING_LEVEL, cur_inst->print(llvm::outs()));
                continue;
            }

            /// NOTE: If the user is a continous StoreInst with same StoreAddr, we don't follow it.
            if (prev_inst_info &&
                isa<StoreInst>(prev_inst_info->InstPtr) &&
                isa<StoreInst>(inst_info->InstPtr) &&
                dyn_cast<StoreInst>(prev_inst_info->InstPtr)->getPointerOperand() ==
                    dyn_cast<StoreInst>(inst_info->InstPtr)->getPointerOperand())
            {
                MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "[Continue to next user] Don't trace continuous StoreInst.\n");
                continue;
            }

            /// NOTE: record visitedLSInst, since they are only one input and one output.
            if (isa<StoreInst>(inst_info->InstPtr) || isa<LoadInst>(inst_info->InstPtr))
            {
                if (std::find(visitedLoadStoreInstList.begin(), visitedLoadStoreInstList.end(), inst_info->InstPtr) != visitedLoadStoreInstList.end())
                {
                    MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "[Continue to next user] Don't trace visited Load/Store Inst.\n");
                    continue;
                }
            }

            /// Mark as visted
            visitedLoadStoreInstList.push_back(inst_info->InstPtr);

            /// add to the double linked list.
            if (prev_inst_info == nullptr)
            {
                func_info->ArgInstInfoList.push_back(inst_info);
                inst_info->Predecessor = nullptr;
            }
            else
            {
                if (findVisitedInstruction(inst_info, prev_inst_info))
                {
                    MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "This should not happen. Check me at line " << __LINE__ << "\n");
                    continue;
                }
                if (prev_inst_info->InfluenceLevel == WEAK)
                    inst_info->InfluenceLevel = WEAK;

                prev_inst_info->Successors.push_back(inst_info);
                inst_info->Predecessor = prev_inst_info;
				inst_info->isDirect = prev_inst_info->isDirect;
            }

            /// NOTE: handle different instructions.
            if (CallBase *call = dyn_cast<CallBase>(cur_inst))
            {
                Function *func = call->getCalledFunction();

                /// IGNORE: function pointer.
                if (func == nullptr)
                    continue;
                /// IGNORE: Debug-generated intrinsic functions, e.g. llvm.dbg.declare().
                if (func->isIntrinsic())
                    continue;
                /// IGNORE: recursive calling
                if (func == func_info->Ptr)
                    continue;

                /*
                 * Make this callInst into a `FuncInfo`
                 */
                string func_name = getOriginalName(func->getName());
                unsigned arg_index = getFuncArgIndex(call, cur_value);
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Function Name : " << func_name << "\n");
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Output: current Arg Index is : " << arg_index << "\n");
                struct FuncInfo *new_func_info = new FuncInfo(func, func_name, arg_index, inst_info->InstLoc.toString());
                unsigned index = isFuncInfoRecorded(new_func_info, func_info->InsideFuncInfoList);
                if (index == ERR_OORANGE)
                {
                    func_info->InsideFuncInfoList.push_back(new_func_info);
                }
                else
                {
                    new_func_info = func_info->InsideFuncInfoList[index];
                    new_func_info->CallLocList.push_back(inst_info->InstLoc.toString());
                }

                /// IGNORE: lib function.
                if (std::find(CommonLibFunctions.begin(), CommonLibFunctions.end(), func_name) != CommonLibFunctions.end())
                {
                    MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[PASS] Calling to lib-function, check tainter.cpp `CommonLibFunctions`:" << func_name << "\n");
                    continue;
                }

                /// IGNORE: If this Function has variable arguments' number, maybe we don't need to follow.
                if (func_info->Ptr->isVarArg())
                {
                    MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[ERROR, PASS] (but strange): Current call Arg Index is larger than Function Arguments: " << func_name << "\n");
                    continue;
                }

                if (func_info->hasInsideDataFlowInfluence == UNDEFINE)
                    // in traceFunction, we use `iterateAndCheck` to determine if there is `hasInsideDataFlowInfluence` outisde to this func.
                    traceFunction(new_func_info, level);

                // So if there is no `hasInsideDataFlowInfluence` or no ret val, do not trace return value
                if (new_func_info->Ptr->getReturnType()->isVoidTy() || new_func_info->hasInsideDataFlowInfluence == NO)
                {
                    MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[PASS] return is Void, OR, no data flow to the return value.");
                    continue;
                }else
                    traceUser(call, new_func_info, inst_info, level);
            }
            else if (StoreInst *store = dyn_cast<StoreInst>(cur_inst))
            {
                //////////////////////////
                /// TODO: do the same thing as in handleInstrcution.
                //////////////////////////
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a StoreInst.\n");
                // trace the store address of StoreInst.
                Value *store_addr = store->getPointerOperand();

                /// BUG: `store_addr`'s `Users` *contain* `cur_inst_info->InstPtr` and
                ///       in `handleUser`, a `if` will continue consequtive StoreInst
                ////////////////////////////
                if (GetElementPtrInst *gep_inst = dyn_cast<GetElementPtrInst>(store_addr))
                {

                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "This StoreInst stores to a address obtained by GetElementPtrInst, ");

                    struct SrcLoc srcloc = getSrcLoc(gep_inst);

                    if (srcloc.filenameHasString("include/c++") || srcloc.dirHasString("include/c++"))
                    {
                        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "but from CXX library, IGNORE.");
                    }
                    else if (!gep_inst->hasIndices() ||
                            gep_inst->getNumOperands() < 3 ||
                            !gep_inst->hasAllConstantIndices())
                    {
                        /** Good Example:
                         ** %field_i = getelementptr inbounds %struct.TTT, %struct.TTT* %6, i32 0, i32 8
                        **                                               |       0        |  1  |  2  |   */
                        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "but num_op = " << gep_inst->getNumOperands() << " (TODO) or not all indices are constant\n");
                        MY_DEBUG(_DEBUG_LEVEL, srcloc.print(1));
                    }
                    else
                    {
                        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Analyzing this GetElementPtrInst.\n");

                        int num = gep_inst->getNumOperands();

                        /// operand 0 (class/struct type)
                        Type *type = gep_inst->getOperand(0)->getType();

                        /// operand 1-n (all constant operand)
                        vector<int> indices;
                        bool ignore = false;
                        for (int i = 1; i < num; i++)
                        {
                            if (ConstantInt *second_offset = dyn_cast<ConstantInt>(gep_inst->getOperand(i)))
                            {
                                indices.push_back(second_offset->getSExtValue());
                            }
                            else
                            {
                                ignore = true;
                            }
                        }
                        // some operand is not constant int, but is int, I don't know why.
                        if (ignore)
                        {
                            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOTE] Ignore this GetElementPtrInst, some operand is not ConstantInt\n");
                            MY_DEBUG(_DEBUG_LEVEL, gep_inst->print(llvm::outs()));
                            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n");
                            MY_DEBUG(_DEBUG_LEVEL, srcloc.print(1));
                        }
                        else
                        {
                            // make this gep_inst into a gep_inst_info, and add this gep_inst_info to the Successors/Predecessor list
                            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOTE] We can handle this GetElementPtrInst.\n");
                            struct SrcLoc srcloc = getSrcLoc(gep_inst);
                            struct InstInfo *inst_info_gep = new InstInfo(gep_inst, srcloc);
                            inst_info->Successors.push_back(inst_info_gep);
                            inst_info_gep->Predecessor = inst_info;
			    			inst_info_gep->isDirect = inst_info->isDirect;

                            /*
                            * Find same type with same offset but not identical gep_inst
                            * This step is NOT accurate with an assumption that "same type with same offset" is for conf only (if yes).
                            *
                            *    1. iterate over all get_inst. find the matched one.
                            */
                            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOTE] Finding GetElementPtrInst with same type, same offset, but not identical one.\n");
                            Value *matched_ins = nullptr;
                            matched_ins = FetchValue4FurtherFollow(type, &indices, gep_inst);
                            if (matched_ins == nullptr)
                            {
                                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOT FOUND] software store conf to a xxx.yyy but never use it? Strange!.\n");
                            }
                            else
                            {
                                /*
                                *    2. add matched_ins_info to list, then follow the matched_ins_info recursively.
                                */
                                if (Instruction *matched_instruction = dyn_cast<Instruction>(matched_ins))
                                {
                                    /*
                                    *    2.1.  make this matched_ins into a matched_ins_info, and add this matched_ins_info to the Successors/Predecessor list
                                    */
                                    struct SrcLoc srcloc = getSrcLoc(matched_instruction);
                                    struct InstInfo *inst_info_matched_ins = new InstInfo(matched_instruction, srcloc, false);
                                    inst_info_gep->Successors.push_back(inst_info_matched_ins);
                                    inst_info_matched_ins->Predecessor = inst_info_gep;
				    				inst_info_matched_ins->isDirect = inst_info_gep->isDirect;
                                    /*
                                    *    2.2.  follow the matched_ins_info recursively.
                                    */
                                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[FOUND] Follow this GetElementPtrInst recursively.\n");
                                    traceUser(matched_instruction, func_info, inst_info_gep, level + 1);
                                }
                                else
                                {
                                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[WRONG] check me at line " << __LINE__ << "\n");
                                }
                            }
                            // printSequenceUsers(gep_inst->getOperand(0));
                        }
                    }

                    /// NOTE: The case we handle here is like:
                    /*
                    *  define dso_local void @_Z11testAddressRii(i32* dereferenceable(4) %a, i32 %b) #0 !dbg !935 {
                    *       entry:                                           \______
                    *       %a.addr = alloca i32*, align 8                           when caller may give a reference, we need be caution if tainting to it.
                    *       %b.addr = alloca i32, align 4
                    *       store i32* %a, i32** %a.addr, align 8  <------------------- (3) %a.addr is stored with an address - %a (first argument)
                    *       store i32 %b, i32* %b.addr, align 4
                    *       %0 = load i32, i32* @CONFIG_VAR, align 4, !dbg !942
                    *       %1 = load i32, i32* %b.addr, align 4, !dbg !943
                    *       %or = or i32 %0, %1, !dbg !944
                    *       %2 = load i32*, i32** %a.addr, align 8, !dbg !945 <-------- (2) %2 is the address stored in %a.addr
                    *       store i32 %or, i32* %2, align 4, !dbg !946 <--------------- (1) when store to %2
                    */
                }
                else if (LoadInst *load_inst = dyn_cast<LoadInst>(store_addr))
                {

                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Pointer analysis: this StoreInst stores to a address loaded by LoadInst, follow the `addr` of it load from.\n");
                    Value *load_addr = load_inst->getOperand(0);

                    /// NOTE: `load_addr` is the address where maybe stored an address to (reference to) a caller's variable
                    ///        but note that if you print the class of `users_of_load_addr`, you may got a 'User::DerivedUser',
                    ///        which is usually caused by instruction like: "a.addr = alloca i32*, align 8"
                    vector<User *> users_of_load_addr = getSequenceUsers(load_addr);

                    /// NOTE: Rather than cast `users_of_load_addr` to instruction, we need to search in among the users of
                    ///       this address to see: if there is a StoreInst ever store something into this address. If yes,
                    ///       and this StoreInst store the address (which is also a function arguments) to it, then, the taint
                    ///       is out this function. We need to visit callgraph to follow further in the caller.
                    for (vector<User *>::iterator I_user = users_of_load_addr.begin(); I_user != users_of_load_addr.end(); I_user++)
                    {

                        if (StoreInst *sb_store_to_this_addr = dyn_cast<StoreInst>(*I_user))
                        {

                            if (comesBefore(sb_store_to_this_addr, load_inst))
                            {
                                Value *tobe_followed_reference = sb_store_to_this_addr->getOperand(0);

                                /// If hits at least one argument. If yes, visit callgraph to follow further in the caller.
                                Function *thisFun = load_inst->getFunction();
                                uint arg_index = 0;
                                for (Function::arg_iterator arg_iter = thisFun->arg_begin(); arg_iter != thisFun->arg_end(); arg_iter++, arg_index++)
                                {

                                    if (arg_iter == tobe_followed_reference)
                                    {
                                        /*
                                        * Get users of the ReturnInst (which is a CallBase)
                                        */
                                        vector<pair<CallBase *, Function *> *> callers = getCallerAndCallInst(thisFun);
                                        printCallers(callers, level + 1);

                                        /*
                                        * For every users (CallBase) of the ReturnInst, trace their correspoding operand (argument).
                                        */
                                        for (vector<pair<CallBase *, Function *> *>::iterator i = callers.begin(); i != callers.end(); i++)
                                        {
                                            CallBase *caller_inst = (*i)->first;
                                            struct SrcLoc srcloc = getSrcLoc(caller_inst);
                                            struct InstInfo *inst_info_caller = new InstInfo(caller_inst, srcloc);
                                            inst_info->Successors.push_back(inst_info_caller);
                                            inst_info_caller->Predecessor = inst_info;
					    					inst_info_caller->isDirect = inst_info->isDirect;

                                            Value *tainted_tobe_followed = caller_inst->getArgOperand(arg_index);
                                            tainted_tobe_followed->print(llvm::outs());
                                            llvm::outs() << "\n";

                                            traceUser(tainted_tobe_followed, func_info, inst_info_caller, level + 1);
                                        }
                                    }
                                }
                            } /// NOTE: NO `else` here, since if not comesBefore, the case can be passed happily.

                        } /// NOTE: NO `else` here, since `handleUser(store_addr, gv_info, cur_inst_info, level+1);` will handle normal cases.
                    }
                }
                else
                { // May be the class of 'store_addr' is 'User::DerivedUser'. Don't care
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Pointer analysis: this StoreInst stores to a address allocated by ["<< getClassType(store_addr) <<"], Currently, no need pointer analysis futher.\n");
                }

                /// TODO: fix bug here.
                // Delete at 2022 10 05. Forgot why I had written this line.
                // Undelete at 2022 10 10. Revert.
                traceUser(store_addr, func_info, inst_info, level + 1);

                ///////////////////////////////
                //Value *store_addr = store->getPointerOperand(); // e.g.  "store i32 %1, i32* @somevariable"
                //traceUser(store_addr, func_info, inst_info, level);    //                "@somevariable" should be traced
                /////////////////////////////////////////////////////////////////////////////////////
            }
            else if (isa<FenceInst>(cur_inst) ||
                     isa<AtomicCmpXchgInst>(cur_inst) ||
                     isa<AtomicRMWInst>(cur_inst))
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a FenceInst/AtomicCmpXchgInst/AtomicRMWInst, STOP.\n");
            }
            else if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(cur_inst))
            {
                //////////////////////////
                /// Why this inst ?
                //////////////////////////

                /// whatever getNumOperands() = 3 or 2, we only trace the base situations.
                if (gep->getOperand(0) == cur_value)
                {
                    traceUser(gep, func_info, inst_info, level);
                }
                if (gep->getNumOperands() == 2 && gep->getOperand(1) == cur_value)
                {
                    inst_info->InfluenceLevel = WEAK;
                    traceUser(gep, func_info, inst_info, level);
                }
                if (gep->getNumOperands() == 3 && (gep->getOperand(1) == cur_value || gep->getOperand(2) == cur_value))
                {
                    inst_info->InfluenceLevel = WEAK;
                    traceUser(gep, func_info, inst_info, level);
                }
            }
            /*
                Terminator Instruction:
                    IndirectBrInst, BranchInst, ReturnInst, SwitchInst, InvokeInst?, CallBrInst?,
                    ResumeInst, CatchSwitchInst, CatchReturnInst, CleanupReturnInst,
            */
            else if (BranchInst *branch = dyn_cast<BranchInst>(cur_inst))
            {
                /// TODO: Do we only consider dataflow inside Function Tracing?
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a BranchInst.\n");
            }
            else if (ResumeInst *resume_inst = dyn_cast<ResumeInst>(cur_inst))
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a ResumeInst.\n");
            }
            else if (SwitchInst *switch_inst = dyn_cast<SwitchInst>(cur_inst))
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a SwitchInst.\n");
                /// TODO: I think we should record all the case BBs as influential area.
            }
            else if (CatchSwitchInst *catch_switch = dyn_cast<CatchSwitchInst>(cur_inst))
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a CatchSwitchInst.\n");
            }
            else if (CatchReturnInst *catch_return = dyn_cast<CatchReturnInst>(cur_inst))
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a CatchReturnInst.\n");
            }
            else if (CleanupReturnInst *cleanup_ret = dyn_cast<CleanupReturnInst>(cur_inst))
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a CleanupReturnInst, stop tracing.\n");
            }
            else if (ReturnInst *ret_inst = dyn_cast<ReturnInst>(cur_inst))
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a ReturnInst, stop tracing. We will determine if need to trace further from the ret of callcite of Caller Function \n");

                /// NOT_TODO: If the dataflow go to a return value, do we need to continue analyzing the usage of current Caller Function.
                /// WHY we need to do this? We have already made it in TraceFunction.
                /// What we need to do is to handle ReturnInst in `handleInstrcution`
            }
            else if (isa<AllocaInst>(cur_inst) || // generally don't need consider.
                     isa<CastInst>(cur_inst) ||
                     isa<ExtractValueInst>(cur_inst) ||
                     isa<FreezeInst>(cur_inst) ||
                     isa<LoadInst>(cur_inst) ||
                     isa<UnaryOperator>(cur_inst) ||
                     // isa<VAArgInst>(cur_inst) || // not usable at present.
                     isa<BinaryOperator>(cur_inst) ||
                     isa<CmpInst>(cur_inst) ||
                     isa<InsertValueInst>(cur_inst) ||
                     isa<PHINode>(cur_inst) ||
                     isa<SelectInst>(cur_inst) ||
                     isa<FreezeInst>(cur_inst) ||
                     isa<ShuffleVectorInst>(cur_inst) ||
                     isa<ExtractElementInst>(cur_inst) ||
                     isa<InsertElementInst>(cur_inst) ||
                     isa<FuncletPadInst>(cur_inst) ||
                     isa<LandingPadInst>(cur_inst) ||
                     isa<BitCastInst>(cur_inst) ||
                     isa<TruncInst>(cur_inst))
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a " << cur_inst->getOpcodeName() << " Instruction.\n");
                // go further
                traceUser(cur_inst, func_info, inst_info, level);
            }

            else
            {
                // even don't know what is it, just keep tracing
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Unknown type of instruction, but still trace it.\n");
                traceUser(cur_inst, func_info, inst_info, level);
            }
        }

        else if (GEPOperator *gepo = dyn_cast<GEPOperator>(cur_user))
        {

            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Here!!! GEPOperator in traceUser() !!! Should not happened!!\n");
            MY_DEBUG(_ERROR_LEVEL, gepo->print(llvm::outs()));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "\n");

            /// whatever getNumOperands() = 3 or 2, we only trace the base situations.
            if (gepo->getOperand(0) == cur_value)
            {
                traceUser(gepo, func_info, prev_inst_info, level);
            }
        }

        else if (ConstantExpr *constant_expr = dyn_cast<ConstantExpr>(cur_value))
        {
            llvm::outs() << "[WARNING] Unhandled User Situation in traceUser: user is a ConstantExpr\n";
            continue;
        }

        else if (ConstantAggregate *constant_struct = dyn_cast<ConstantAggregate>(cur_value))
        {
            llvm::outs() << "[WARNING] Unhandled User Situation in traceUser: user is a ConstantAggregate\n";
            continue;
        }

        else
        {
            MY_DEBUG(_DEBUG_LEVEL,
                     llvm::outs() << "[WARNING] Unhandled User Situation in traceUser:\n";
                     cur_user->print(llvm::outs());
                     llvm::outs() << "\n";
                     cur_user->getType()->print(llvm::outs());
                     llvm::outs() << "\n";
                     string class_type = getClassType(cur_user);
                     llvm::outs() << "This is a " << class_type << " Class\n\n";)
        }
    }
}

/// OUTPUT: If there is any ReturnInst in the chain, return true.
bool iterateAndCheck(struct InstInfo *inst_info)
{
    if (isa<ReturnInst>(inst_info->InstPtr))
        return true;
    bool aa = false;
    for (auto it = inst_info->Successors.begin(); it != inst_info->Successors.end(); ++it)
    {
        struct InstInfo *succ = *it;
        aa = aa || iterateAndCheck(succ);
    }
    return aa;
}

/// OUTPUT: Return true if current argument has a dataflow to the return value of this function.
void traceFunction(struct FuncInfo *func_info, uint level)
{
    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[## " << __func__ << " ##]\n");
    // I do not think this is possible ?
    //     func_info->Ptr  ~~ Function *
    //     arg_size()  -- number of arguments
    if (func_info->ArgIndex >= func_info->Ptr->arg_size())
    {
        func_info->hasInsideDataFlowInfluence = NO;
        return;
    }
    /*************************************
     **  llvm::Value <--- llvm::Argument
     *************************************/
    Argument *arg = func_info->Ptr->getArg(func_info->ArgIndex);
    traceUser(arg, func_info, nullptr, level+1);

    /*
     * If there exist at least one data flow from one of the function argument to the return value.
     *     if YES, the return value will be continued to be followed in the caller function
     *     Otherwise, won't.
     */
    bool flag_has_influence = false;
    for (unsigned i = 0; i < func_info->ArgInstInfoList.size(); i++)
    {
        struct InstInfo *inst_info = func_info->ArgInstInfoList[i];
        flag_has_influence = iterateAndCheck(inst_info);
    }

    if (flag_has_influence == true)
        func_info->hasInsideDataFlowInfluence = YES;
    else
    {
        func_info->hasInsideDataFlowInfluence = NO;
    }
}


struct InstInfo *MkNewInstInfoAndLinkOntoPrevInstInfo(struct Instruction *cur_inst,
                                                      struct InstInfo *prev_inst_info)
{
    struct SrcLoc srcloc = getSrcLoc(cur_inst);
    struct InstInfo *cur_inst_info = new InstInfo(cur_inst, srcloc, true);
    prev_inst_info->Successors.push_back(cur_inst_info);
    cur_inst_info->isDirect = prev_inst_info->isDirect;
    cur_inst_info->Predecessor = prev_inst_info;
    return cur_inst_info;
}

struct InstInfo *MkNewInstInfoAndLinkOntoPrevInstInfo(struct Instruction *cur_inst,
                                                      struct InstInfo *prev_inst_info,
                                                      bool add_tab)
{
    struct SrcLoc srcloc = getSrcLoc(cur_inst);
    struct InstInfo *cur_inst_info = new InstInfo(cur_inst, srcloc, add_tab);
    prev_inst_info->Successors.push_back(cur_inst_info);
    cur_inst_info->isDirect = prev_inst_info->isDirect;
    cur_inst_info->Predecessor = prev_inst_info;
    return cur_inst_info;
}

#define PHIBB_UNION
#undef PHIBB_INTERSECT
void calcPHIedBB(BasicBlock * left, BasicBlock * right, vector<BasicBlock*> &res){
    vector<BasicBlock *> left_children, right_children, tmp;
    collectBFSReachableBB(left_children, left);
    collectBFSReachableBB(right_children, right);

#ifdef PHIBB_INTERSECT
    // intersection
    for(auto &i:left_children){
        for(auto &j:right_children){
            if(i==j){
                res.push_back(i);
                break;
            }
        }
    }
#endif
    
#ifdef PHIBB_UNION
    // union
    tmp.assign(right_children.begin(), right_children.end());
    for(auto &i:left_children){
        if(std::find(right_children.begin(), right_children.end(), i) == right_children.end())
            // left contains but right not contains
            tmp.push_back(i);
    }
#endif

    /// LAST: de-dup
    for(auto j=tmp.begin(); j!=tmp.end(); j++)
    {
        BasicBlock * bb = *j;
        if(std::find(res.begin(), res.end(), bb) == res.end())
            res.push_back(bb);
    }
}

bool reachable(BasicBlock* FromBB, BasicBlock* ToBB){
    vector<BasicBlock*> children;
    collectBFSReachableBB(children, FromBB);
    if(std::find(children.begin(), children.end(), ToBB) == children.end())
        return false;
    else
        return true;
}

bool taintPHINode(PHINode* phi_inst,
                  DominatorTree* DT,
                  BasicBlock* leftBB,
                  BasicBlock* rightBB)
{
    uint inBlockNum = phi_inst->getNumIncomingValues();
    uint left_dom=0, right_dom=0;

    /// CORE: exist a successor of “if(config)” which dominates 
    ///       at least one but not all predecessors of PHI-instruction

    // for every predecessors of PHI-instruction
    for(uint i=0; i<inBlockNum; i++){

        BasicBlock * preNode = phi_inst->getIncomingBlock(i);

        if( DT->dominates( &*(leftBB->begin()), &*(preNode->begin()) ) || leftBB == preNode){
            left_dom++;
        }
        if( DT->dominates( &*(rightBB->begin()), &*(preNode->begin()) )  || rightBB == preNode){
            right_dom++;
        }
    }
    if(left_dom < inBlockNum && left_dom > 0 || right_dom < inBlockNum && right_dom > 0)
        return true;
    else
        return false;
}

// 1nd argument (BBsPhi) is the basic block set that need to be anaylzed for "PHI cases".
void handlePHINodesFromBBs(vector<BasicBlock *> &BBsPhi, // candidate BB where we find candidate PHI
                           BasicBlock * leftBB,          // left BB of the `branchInst`
                           BasicBlock * rightBB,         // right BB of the `branchInst`
                           DominatorTree * DT,      // to calculate the POST-DOMINANCE relation
                           unsigned level,
                           struct GlobalVariableInfo *gv_info, // if a candidate win, use it to trace further.
                           struct InstInfo *cur_inst_info)     // if a candidate win, use it to trace further.
{
    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[== " << __func__ << " ==]\n");

    // for iteration of all instruction
    for (vector<BasicBlock *>::iterator iB = BBsPhi.begin(); iB != BBsPhi.end(); iB++){

        // traverse each instruction, find PHINode.
        for (BasicBlock::iterator inst = (*iB)->begin(); inst != (*iB)->end(); inst++){
        
            // for the PHINode, determine if should be tainted further.
            if (PHINode *phi_inst = dyn_cast<PHINode>(inst)){
        
                // CORE: the rule to determine if taint this phi_inst.
                if(taintPHINode(phi_inst, DT, leftBB, rightBB)){

                    struct InstInfo *the_phi_ins = MkNewInstInfoAndLinkOntoPrevInstInfo(phi_inst, cur_inst_info, false);
		    		the_phi_ins->isDirect = false; // starting here, turn into indirect data-flow
                    
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "A PHI-Node is tainted:\n");
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, phi_inst->print(llvm::outs()));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n");
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << the_phi_ins->InstLoc.toString() << "\n");

                    //handleUser(phi_inst, gv_info, the_phi_ins, level+1);
                }
            }
        }
    }
}


// 1st argument (BBs) is the tainted basic block set that the 3rd argument (cur_inst_info) produces.
// 3rd argument (cur_inst_info) is the branchInst.
void handleControFlowFromBBs(vector<BasicBlock *> &BBs,
                             struct GlobalVariableInfo *gv_info,
                             struct InstInfo *cur_inst_info, // is a branchInst
                             unsigned level)
{
    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[== " << __func__ << " ==]\n");

    // iterate every BB in vector<BasicBlock*> BBs, handle
    // store ins and call ins only (for now)
    for (vector<BasicBlock *>::iterator iB = BBs.begin(); iB != BBs.end(); iB++)
    {

        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "+-----------------------------\n");
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "|  BasicBlock \"" << (*iB)->getName() << "\"\n");
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "+-----------------------------\n");

        vector<StoreInst *> store_ins_set;
        vector<CallBase *> call_ins_set;

        // clear store_ins_set and call_ins_set
        vector<StoreInst *>::iterator iter_s = store_ins_set.begin();
        for (; iter_s != store_ins_set.end();)
            iter_s = store_ins_set.erase(iter_s);
        vector<CallBase *>::iterator iter_c = call_ins_set.begin();
        for (; iter_c != call_ins_set.end();)
            iter_c = call_ins_set.erase(iter_c);

        // for every ins in this BB
        for (BasicBlock::iterator inst = (*iB)->begin(); inst != (*iB)->end(); inst++)
        {

            if (StoreInst *store_inst = dyn_cast<StoreInst>(inst))
            {
                store_ins_set.push_back(store_inst);
            }
            else if (CallBase *call_inst = dyn_cast<CallBase>(inst))
            {
                if (call_inst->getCalledFunction() &&
                    !call_inst->getCalledFunction()->isIntrinsic() &&
                    !isSubStr(call_inst->getCalledFunction()->getName().str(), "llvm.dbg"))
                {
                    call_ins_set.push_back(call_inst);
                } // else it is a callBase but without any function.
            }
        }

        if (store_ins_set.empty() && call_ins_set.empty())
        {

            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "No storeInst or callBase in this basicBlock. Control flow in this basic block stop here.\n");
            continue;
        }
        else if (!store_ins_set.empty() && call_ins_set.empty())
        {

            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Only storeInst (no callBase) in this basicBlock. Control flow continues from the stored address.\n");
#ifdef CONTROL_STORE
            for (vector<StoreInst *>::iterator i = store_ins_set.begin(); i != store_ins_set.end(); i++)
            {
                struct InstInfo *the_store_ins = MkNewInstInfoAndLinkOntoPrevInstInfo(*i, cur_inst_info, false);
                handleInstruction((*i)->getValueOperand(), gv_info, the_store_ins, level + 1);
            }
#else
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Def `CONTROL_STORE` to enable storeInst.\n");
#endif
        }
        else if (store_ins_set.empty() && !call_ins_set.empty())
        {

            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Only callBase (no storeInst) in this basicBlock. We take all calls as the influenced functions finally. And control flow in this basic block stop here.\n");
            for (vector<CallBase *>::iterator i = call_ins_set.begin(); i != call_ins_set.end(); i++)
            {
                cur_inst_info->addControllingFuncs((*i)->getCalledFunction());
                gv_info->InfluencedFuncList[gv_info->currentGVStartingFuncName].push_back((*i)->getCalledFunction());
            }
        }
        else
        {

            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Both storeInst and callBase in this basicBlock. The most complex situation here. Control flow continues from the stored address and take all calls as the influenced functions\n");
#ifdef CONTROL_STORE
            for (vector<StoreInst *>::iterator i = store_ins_set.begin(); i != store_ins_set.end(); i++)
            {
                struct InstInfo *the_store_ins = MkNewInstInfoAndLinkOntoPrevInstInfo(*i, cur_inst_info, false);
                handleInstruction((*i)->getValueOperand(), gv_info, the_store_ins, level + 1);
            }
#else
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Def `CONTROL_STORE` to enable storeInst.\n");
#endif
            for (vector<CallBase *>::iterator i = call_ins_set.begin(); i != call_ins_set.end(); i++)
            {
                cur_inst_info->addControllingFuncs((*i)->getCalledFunction());
                gv_info->InfluencedFuncList[gv_info->currentGVStartingFuncName].push_back((*i)->getCalledFunction());
            }
        }
    }
}

unsigned isFuncInfoRecorded(struct FuncInfo *_funcinfo, vector<struct FuncInfo *> FuncInfoList)
{
    for (unsigned i = 0; i < FuncInfoList.size(); i++)
    {
        struct FuncInfo *cur_fi = FuncInfoList[i];
        if (cur_fi->Ptr == _funcinfo->Ptr &&
            cur_fi->ArgIndex == _funcinfo->ArgIndex)
        {
            return i;
        }
    }
    return ERR_OORANGE;
}

void calcTaintBBfromBr(vector<BasicBlock*> *cur_test_successors, 
                       vector<BasicBlock*> *cur_br_successors, 
                       vector<BasicBlock*> *TaintedBBs, 
                       PostDominatorTree   *PDT)
{
    bool stepFurther = false;
    vector<BasicBlock*> next_test_successors, next_br_successors;

#undef DEBUG_calcTaintBBfromBr
#ifdef DEBUG_calcTaintBBfromBr
    llvm::outs() << "  cur_br_successors:  ";
    for(auto x : *cur_br_successors){
        llvm::outs() << x->getName() << "\t";
    }
    llvm::outs() << "\n";

    llvm::outs() << "cur_test_successors:  ";
    for(auto x : *cur_test_successors){
        llvm::outs() << x->getName() << "\t";
    }
    llvm::outs() << "\n\n";
#endif
    // adding this line of code or not are both OK?
    next_br_successors.insert(next_br_successors.end(), cur_br_successors->begin(), cur_br_successors->end());

    for(auto cur_br_successors_i : *cur_test_successors){

        // determine if `cur_br_successors_i` is control-flow tainted
        for(auto cur_br_successors_j : *cur_br_successors){

            // ------- taint! --------
            // if `cur_br_successors_i` does not post-dominate all successors, it is tainted. (it at least post-dominate itself)
            if (!PDT->dominates(&*cur_br_successors_i->begin(), &*cur_br_successors_j->begin())){

                if(std::find(TaintedBBs->begin(), TaintedBBs->end(), cur_br_successors_i) == TaintedBBs->end())
                    TaintedBBs->push_back(cur_br_successors_i);

                // for one step further in CFG.
                for (BasicBlock *succ_OF_cur_br_successors_i : successors(cur_br_successors_i)){

                    // prevent loop.
                    if(std::find(cur_br_successors->begin(), cur_br_successors->end(), succ_OF_cur_br_successors_i) == cur_br_successors->end()){
                        next_br_successors.push_back(succ_OF_cur_br_successors_i);
                        next_test_successors.push_back(succ_OF_cur_br_successors_i);
                        stepFurther = true;
                    }
                }
                
                // this loop is only to determine if `cur_br_successors_i` is tainted or not, once hit, this layer of loop can over.
                break;
            }

            // ------- NOT taint! --------
            // this basic-block post-dominate all basic-blocks in `cur_br_successors`,
            // in this case, the children of this basic-block either:
            //     1) post-dominates ALL basic-blocks in `cur_br_successors`, OR
            //     2) post-dominates NO basic-block in `cur_br_successors`
        }
    }

    // if do one step further in CFG.
    if(!stepFurther){
        return;
    }

    // do one step further in CFG.
    calcTaintBBfromBr(&next_test_successors, &next_br_successors, TaintedBBs, PDT);
}


/**
 *  call in `handleUser`: handleInstruction(cur_value, gv_info, inst_info, level);
 *                                              \___ isa<GlobalVariable>  (maybe)
 */
void handleInstruction(Value *cur_value, // one of the user of `cur_inst_info->ptr`
                       struct GlobalVariableInfo *gv_info,
                       struct InstInfo *cur_inst_info, // the last inst before this user `cur_value`
                       unsigned level)
{
    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[== " << __func__ << " ==]\n");

    if (!cur_inst_info)
        return;

    Instruction *cur_inst = cur_inst_info->InstPtr;
    if (!cur_value || !cur_inst)
        return;

    /**
     ** `CallBase` is the base class for all callable instructions (`InvokeInst` and `CallInst`)
     **            Holds everything related to calling a function.
     **      NOTE: CallInst     is a regular call,
     **            InvokeInst   is a call with Exception Handling branches.
     ** -----------------------------------------------------------------------------------------------------
     **     void bar() {                    define void @_Z3barv() #0 {
     **         foo();      ==CallInst==>       call void @_Z3foov()
     **     }                                   ret void }
     ** -----------------------------------------------------------------------------------------------------
     **     void bar() {
     **         try {
     **             foo();                  ==InvokeInst==>   invoke void @_Z3foov() to label %5 unwind label %6
     **         } catch (MyError err) {}                      5:
     **     }                                                   br label %18   ## Normal branch, do nothing.
     **                                                       6:
     **                                                         %7 = landingpad { i8*, i32 } ... ## error handling
     ** -----------------------------------------------------------------------------------------------------
     **/
    ///
    if (CallBase *call = dyn_cast<CallBase>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a CallBase.\n");

        Function *func = call->getCalledFunction();

        /** Ignore function pointer.
         **     OR: isIndirectCall()
         **   TODO: Intrinsic::ID CallBase::getIntrinsicID()?
         **         FunctionType * llvm::Intrinsic::getType(LLVMContext & Context, ID id, ArrayRef< Type * > Tys = None)
         **         =?= llvm::Type::TypeID.FunctionTyID
         **         Function * llvm::Intrinsic::getDeclaration(LLVMContext & Context, ID id, ArrayRef< Type * > Tys = None)
         **/
        if (func == nullptr)
            return;
        /// Ignore Debug-generated intrinsic functions, e.g. llvm.dbg.declare().
        if (func->isIntrinsic())
            return;

        /**
         ** userFunctionOfCONF(x, x, .., CONF, ...);
         **                    0  1    arg_index
         **/
        string func_name = getOriginalName(func->getName());
        unsigned arg_index = getFuncArgIndex(call, cur_value);
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Function Name : " << func_name << "\n");
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Output: current Arg Index is : " << arg_index << "\n");

        /// Record current call information.
        struct FuncInfo *func_info = new FuncInfo(func, func_name, arg_index, cur_inst_info->InstLoc.toString());
        unsigned index = isFuncInfoRecorded(func_info, gv_info->FuncInfoList);
        if (index == ERR_OORANGE) // never recorded before
        {
            gv_info->FuncInfoList.push_back(func_info);
        }
        else
        {
            /**
             ** have recorded before, so this another time of calling. In this case, use
             ** `gv_info->FuncInfoList[index]->CallLocList` to add one more element (source location)
             ** i.e., cur_inst_info->InstLoc.toString() which is a string.
             **/
            func_info = gv_info->FuncInfoList[index];
            func_info->CallLocList.push_back(cur_inst_info->InstLoc.toString());
        }

        /// IGNORE: logging library functions.
        if (std::find(CommonLibFunctions.begin(), CommonLibFunctions.end(), func_info->FuncName) != CommonLibFunctions.end())
        {
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "It is a log function, stop. " << func_info->FuncName << ".\n");
            return;
        }

        /// IGNORE: library functions.
        /*
        if (std::find(CommonLibFunctions.begin(), CommonLibFunctions.end(), func_info->FuncName) != CommonLibFunctions.end())
        {
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "It is a lib function, need rule. " << func_info->FuncName << ".\n");
            return;
        }
        */

        /// NOTE: If this Function has variable arguments' number, maybe we don't need to follow.
        ///       e.g., fun(x, y, ..) / fun(x, y).  Corner cases.
        if (func_info->Ptr->isVarArg())
        {
            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "It is a function with variable arguments: " << func_info->FuncName << ".\n");
            return;
        }

        if (func_info->hasInsideDataFlowInfluence == UNDEFINE)
        {    
            traceFunction(func_info, level);
        }
        /// TODO: Do we need to consider Reference Passing in arguments of functions?
        if (func->getReturnType()->isVoidTy() || func_info->hasInsideDataFlowInfluence == NO)
        {
            MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Don't need to trace after this function: " << func_info->FuncName << ".\n");
            return;
        }
        else
        {
            handleUser(call, gv_info, cur_inst_info, level + 1);
        }

        // `call` is a llvm::Function, its parent class is llvm::GlobalValue, this will branch into 'return'.
        /******************************************************************
         *   if(  call != gv_info->Ptr
         *    &&  isa<GlobalVariable>(call)
         *    ){
         *        gv_info->InfluencedGVList.push_back(dyn_cast<GlobalVariable>(call));
         *        return;
         *    }
         ******************************************************************/
    }
    else if (StoreInst *store = dyn_cast<StoreInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a StoreInst.\n");
        // trace the store address of StoreInst.
        Value *store_addr = store->getPointerOperand();

        /// NOTE: if gv is the store addr operand, we don't trace this situation, since it is a destination.
        if (level != 0 && gv_info->NameInfo->VarType == SINGLE && store_addr == gv_info->Ptr)
            return;
        if (level != 0 && (gv_info->NameInfo->VarType == STRUCT || gv_info->NameInfo->VarType == CLASS))
        {
            if (GEPOperator *store_gepo = dyn_cast<GEPOperator>(store_addr))
            {
                if (gv_info->GlobalVariableType != "" &&
                    getStructTypeStrFromPrintAPI(store_gepo->getType()) == gv_info->GlobalVariableType)
                    return;
            }
        }

        /// BUG: `store_addr`'s `Users` *contain* `cur_inst_info->InstPtr` and
        ///       in `handleUser`, a `if` will continue consequtive StoreInst
        ////////////////////////////
        /// TODO
        ////////////////////////////
        // getSequenceUses_OnlyGEPins(store_addr);
        // printSequenceUsers(store_addr);
        if (GetElementPtrInst *gep_inst = dyn_cast<GetElementPtrInst>(store_addr))
        {

            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "This StoreInst stores to a address obtained by GetElementPtrInst, ");

            struct SrcLoc srcloc = getSrcLoc(gep_inst);

            if (srcloc.filenameHasString("include/c++") || srcloc.dirHasString("include/c++"))
            {
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "but from CXX library, IGNORE.");
            }
            else if (!gep_inst->hasIndices() ||
                     gep_inst->getNumOperands() < 3 ||
                     !gep_inst->hasAllConstantIndices())
            {
                /** Good Example:
                 ** %field_i = getelementptr inbounds %struct.TTT, %struct.TTT* %6, i32 0, i32 8
                 **                                               |       0        |  1  |  2  |   */
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "but num_op = " << gep_inst->getNumOperands() << " (TODO) or not all indices are constant\n");
                MY_DEBUG(_DEBUG_LEVEL, srcloc.print(1));
            }
            else
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Analyzing this GetElementPtrInst.\n");

                int num = gep_inst->getNumOperands();

                /// operand 0 (class/struct type)
                Type *type = gep_inst->getOperand(0)->getType();

                /// operand 1-n (all constant operand)
                vector<int> indices;
                bool ignore = false;
                for (int i = 1; i < num; i++)
                {
                    if (ConstantInt *second_offset = dyn_cast<ConstantInt>(gep_inst->getOperand(i)))
                    {
                        indices.push_back(second_offset->getSExtValue());
                    }
                    else
                    {
                        ignore = true;
                    }
                }
                // some operand is not constant int, but is int, I don't know why.
                if (ignore)
                {
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOTE] Ignore this GetElementPtrInst, some operand is not ConstantInt\n");
                    MY_DEBUG(_DEBUG_LEVEL, gep_inst->print(llvm::outs()));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n");
                    MY_DEBUG(_DEBUG_LEVEL, srcloc.print(1));
                }
                else
                {
                    // make this gep_inst into a gep_inst_info, and add this gep_inst_info to the Successors/Predecessor list
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOTE] We can handle this GetElementPtrInst.\n");
                    struct SrcLoc srcloc = getSrcLoc(gep_inst);
                    struct InstInfo *inst_info_gep = new InstInfo(gep_inst, srcloc);
                    cur_inst_info->Successors.push_back(inst_info_gep);
                    inst_info_gep->Predecessor = cur_inst_info;
		    		inst_info_gep->isDirect = cur_inst_info->isDirect;

                    /*
                     * Find same type with same offset but not identical gep_inst
                     * This step is NOT accurate with an assumption that "same type with same offset" is for conf only (if yes).
                     *
                     *    1. iterate over all get_inst. find the matched one.
                     */
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOTE] Finding GetElementPtrInst with same type, same offset, but not identical one.\n");
                    Value *matched_ins = nullptr;
                    matched_ins = FetchValue4FurtherFollow(type, &indices, gep_inst);
                    if (matched_ins == nullptr)
                    {
                        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOT FOUND] software store conf to a xxx.yyy but never use it? Strange!.\n");
                    }
                    else
                    {
                        /*
                         *    2. add matched_ins_info to list, then follow the matched_ins_info recursively.
                         */
                        if (Instruction *matched_instruction = dyn_cast<Instruction>(matched_ins))
                        {
                            /*
                             *    2.1.  make this matched_ins into a matched_ins_info, and add this matched_ins_info to the Successors/Predecessor list
                             */
                            struct SrcLoc srcloc = getSrcLoc(matched_instruction);
                            struct InstInfo *inst_info_matched_ins = new InstInfo(matched_instruction, srcloc, false);
                            inst_info_gep->Successors.push_back(inst_info_matched_ins);
                            inst_info_matched_ins->Predecessor = inst_info_gep;
			    			inst_info_matched_ins->isDirect = inst_info_gep->isDirect;
                            /*
                             *    2.2.  follow the matched_ins_info recursively.
                             */
                            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[FOUND] Follow this GetElementPtrInst recursively.\n");
                            handleUser(matched_instruction, gv_info, inst_info_gep, level + 1);
                        }
                        else
                        {
                            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[WRONG] check me at line " << __LINE__ << "\n");
                        }
                    }
                    // printSequenceUsers(gep_inst->getOperand(0));
                }
            }

            /// NOTE: The case we handle here is like:
            /*
             *  define dso_local void @_Z11testAddressRii(i32* dereferenceable(4) %a, i32 %b) #0 !dbg !935 {
             *       entry:                                           \______
             *       %a.addr = alloca i32*, align 8                           when caller may give a reference, we need be caution if tainting to it.
             *       %b.addr = alloca i32, align 4
             *       store i32* %a, i32** %a.addr, align 8  <------------------- (3) %a.addr is stored with an address - %a (first argument)
             *       store i32 %b, i32* %b.addr, align 4
             *       %0 = load i32, i32* @CONFIG_VAR, align 4, !dbg !942
             *       %1 = load i32, i32* %b.addr, align 4, !dbg !943
             *       %or = or i32 %0, %1, !dbg !944
             *       %2 = load i32*, i32** %a.addr, align 8, !dbg !945 <-------- (2) %2 is the address stored in %a.addr
             *       store i32 %or, i32* %2, align 4, !dbg !946 <--------------- (1) when store to %2
             */
        }
        else if (LoadInst *load_inst = dyn_cast<LoadInst>(store_addr))
        {

            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Pointer analysis: this StoreInst stores to a address loaded by LoadInst, follow the `addr` of it load from.\n");
            Value *load_addr = load_inst->getOperand(0);

            /// NOTE: `load_addr` is the address where maybe stored an address to (reference to) a caller's variable
            ///        but note that if you print the class of `users_of_load_addr`, you may got a 'User::DerivedUser',
            ///        which is usually caused by instruction like: "a.addr = alloca i32*, align 8"
            vector<User *> users_of_load_addr = getSequenceUsers(load_addr);

            /// NOTE: Rather than cast `users_of_load_addr` to instruction, we need to search in among the users of
            ///       this address to see: if there is a StoreInst ever store something into this address. If yes,
            ///       and this StoreInst store the address (which is also a function arguments) to it, then, the taint
            ///       is out this function. We need to visit callgraph to follow further in the caller.
            for (vector<User *>::iterator I_user = users_of_load_addr.begin(); I_user != users_of_load_addr.end(); I_user++)
            {

                if (StoreInst *sb_store_to_this_addr = dyn_cast<StoreInst>(*I_user))
                {

                    if (comesBefore(sb_store_to_this_addr, load_inst))
                    {
                        Value *tobe_followed_reference = sb_store_to_this_addr->getOperand(0);

                        /// If hits at least one argument. If yes, visit callgraph to follow further in the caller.
                        Function *thisFun = load_inst->getFunction();
                        uint arg_index = 0;
                        for (Function::arg_iterator arg_iter = thisFun->arg_begin(); arg_iter != thisFun->arg_end(); arg_iter++, arg_index++)
                        {

                            if (arg_iter == tobe_followed_reference)
                            {
                                /*
                                 * Get users of the ReturnInst (which is a CallBase)
                                 */
                                vector<pair<CallBase *, Function *> *> callers = getCallerAndCallInst(thisFun);
                                printCallers(callers, level + 1);

                                /*
                                 * For every users (CallBase) of the ReturnInst, trace their correspoding operand (argument).
                                 */
                                for (vector<pair<CallBase *, Function *> *>::iterator i = callers.begin(); i != callers.end(); i++)
                                {
                                    CallBase *caller_inst = (*i)->first;
                                    struct SrcLoc srcloc = getSrcLoc(caller_inst);
                                    struct InstInfo *inst_info_caller = new InstInfo(caller_inst, srcloc);
                                    cur_inst_info->Successors.push_back(inst_info_caller);
                                    inst_info_caller->Predecessor = cur_inst_info;
				    				inst_info_caller->isDirect = cur_inst_info->isDirect;

                                    Value *tainted_tobe_followed = caller_inst->getArgOperand(arg_index);
                                    tainted_tobe_followed->print(llvm::outs());
                                    llvm::outs() << "\n";

                                    handleUser(tainted_tobe_followed, gv_info, inst_info_caller, level + 1);
                                }
                            }
                        }
                    } /// NOTE: NO `else` here, since if not comesBefore, the case can be passed happily.

                } /// NOTE: NO `else` here, since `handleUser(store_addr, gv_info, cur_inst_info, level+1);` will handle normal cases.
            }
        }
        else
        { // May be the class of 'store_addr' is 'User::DerivedUser'. Don't care
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Pointer analysis: this StoreInst stores to a address allocated by alloca (User::DerivedUser), so no need pointer analysis futher.\n");
        }

        /// TODO: fix bug here.
        // Deleted 2022 10 05 forgot why I had written this line
        // Undelete at 2022 10 10. Revert.
        handleUser(store_addr, gv_info, cur_inst_info, level + 1);
    }
    else if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a GetElementPtrInst.\n");

        /*
            If GEP has 2 operands, and cur_value is the gep_base(the offset is often constant),
            we continue following the flow; if cur_value is the offset, we should not follow.

            If GEP has 3 operands, and cur_value is the gep_base, and the second operand is
            constant(often 0), we continue following; if cur_value is the third operand, we
            should not follow.
        */
        // HERE, is the code support TTT.fieldConfig as entry.
        if (cur_inst_info->Predecessor == nullptr)
        {
            if (isMatchedGEPOperator(dyn_cast<GEPOperator>(gep), gv_info, level))
            {
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Found start point of config: " << gv_info->NameInfo->getNameAsString() << "\n");
                visitedStructGVCases.push_back(cur_inst);

                /// NOTE: We found the gep instruction to get structural configuration option, so we trace it.
                handleUser(gep, gv_info, cur_inst_info, level + 1);
            }
        }
        else // prev_inst_info != nullptr
        {
            if (gep->getPointerOperand() == cur_value) // used as gep_base
            {
                handleUser(gep, gv_info, cur_inst_info, level + 1);
            }
            else
            {
                for (auto it = gep->idx_begin(); it != gep->idx_end(); ++it)
                {
                    if ((*it) == cur_value)
                    {
                        cur_inst_info->InfluenceLevel = WEAK;
                        handleUser(gep, gv_info, cur_inst_info, level + 1);
                    }
                }
            }
        }
    }

    /*
        Terminator Instruction:
            IndirectBrInst, BranchInst, ReturnInst, SwitchInst, InvokeInst?, CallBrInst?,
            ResumeInst, CatchSwitchInst, CatchReturnInst, CleanupReturnInst,
    */
    else if (SelectInst *select_inst = dyn_cast<SelectInst>(cur_inst))
    {
        if (select_inst->getCondition() != nullptr &&
            select_inst->getCondition() == cur_value)
        {
            // There may be argue further. Should we follow such case? I think yes. because
            // select is the most simple way to convert configuration variable to other form
            // of variable. 
            //    e.g., open_flag = select, srv_unix_file_flush_method==SRV_UNIX_O_DSYNC? O_SYNC : xxx
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[Control-Flow WARINING] following a selectInst, where the pre-taint is the condition.\n");
            handleUser(select_inst, gv_info, cur_inst_info, level + 1);

            // Former way, deprecated.
            //cur_inst_info->isControllingInst = true;
        }
        else if (select_inst->getTrueValue() != nullptr &&
                     select_inst->getTrueValue() == cur_value ||
                 select_inst->getFalseValue() != nullptr &&
                     select_inst->getFalseValue() == cur_value)
        {
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "following a selectInst, where the pre-taint is the if-true/if-false value (this is data-flow).\n");
            handleUser(select_inst, gv_info, cur_inst_info, level + 1);
        }
    }
    else if (BranchInst *branch = dyn_cast<BranchInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a BranchInst.\n");

        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Num of Successors: " << branch->getNumSuccessors() << "\n");

        if (branch->isUnconditional() || branch->getNumSuccessors() != 2)
        {
            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "It should be conditional Branch.");
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Or it should not have more than 2 Successors.\n");
            return;
        }
        else if (branch->isConditional())
        {
            /// NOTE: collect all the instructions from two branches, and
            ///       then determine if they are tainted.
            BasicBlock *leftBB = branch->getSuccessor(0);
            BasicBlock *rightBB = branch->getSuccessor(1);
            if (leftBB && rightBB && rightBB->getParent() != leftBB->getParent())
            {
                MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[ERROR] parent of the two children are not expected, RETERN.\n");
                return;
            }

            vector<BasicBlock *> childrens = {leftBB, rightBB};
            vector<BasicBlock *> taintedBB;

#undef OLD_JUDGE_TAINT
#ifdef OLD_JUDGE_TAINT
            expandToAllReachableBB(childrens);
#endif
            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "exp.-to-all children size: " << childrens.size() << "\n");

            // HERE, must saitisfy: "leftBB->getParent() == rightBB->getParent()"
            //   the PostDominatorTree is for block-level control dependency
            //   the DominatorTree is for phi(instruction) level control dependency
            DominatorTree *DT = new DominatorTree(*leftBB->getParent());
            PostDominatorTree *PDT = new PostDominatorTree(*leftBB->getParent());

#ifdef OLD_JUDGE_TAINT
            for (vector<BasicBlock *>::iterator iB = childrens.begin(); iB != childrens.end(); iB++)
            {
                /* 
                 *** The core to determine if a BB is tainted -- the "post-dominance" ***
                 *        A block Y is control dependent on block X if and only if 
                 *        Y post dominates at least one but not all successors of X.
                 *                                TO
                 *        A block Y is dependent on block X if and only if 
                 *        Y post dominates not all successors of X. When encounter
                 *        a none-control-dependent node, stop determining its children.
                 * 
                 * MoreINFO: https://stackoverflow.com/questions/72052295                        
                 */

                if (PDT->dominates( &*(*iB)->begin(), &*leftBB->begin()) &&
                    PDT->dominates( &*(*iB)->begin(), &*rightBB->begin()))
                    continue;
                else
                    taintedBB.push_back(*iB);
            }
#endif
#ifndef OLD_JUDGE_TAINT
            calcTaintBBfromBr(&childrens, &childrens, &taintedBB, PDT);
#endif

            if (taintedBB.empty())
            {
                MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[ERROR] VERY Strange case. None-post-dominant-branches should NOT be empty.\n");
                return;
            }
            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "tainted basic-blocks: " << taintedBB.size() << "\n");

            vector<BasicBlock*> PHIedBB;
            calcPHIedBB(leftBB, rightBB, PHIedBB);
            if(PHIedBB.empty()){
                MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[NOTE] no PHIed nodes here, i.e., no auxillary data flow.\n");
            }
            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
#ifdef PHIBB_UNION
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Union ");
#endif
#ifdef PHIBB_INTERSECT
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Intersect ");
#endif
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "PHI-ed basic-blocks: " << PHIedBB.size() << "\n");

            /// currently, we follow phi cases infinitely;  
            /// TODO: add max limitation on the flow length.
            handlePHINodesFromBBs(PHIedBB, 
                                  leftBB, 
                                  rightBB, 
                                  DT, 
                                  level + 1, 
                                  gv_info, 
                                  cur_inst_info);

            /// TODO: iterate cur_inst_info and its Predecessors to find whether there is used Values in taintedBB.
            ///       If not, just record diff_bbs as ControllingBBs in BranchInst as currently done.

            /// currently, we only mark functions in the tainted BB.
            handleControFlowFromBBs(taintedBB, 
                                    gv_info, 
                                    cur_inst_info, 
                                    level + 1);


            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "This Instruction marked as a ControllingInst 2.\n");
            cur_inst_info->isControllingInst = true;
            cur_inst_info->setControllingBBs(taintedBB);
        }
        else
        {
            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "WTF is this?? check me in line " << __LINE__ << ".\n");
        }
    }
    else if (SwitchInst *switch_inst = dyn_cast<SwitchInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a SwitchInst, with cases: " << switch_inst->getNumCases() << ", and successors: " << switch_inst->getNumSuccessors() <<"\n");

        // calculated the set of BB should be tainted.
        vector<BasicBlock*> children, taintedBBs;
        PostDominatorTree * PDT = new PostDominatorTree(*cur_inst->getParent()->getParent());
        for(uint i=0; i<switch_inst->getNumSuccessors(); i++){
            children.push_back(switch_inst->getSuccessor(i));
        }
        calcTaintBBfromBr(&children, &children, &taintedBBs, PDT);
        if (taintedBBs.empty())
        {
            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[ERROR] VERY Strange case. should be at least one element.\n");
            return;
        }
        MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "tainted basic-blocks: " << taintedBBs.size() << "\n");

        // handle those BBs.
        handleControFlowFromBBs(taintedBBs, gv_info, cur_inst_info, level+1);

    }
    else if (isa<FenceInst>(cur_inst) ||
             isa<AtomicCmpXchgInst>(cur_inst) ||
             isa<AtomicRMWInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a FenceInst/AtomicCmpXchgInst/AtomicRMWInst, STOP.\n");
    }
    else if (ResumeInst *resume_inst = dyn_cast<ResumeInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a ResumeInst.\n");
    }
    else if (CatchSwitchInst *catch_switch = dyn_cast<CatchSwitchInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a CatchSwitchInst.\n");
    }
    else if (CatchReturnInst *catch_return = dyn_cast<CatchReturnInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a CatchReturnInst.\n");
    }
    else if (CleanupReturnInst *cleanup_ret = dyn_cast<CleanupReturnInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a CleanupReturnInst, stop tracing.\n");
    }
    else if (ReturnInst *ret_inst = dyn_cast<ReturnInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a ReturnInst, since we reach a ret, we need to go to caller. Caller-CallInst pairs of [" << getOriginalName(ret_inst->getFunction()->getName().str()) << "]:\n");

        /*
         * Get users of the ReturnInst (which is a CallBase)
         */
        clock_t startTime, endTime;
        startTime = clock();
        Function *thisfunc = ret_inst->getFunction();
        vector<pair<CallBase *, Function *> *> callers = getCallerAndCallInst(thisfunc);
        endTime = clock();
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "traversed callgraph for [" << (endTime - startTime) / CLOCKS_PER_SEC << "] secs." << "\n";);
        printCallers(callers, level + 1);

        /*
         * For every users (CallBase) of the ReturnInst, trace them.
         */
        for (vector<pair<CallBase *, Function *> *>::iterator i = callers.begin(); i != callers.end(); i++)
        {
            CallBase *caller_inst = (*i)->first;
            struct SrcLoc srcloc = getSrcLoc(caller_inst);
            struct InstInfo *inst_info_caller = new InstInfo(caller_inst, srcloc);
            cur_inst_info->Successors.push_back(inst_info_caller);
            inst_info_caller->Predecessor = cur_inst_info;
	    	inst_info_caller->isDirect = cur_inst_info->isDirect;

            handleUser(caller_inst, gv_info, inst_info_caller, level + 1);
        }
    }
    else if (                        // UnaryInstruction
        isa<AllocaInst>(cur_inst) || // generally don't need consider.
        isa<CastInst>(cur_inst) ||
        isa<ExtractValueInst>(cur_inst) ||
        isa<FreezeInst>(cur_inst) ||
        isa<LoadInst>(cur_inst) ||
        isa<UnaryOperator>(cur_inst) ||
        // isa<VAArgInst>(cur_inst) || // not usable at present.
        // BinaryInstruction
        isa<BinaryOperator>(cur_inst) ||
        // multi-operand Instruction
        isa<CmpInst>(cur_inst) ||
        isa<InsertValueInst>(cur_inst) ||
        isa<PHINode>(cur_inst) ||
        isa<FreezeInst>(cur_inst) ||
        isa<ShuffleVectorInst>(cur_inst) ||
        isa<ExtractElementInst>(cur_inst) ||
        isa<InsertElementInst>(cur_inst) ||
        // FuncletPadInst includes CatchPadInst, CleanupPadInst
        isa<FuncletPadInst>(cur_inst) ||
        isa<LandingPadInst>(cur_inst))
    {
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a " << cur_inst->getOpcodeName() << " Instruction.\n");
        // go further
        handleUser(cur_inst, gv_info, cur_inst_info, level + 1);
    }

    else
    {
        MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
        MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Unhandled Instruction, Opcode: " << cur_inst->getOpcodeName() << "\n");

        // even don't know what is it, just keep tracing
        handleUser(cur_inst, gv_info, cur_inst_info, level + 1);
    }
}

bool reachTargetFunction(Function *base, Function *target, vector<Function *> &visitedFuncs)
{
    /*
    CallGraph CG = CallGraph(*M);

    for (auto IT = df_begin(&CG), EI = df_end(&CG); IT != EI; IT++) {
        if (Function *F = IT->getFunction()) {
            llvm::outs() << "Visiting function: " << F->getName() << "\n";
        }
    }
    */
    return true;
}

bool reachTargetBasicBlock(BasicBlock *base, BasicBlock *target, vector<BasicBlock *> &visitedBB)
{
    for (BasicBlock *succ : successors(base))
    {
        // if `succ` not in `visitedBB`, continue to next `succ` (because if in, `succ` must has been explored)
        if (std::find(visitedBB.begin(), visitedBB.end(), succ) != visitedBB.end())
            continue;
        // there exist a path from base to target.
        if (succ == target)
            return true;
        visitedBB.push_back(succ);
        return reachTargetBasicBlock(succ, target, visitedBB);
    }
    return false;
}

/*
    Input: two Instruction pointer;
    Output: return true if tar is before base (in the same BasicBlock).
*/
bool comesBefore(Instruction *tar, Instruction *base)
{
    if (!base)
        return false;

    if (base == tar)
        return false;

    /*
     *  `tar` and `base` are not in the same basic block but in a same function.
     *     - we need to know if there is a path between the two functions.
     */
    if (base->getParent()->getParent() != tar->getParent()->getParent())
    {
        vector<Function *> visitedFuncs;
        visitedFuncs.clear();
        // condition is True if there is a *forward* path from `base`'s function to `tar`'s function
        if (reachTargetFunction(base->getParent()->getParent(), tar->getParent()->getParent(), visitedFuncs))
        {
            // in other words, there is not a *forward* path from `tar` to `base`, so return false.
            return false;
        }
        else
        {
            return true;
        }
    }

    /*
     *  `tar` and `base` are not in the same basic block but in a same function.
     *     - we need to know if there is a path between the two basic blocks.
     */
    if (base->getParent() != tar->getParent() &&
        base->getParent()->getParent() == tar->getParent()->getParent())
    {
        vector<BasicBlock *> visitedBB;
        visitedBB.clear();
        // condition is True if there is a *forward* path from `base`'s basic-block to `tar`'s basic-block
        if (reachTargetBasicBlock(base->getParent(), tar->getParent(), visitedBB))
        {
            // in other words, there is not a *forward* path from `tar` to `base`, so return false.
            return false;
        }
        else
        {
            return true;
        }
    }

    /*
     *  `tar` and `base` are in the same basic block.
     *      so we start from entry inst to the last inst,
     *      return true if we meet `tar` first,
     *      return false if we meet `base` first.
     */
    bool flag_meet = false;
    BasicBlock *bb = tar->getParent();
    for (BasicBlock::iterator i = bb->begin(); i != bb->end(); i++)
    {
        Instruction *inst = &*i;
        if (inst == base) // we meet `base` first, the next `if` will fail later (because flag_meet = true), and finally return false
            flag_meet = true;
        if (inst == tar && flag_meet == false) // we meet `tar` first, return true
            return true;
    }
    return false;
}

std::vector<User *> getSequenceUsers(Value *cur_value)
{
    vector<User *> UserVec;
    UserVec.clear();
    for (Value::user_iterator i = cur_value->user_begin(), e = cur_value->user_end(); i != e; i++)
    {
        /// TAG: Records of GVs' Users.
        // Instruction: load, store, phi, call, invoke
        // Constant: ConstantExpr, ConstantStruct
        User *cur_user = *i;
        if (cur_user == cur_value)
            continue;
        UserVec.push_back(cur_user);
    }
    return UserVec;
}

vector<Use *> getSequenceUses_OnlyGEPins(Value *cur_value)
{
    llvm::outs() << "[Uses of] ";
    cur_value->print(llvm::outs());
    llvm::outs() << "are: \n";
    vector<Use *> UseVec;
    UseVec.clear();
    for (Value::use_iterator i = cur_value->use_begin(), e = cur_value->use_end(); i != e; i++)
    {
        if (GetElementPtrInst *gep_ins = dyn_cast<GetElementPtrInst>(*i))
        {
            MY_DEBUG(_DEBUG_LEVEL, gep_ins->print(llvm::outs()));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n");
        }
        else
        {
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "NOT gep_ins, but " << getClassType(*i) << "\n");
        }
    }
    return UseVec;
}

void printSequenceUsers(Value *cur_value)
{
    llvm::outs() << "[Users of] ";
    cur_value->print(llvm::outs());
    llvm::outs() << "are: \n";
    for (Value::user_iterator i = cur_value->user_begin(), e = cur_value->user_end(); i != e; i++)
    {
        User *cur_user = *i;
        i->print(llvm::outs());
        llvm::outs() << "\n";
    }
    return;
}

bool findVisitedInstruction(struct InstInfo *inst_info, struct InstInfo *prev_inst_info)
{
    /// if any of the two is nullptr, return true?
    if (!inst_info || !prev_inst_info)
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "inst_info is nullptr. Check me at line: " << __LINE__ << "\n");
        return false;
    }

    if (inst_info->InstPtr == prev_inst_info->InstPtr)
        return true;

    unsigned cnt = 1;
    struct InstInfo *temp = prev_inst_info->Predecessor;
    while (temp)
    {
        if (inst_info->InstPtr == temp->InstPtr)
        {
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Find in reverse the previous Inst node " << cnt << "\n");
            MY_DEBUG(_ERROR_LEVEL, temp->InstPtr->print(llvm::outs()));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "\n");
            return true;
        }
        temp = temp->Predecessor;
        cnt++;
    }
    return false;
}

bool isMatchedGEPOperator(GEPOperator *gepo, struct GlobalVariableInfo *gv_info, int level) {

    /// NOTE: if this is the start point of structural GVs' gep instruction, check and follow.
    if (gv_info->NameInfo->VarType == STRUCT &&
        (getStructTypeStrFromPrintAPI(gepo->getPointerOperandType()) == gv_info->GlobalVariableType ||                                       // directly match base with GVs.
         std::find(verifiedStructTypeList.begin(), verifiedStructTypeList.end(), gepo->getPointerOperand()) != verifiedStructTypeList.end()) // previously verified bitcast type same to %struct.redisServer.
    )
    {
        vector<unsigned> idx_vec = getIndexFromGEPO(gepo);
        if (idx_vec.empty())
        {
            MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Unexpected Situation: Error in get Indexs in GEPOperator.\n");
            return false;
        }
        
        // Line 1
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Found offsets are: [ ");
        for (unsigned c = 0; c < idx_vec.size(); c++)
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << idx_vec[c] << " ");
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "]\n");

        // Line 2
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Expected offsets are:  ");
        MY_DEBUG(_DEBUG_LEVEL, gv_info->printNameInfo(0));

        if (idx_vec.size() == gv_info->Offsets.size() + 1)
        {
            bool idx_matched = true;
            for (unsigned i = 0; i < gv_info->Offsets.size(); i++)
            {
                if (idx_vec[i + 1] != gv_info->Offsets[i])
                {
                    idx_matched = false;
                    break;
                }
            }
            if (idx_matched == false)
                return false;
            return true;
        }
    }
    return false;
}

/** The call in tainter.cpp:
 **     handleUser(gv, gv_info_list[gv_cnt], nullptr, 0);
 **         gv  ~~  GlobalVariable*
 **/
void handleUser(Value *cur_value,
                struct GlobalVariableInfo *gv_info,
                struct InstInfo *prev_inst_info,
                unsigned level) {
    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[== " << __func__ << " ==]\n");

    if (!cur_value && level>0)
        return;
    else if (!cur_value && level==0){

        /*
        * ========== FIELD case ==========
        * Find same type with same offset but not identical gep_inst
        * This step is NOT accurate with an assumption that "same type with same offset" is for conf only (if yes).
        *
        *    1. iterate over all get_inst. find the matched one.
        */
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Entry point of non-global pointer parameter.\n");
        MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
        MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[NOTE] Finding GetElementPtrInst with same type, same offset, but not identical one.\n");
        Value *matched_ins = nullptr;
        vector<int> indices;
        vector<Value *> matched_inss;
        indices.push_back(0);
        indices.push_back(gv_info->Offsets[0]);
        matched_inss = FetchValue4FurtherFollow2(gv_info->NameInfo->getNameAsStringPrefix(), &indices);
        if(matched_inss.size()==0){
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[WARNING] Not found use of "<< gv_info->NameInfo->getNameAsString() <<". Check *-parameter.txt file\n");
            return;
        }
        for(auto matched_ins:matched_inss)
        {
            if (matched_ins == nullptr)
            {
                MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[ERROR] current matched_ins is null. Next matched_ins.\n");
                continue;
            }
            else
            {
                /*
                *    2. add matched_ins_info to list, then follow the matched_ins_info recursively.
                */
                if (Instruction *matched_instruction = dyn_cast<Instruction>(matched_ins))
                {
                    /*
                    *    2.1.  make this matched_ins into a matched_ins_info
                    */
                    struct SrcLoc srcloc = getSrcLoc(matched_instruction);
                    struct InstInfo *inst_info_matched_ins = new InstInfo(matched_instruction, srcloc, false);
                    /*
                    *    2.2.  follow the matched_ins_info recursively.
                    */
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[FOUND] Follow this GetElementPtrInst recursively.\n");
                    handleUser(matched_instruction, gv_info, nullptr, level + 1);
                }
                else
                {
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "[WRONG] check me at line " << __LINE__ << "\n");
                }
            }
        }
        return;
    }
    // ========== FIELD case DONE ==========

    /**
     ** NOTE: If goto a global variable, we treat that gv's influencial zone is included by current gv.
     **       So we record it, and stop visiting its Users.
     **/
    if (gv_info->Ptr && cur_value != gv_info->Ptr && isa<GlobalVariable>(cur_value))
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Come to Global Variable: " << cur_value->getName() << ", stop\n");
        gv_info->InfluencedGVList.push_back(dyn_cast<GlobalVariable>(cur_value));
        //return;
    }

    /**
     * @brief for(Value::user_iterator i=cur_value->user_begin(), e=cur_value->user_end(); i!=e; i++)
     *            UserVec.push_back(*i);
     */
    vector<User *> UserVec = getSequenceUsers(cur_value);
    unsigned user_ite_cnt = 0;

    /**
     * @brief for each user of this global variable
     */
    for(auto i = UserVec.rbegin(), e = UserVec.rend(); i != e; i++)
    {
        User *cur_user = *i;
        user_ite_cnt++;

        /// If cur_user is a struct GV User, and matched with previous GVs, we don't waste time to handle it once more.
        if (std::find(visitedStructGVCases.begin(), visitedStructGVCases.end(), cur_user) != visitedStructGVCases.end())
            continue;

        if (level == 0)
        {
            visitedLoadStoreInstList.clear();
            MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "\nThe " << user_ite_cnt << " th New Direct User tracing:\n");
        }
        MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
        MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Current User: level " << level << "\n");
        MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
        MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "  ↳");
        MY_DEBUG(_WARNING_LEVEL, cur_user->print(llvm::outs()));
        if (isa<Instruction>(cur_user))
        {
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "\t");
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << getSrcLoc(dyn_cast<Instruction>(cur_user)).toString() << "\n");
        }
        else
        {
            MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "\n");
        }

        if (Instruction *cur_inst = dyn_cast<Instruction>(cur_user))
        {
            struct SrcLoc srcloc = getSrcLoc(cur_inst);
            struct InstInfo *inst_info = new InstInfo(cur_inst, srcloc);

            if (prev_inst_info && cur_inst == prev_inst_info->InstPtr)
            {
                MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "`Previous ins` == `Current ins`. Mostly because tracing user of *addr* of a storeIns, or a tainted argument of a call\n");
                continue;
            }

            /**
             ** CONTINUE: If the first User is a StoreInst targeting GV,
             **           it should be an assignment, we don't follow it.
             **/
            if (gv_info->Ptr &&                                                               // if null, the "FIELD" case
                !prev_inst_info &&                                                            // if is the first User
                isa<StoreInst>(inst_info->InstPtr) &&                                         // if is a StoreInst
                dyn_cast<StoreInst>(inst_info->InstPtr)->getPointerOperand() == gv_info->Ptr) // if store to GV
            {
                MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Don't trace target GV Assignment.\n");
                continue;
            }

            /**
             ** CONTINUE: If current instruction is before the previous one, we should not record.
             **           member function comesBefore() of Instruction doesn't exist in llvm-10.0.0,
             **           so we manually implement one.
             **           TODO: Cache the sequence of instructions to accelerate?
             **/
            if (prev_inst_info && comesBefore(inst_info->InstPtr, prev_inst_info->InstPtr))
            {
                MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "\"");
                MY_DEBUG(_WARNING_LEVEL, inst_info->InstPtr->print(llvm::outs()));
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "\" comes before \"");
                MY_DEBUG(_WARNING_LEVEL, prev_inst_info->InstPtr->print(llvm::outs()));
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "\" So, don't trace earlier instruction\n");
                continue;
            }

            /**
             **  CONTINUE: If the user is a continous StoreInst with same StoreAddr, we don't follow it.
             **            continuous same StoreInsts is included in this situation.
             **/
            if (prev_inst_info &&
                isa<StoreInst>(prev_inst_info->InstPtr) &&
                isa<StoreInst>(inst_info->InstPtr) &&
                dyn_cast<StoreInst>(prev_inst_info->InstPtr)->getPointerOperand() ==
                    dyn_cast<StoreInst>(inst_info->InstPtr)->getPointerOperand())
            {
                if (prev_inst_info->InstPtr == inst_info->InstPtr)
                    MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "VERY WRONG. prev_inst_info->InstPtr == inst_info->InstPtr ");
                MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "This is a WRONG branch! \n");
            }

            /**
             **  CONTINUE: the Load/Store Inst is visited
             **/
            if (isa<StoreInst>(inst_info->InstPtr) || isa<LoadInst>(inst_info->InstPtr))
            {
                if (std::find(visitedLoadStoreInstList.begin(), visitedLoadStoreInstList.end(), inst_info->InstPtr) != visitedLoadStoreInstList.end())
                {
                    MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Don't trace visited Load/Store Inst.\n");
                    continue;
                }
            }

            /// NOTE: record visitedLSInst, since they are only one input and one output.
            visitedLoadStoreInstList.push_back(inst_info->InstPtr);

            /// NOTE: After filter exceptional situation, we begin to record information.
            if (prev_inst_info == nullptr) // means the first User of current GV.
            {
                gv_info->currentGVStartingFunc = inst_info->InstPtr->getFunction();
                gv_info->currentGVStartingFuncName = getOriginalName(inst_info->InstPtr->getFunction()->getName());
                gv_info->InstInfoList.push_back(inst_info);
                inst_info->Predecessor = nullptr;
            }
            else
            { // `(Instruction*) cur_inst` is a `dyn_cast<Instruction>(cur_user)`
                if (findVisitedInstruction(inst_info, prev_inst_info))
                    continue;
                if (prev_inst_info->InfluenceLevel == WEAK)
                    inst_info->InfluenceLevel = WEAK;
                /**
                 ** like a double-way linked tree
                 **
                 **          _`prev_inst_info`
                 **          /|    |      /|\
                 **         /     \|/      |
                 ** Predecessor Successors  Predecessor
                 **        /|\  [1] ... [n]    /|\
                 **         |      //   \\      |
                 **        `xxxxxxx`   `inst_info`
                 **/
                prev_inst_info->Successors.push_back(inst_info);
                inst_info->Predecessor = prev_inst_info;
				inst_info->isDirect = prev_inst_info->isDirect;
            }

            handleInstruction(cur_value, gv_info, inst_info, level);
        }

        else if (GEPOperator *gepo = dyn_cast<GEPOperator>(cur_user))
        {
            /// NOTE: handle GEPOperator situations.
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a GEPOperator instance.\n");

            /// NOTE: A judgement for our handling logic, if there is any Unexpected situations here, we might need to adjust the handling logic on GEPOperator.
            unsigned gepo_op_index = 0;
            for (auto it = gepo->idx_begin(), et = gepo->idx_end(); it != et; ++it)
            {
                gepo_op_index++;
                if (!isa<Constant>(*it))
                {
                    MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[WARNING] Unexpected Situation: The " << gepo_op_index << " th Index in GEPOperator is not Constant.\n");
                }
            }

            /// NOTE: if this is the start point of structural GVs' gep instruction, check and follow.
            if (prev_inst_info == nullptr) // might be the first Instruction to access structural GVs.
            {
                if (isMatchedGEPOperator(gepo, gv_info, level))
                {
                    MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Found start point of config: " << gv_info->NameInfo->getNameAsString() << "\n");
                    visitedStructGVCases.push_back(cur_user);

                    /// NOTE: We found the gep instruction to get structural configuration option, so we trace it.
                    handleUser(gepo, gv_info, prev_inst_info, level + 1);
                }
                else
                {
                    MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                    MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "This is not a start point. [OK, PASS]\n");
                }
            }
            else // prev_inst_info != nullptr
            {
                MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[ERROR] Meet GEPOperator while prev_inst_info != nullptr\n");

                if (gepo->getPointerOperand() == cur_value)
                {
                    handleUser(gepo, gv_info, prev_inst_info, level + 1);
                }
            }
        }

        else if (BitCastOperator *bcop = dyn_cast<BitCastOperator>(cur_user))
        {
            MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "It is a BitCastOperator.\n");

            if (prev_inst_info == nullptr)
            {
                string src_type_str = getStructTypeStrFromPrintAPI(bcop->getSrcTy());
                string dest_type_str = getStructTypeStrFromPrintAPI(bcop->getDestTy());
                MY_DEBUG(_ERROR_LEVEL, printTabs(level + 1));
                MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Src Type: " << src_type_str << "\t Dest Type: " << dest_type_str << "\n");

                if (gv_info->NameInfo->VarType == STRUCT &&
                    gv_info->GlobalVariableType == src_type_str)
                {
                    size_t pos = dest_type_str.find(src_type_str);
                    if (pos != std::string::npos)
                    {
                        verifiedStructTypeList.push_back(bcop); // record these verified type cast about %struct.redisServer in Redis.
                        unsigned type_offset = std::stoul(dest_type_str.substr(pos + src_type_str.length() + 1, dest_type_str.length()));
                        MY_DEBUG(_WARNING_LEVEL, printTabs(level + 1));
                        MY_DEBUG(_WARNING_LEVEL, llvm::outs() << "Current Type Offset : " << type_offset << "\n");
                        handleUser(bcop, gv_info, prev_inst_info, level + 1);
                    }
                }
            }
            else // prev_inst_info != nullptr
            {
                MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "[ERROR] Meet BitCastOperator while prev_inst_info != nullptr\n");
            }
        }

        else if (ConstantExpr *constant_expr = dyn_cast<ConstantExpr>(cur_user))
        {
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a ConstantExpr.\n");
        }
        else if (isa<ConstantAggregate>(cur_user))
        {
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a ConstantAggregate.\n");
        }
        else if (isa<ConstantStruct>(cur_user))
        {
            MY_DEBUG(_DEBUG_LEVEL, printTabs(level + 1));
            MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "It is a ConstantStruct.\n");
        }
        else
        {
            MY_DEBUG(
                _DEBUG_LEVEL,
                printTabs(level + 1);
                llvm::outs() << "[WARNING] Unhandled User Situation in handleUser:\n";
                printTabs(level + 2);
                cur_user->print(llvm::outs());
                llvm::outs() << "\n";
                printTabs(level + 2);
                cur_user->getType()->print(llvm::outs());
                llvm::outs() << "\n";
                string class_type = getClassType(cur_user);
                llvm::outs() << "This is a " << class_type << " Class\n\n";

                if (isa<ConstantExpr>(cur_user)) {
                    llvm::outs() << "Yes, it is!!!!\n\n";
                })
        }
    }
}

int main2(int argc, char **argv)
{
    if (argc < 3)
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Expected two arguments:\n\tFirst argument - IR file name,\n\tSecond argument - Config variable file.\n");
        return -1;
    }

    string ir_file = string(argv[1]);
    string var_file = string(argv[2]);

    if (ir_file.substr(ir_file.length() - 3, 3) != ".bc" &&
        ir_file.substr(ir_file.length() - 3, 3) != ".ll")
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "First argument format WRONG!\n\tExpect for an IR file: .bc or .ll!\n");
        return -1;
    }

    /*********************************
     **
     ** Initialize LLVM IR related.
     **
     *********************************/
    llvm::outs() << "---------------------------------------------------------------------\n";
    llvm::outs() << "Parsing IR file... \n";
    clock_t startTime = clock();

    LLVMContext context;
    SMDiagnostic Err;
    std::unique_ptr<llvm::Module> module = parseIRFile(ir_file.c_str(), Err, context);
    M = module.get();
    if (!M)
    {
        MY_DEBUG(_ERROR_LEVEL, Err.print(argv[0], errs()));
        exit(1);
    }
    clock_t endTime = clock();
    llvm::outs() << "Parsing DONE. [" << (endTime - startTime) / CLOCKS_PER_SEC << "] secs\n    This time is mandatary (LLVM library induced), non-optimizable. "
                 << "\n";
    llvm::outs() << "---------------------------------------------------------------------\n";

    /********************************************************
     **
     ** Get all IR instructions and record some GetElementPtrInsts.
     **
     ********************************************************/
    llvm::outs() << "Recording all GEPInstructions... \n";
    startTime = clock();
    int recorded = 0, ignored = 0, cnt = 0;
    bool next = false;
    uint inst_count = M->getInstructionCount();
    // for every function in the module.
    for (Module::iterator F = M->begin(), F_End = M->end(); F != F_End; ++F)
    {

        // true if the function's name starts with "llvm.".
        if (F->isIntrinsic())
            continue;

        // for every basic block in the function.
        for (Function::iterator BB = F->begin(), BEnd = F->end(); BB != BEnd; ++BB)
        {

            // for every instruction in the basic block
            for (BasicBlock::iterator inst = BB->begin(); inst != BB->end(); inst++)
            {

                // First, ignore this function if the first inst is from library.
                struct SrcLoc srcloc = getSrcLoc(&*inst);
                if (srcloc.filenameHasString("include/c++") || srcloc.dirHasString("include/c++"))
                {
                    next = true;
                    break;
                }

                if (++cnt % 1000 == 0)
                {
                    printf("\033[K%u/(about %u)\r", cnt, inst_count);
                    fflush(stdout);
                }

                if (GetElementPtrInst *gep_inst = dyn_cast<GetElementPtrInst>(inst))
                {

                    if (!gep_inst->hasIndices() ||
                        gep_inst->getNumOperands() < 3 ||
                        !gep_inst->hasAllConstantIndices())
                    {
                        ignored++;
                        continue;
                    }

                    int num = gep_inst->getNumOperands();

                    /// operand 0 (class/struct type)
                    llvm::Type *type = gep_inst->getOperand(0)->getType();

                    /// operand 1-n (all constant operand)
                    vector<int> indices;
                    bool ignore = false;
                    for (int i = 1; i < num; i++)
                    {
                        if (ConstantInt *second_offset = dyn_cast<ConstantInt>(gep_inst->getOperand(i)))
                        {
                            indices.push_back(second_offset->getSExtValue());
                        }
                        else
                        {
                            ignore = true;
                        }
                    }
                    // some operand is not constant int, but is int, I don't know why.
                    if (ignore)
                    {
                        // MY_DEBUG( _DEBUG_LEVEL, llvm::outs() <<"[NOTE] Ignore this GetElementPtrInst\n");
                        // MY_DEBUG( _DEBUG_LEVEL, gep_inst->print(llvm::outs()));
                        // MY_DEBUG( _DEBUG_LEVEL, llvm::outs() <<"\n");
                        // MY_DEBUG( _DEBUG_LEVEL, srcloc.print(1));
                        ignored++;
                        continue;
                    }

                    // insert to GEPTypeOffsetInstList (unique, no dup)
                    pair<Type *, vector<int>> p(type, indices);
                    pair<pair<Type *, vector<int>>, Value *> pp(p, gep_inst);
                    bool push_back = true;

                    if (push_back)
                    {
                        GEPTypeOffsetInstList.push_back(pp);
                        recorded++;
                    }
                }
            }
            if (next)
            {
                next = false;
                cnt += F->getInstructionCount();
                break;
            }
        }
    }
    endTime = clock();
    llvm::outs() << "Recording DONE. [" << (endTime - startTime) / CLOCKS_PER_SEC << "] secs"
                 << "\n";

    if(recorded != GEPTypeOffsetInstList.size())
    {
        llvm::outs() << "[ERROR] recorded != GEPTypeOffsetInstList.size(). Exit. check at line: " << __LINE__ <<"\n";
        exit(1);
    }
    llvm::outs() << "-   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -\n";
    llvm::outs() << "Record " << recorded << " gep_ins,  Ignore " << ignored << " unhandled gep_ins.\n";
    llvm::outs() << "---------------------------------------------------------------------\n";

    /*********************************
     **
     ** Read name of CONFs from file to
     **   config_names  ~~  vector< ConfigVariableNameInfo* >
     **
     *********************************/
    std::vector<struct ConfigVariableNameInfo *> config_names;
    config_names.clear();
    if (!readConfigVariableNames(var_file, config_names))
        exit(1);

    /*********************************
     **
     ** Search from
     **     M->global_begin() to M->global_end()
     ** to find if
     **     config_names  ~~  vector< ConfigVariableNameInfo* >
     ** exist in the LLVM IR. If yes, STORE it in
     **     gv_info_list  ~~  vector< GlobalVariableInfo* >
     **
     *********************************/
    int gv_cnt = 0;
    std::vector<struct GlobalVariableInfo *> gv_info_list;
    gv_info_list.clear();
    for (auto I = M->global_begin(), E = M->global_end(); I != E; ++I)
    {
        GlobalVariable *gv = &*I;

        if (!findConfigVariable(config_names, gv, gv_info_list))
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Not a Config Variable\n");
            continue;
        }
        else
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "It is indeed a Config Variable\n");
        }
    }
    /// ======= FIELD ========
    //llvm::outs() << gv_info_list.size() <<" -----------------------------\n\n";
    for(uint config_names_idx=0; config_names_idx<config_names.size(); config_names_idx++){
        if(config_names[config_names_idx]->VarType != FIELD)
            continue;
        vector<uint> offsets;
        offsets.push_back(std::atoi(config_names[config_names_idx]->FieldName[1].c_str()));
        struct GlobalVariableInfo *gv_info = new GlobalVariableInfo(config_names[config_names_idx], nullptr, offsets);
        gv_info_list.push_back(gv_info);
    }
    /// =====================


    /// NOTE: Record config variables found in dbg info.
    string var_found_path = ir_file.substr(0, ir_file.find_last_of(".")) + "-found.txt";
    fstream var_found_f(var_found_path.c_str(), fstream::out | ios_base::trunc);
    if (!var_found_f)
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "open " << var_found_path << " FAILED!\n");
        return -1;
    }
    for (auto i = gv_info_list.begin(), e = gv_info_list.end(); i != e; i++)
    {
        struct GlobalVariableInfo *gv_info = *i;
        var_found_f << gv_info->NameInfo->getNameAsString() << "\n";
    }
    var_found_f.close();
    /// NOTE: Print to llvm::outs
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n\n\n**********************************************************\n\n\n\n");
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Found " << gv_info_list.size() << " GlobalVariables in total.\n");
    MY_DEBUG(
        _DEBUG_LEVEL,
        for (auto i = gv_info_list.begin(), e = gv_info_list.end(); i != e; i++) {
            struct GlobalVariableInfo *gv_info = *i;
            gv_info->printNameInfo(1);
            if(gv_info->Ptr){
                llvm::outs() << "                      ";
                gv_info->Ptr->print(llvm::outs());
                llvm::outs() << "\n";
            }else{
                llvm::outs() << "\t\tThis is FIELD type. Not a global variable.\n";
            }
            if (gv_info->NameInfo->VarType != SINGLE && gv_info->NameInfo->VarType != FIELD)
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "GV Type: " << gv_info->GlobalVariableType << "\n";);
        })
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n\n\n**********************************************************\n\n\n\n");

    /*******************************************************************
     **
     ** NOTE: Now, trace the usage of GlobalVariables related to configs.
     **
     *******************************************************************/
    startTime = clock();
    visitedStructGVCases.clear();
    for (unsigned gv_cnt = 0; gv_cnt < gv_info_list.size(); gv_cnt++)
    {

        //  gv_info_list    ~~  std::vector<GlobalVariableInfo*>
        struct GlobalVariableInfo *gv_info = gv_info_list[gv_cnt];
        GlobalVariable *gv = gv_info_list[gv_cnt]->Ptr;

        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "\n\n\n\n========= Progress: ["<< gv_cnt+1 << "/" << gv_info_list.size() <<"] ========\n\n");
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Current Analyzing GV is : ");
        MY_DEBUG(_ERROR_LEVEL, gv_info_list[gv_cnt]->NameInfo->print(0));

        /*============================================
         * This is the very core function of this tool.
          ============================================*/
        handleUser(gv, gv_info, nullptr, 0);
    }

    /// NOTE: Record information to 'records.dat' and DEBUG INFO.
    endTime = clock();
    llvm::outs() << "\nTainting DONE. [" << (endTime - startTime) / CLOCKS_PER_SEC << "] secs"
                 << "\n";
    llvm::outs() << "---------------------------------------------------------------------\n";
    string output_file = ir_file.substr(0, ir_file.find_last_of(".")) + "-records.dat";
    fstream existing_f(output_file.c_str(), fstream::out | ios_base::trunc);
    if (!existing_f)
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Open output file " << output_file << " FAILED!\n");
        return -1;
    }
    existing_f.close();
    for (auto i = gv_info_list.begin(), e = gv_info_list.end(); i != e; i++)
    {
        struct GlobalVariableInfo *gv_info = *i;
        if (!gv_info->writeToFile(output_file))
        {
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Write to output file " << output_file << " FAILED!\n");
            break;
        }
    }

    /*******************************************************************
     **
     ** NOTE: Ouput key information to 'keyinfo.txt'
     **
     *******************************************************************/
    string keyinfo_file = ir_file.substr(0, ir_file.find_last_of(".")) + "-keyinfo.txt";
    fstream keyinfo_f(keyinfo_file.c_str(), fstream::out | ios_base::trunc);
    if (!keyinfo_f)
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Open statistics file " << keyinfo_file << " FAILED!\n");
        return -1;
    }
    keyinfo_f.close();
    for (auto i = gv_info_list.begin(); i != gv_info_list.end(); i++)
    {
        struct GlobalVariableInfo *gv_info = *i;
        if (!gv_info->writeKeyInfo(keyinfo_file))
        {
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Write to statistics file " << keyinfo_file << " FAILED!\n");
            MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Current GlobalVariable is " << gv_info->NameInfo->getNameAsString() << "\n");
            break;
        }
    }
    return 0;
}

// provide api for scalablity
int buildModule(std::unique_ptr<llvm::Module> &module, LLVMContext &context, SMDiagnostic &Err, string ir_file)
{
    if (ir_file.substr(ir_file.length() - 3, 3) != ".bc" &&
        ir_file.substr(ir_file.length() - 3, 3) != ".ll")
    {
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "First argument format WRONG!\n\tExpect for an IR file: .bc or .ll!\n");
        return -1;
    }

    /*********************************
     **
     ** Initialize LLVM IR related.
     **
     *********************************/
    llvm::outs() << "---------------------------------------------------------------------\n";
    llvm::outs() << "Parsing IR file... \n";
    clock_t startTime = clock();

    //LLVMContext context;
    //SMDiagnostic Err;
    module = parseIRFile(ir_file.c_str(), Err, context);
    M = module.get();
    if (!M)
    {
		MY_DEBUG(_ERROR_LEVEL, llvm::outs() <<"Parese module error!\n");
        //MY_DEBUG(_ERROR_LEVEL, Err.print(errs()));
        exit(1);
    }
    clock_t endTime = clock();
    llvm::outs() << "Parsing DONE. [" << (endTime - startTime) / CLOCKS_PER_SEC << "] secs\n    This time is mandatary (LLVM library induced), non-optimizable. "
                 << "\n";
    llvm::outs() << "---------------------------------------------------------------------\n";

    /********************************************************
     **
     ** Get all IR instructions and record some GetElementPtrInsts.
     **
     ********************************************************/
    llvm::outs() << "Recording all GEPInstructions... \n";
    startTime = clock();
    int recorded = 0, ignored = 0, cnt = 0;
    bool next = false;
    uint inst_count = M->getInstructionCount();
    // for every function in the module.
    for (Module::iterator F = M->begin(), F_End = M->end(); F != F_End; ++F)
    {

        // true if the function's name starts with "llvm.".
        if (F->isIntrinsic())
            continue;

        // for every basic block in the function.
        for (Function::iterator BB = F->begin(), BEnd = F->end(); BB != BEnd; ++BB)
        {

            // for every instruction in the basic block
            for (BasicBlock::iterator inst = BB->begin(); inst != BB->end(); inst++)
            {

                // First, ignore this function if the first inst is from library.
                struct SrcLoc srcloc = getSrcLoc(&*inst);
                if (srcloc.filenameHasString("include/c++") || srcloc.dirHasString("include/c++"))
                {
                    next = true;
                    break;
                }

                if (++cnt % 1000 == 0)
                {
                    printf("\033[K%u/(about %u)\r", cnt, inst_count);
                    fflush(stdout);
                }

                if (GetElementPtrInst *gep_inst = dyn_cast<GetElementPtrInst>(inst))
                {

                    if (!gep_inst->hasIndices() ||
                        gep_inst->getNumOperands() < 3 ||
                        !gep_inst->hasAllConstantIndices())
                    {
                        ignored++;
                        continue;
                    }

                    int num = gep_inst->getNumOperands();

                    /// operand 0 (class/struct type)
                    llvm::Type *type = gep_inst->getOperand(0)->getType();

                    /// operand 1-n (all constant operand)
                    vector<int> indices;
                    bool ignore = false;
                    for (int i = 1; i < num; i++)
                    {
                        if (ConstantInt *second_offset = dyn_cast<ConstantInt>(gep_inst->getOperand(i)))
                        {
                            indices.push_back(second_offset->getSExtValue());
                        }
                        else
                        {
                            ignore = true;
                        }
                    }
                    // some operand is not constant int, but is int, I don't know why.
                    if (ignore)
                    {
                        // MY_DEBUG( _DEBUG_LEVEL, llvm::outs() <<"[NOTE] Ignore this GetElementPtrInst\n");
                        // MY_DEBUG( _DEBUG_LEVEL, gep_inst->print(llvm::outs()));
                        // MY_DEBUG( _DEBUG_LEVEL, llvm::outs() <<"\n");
                        // MY_DEBUG( _DEBUG_LEVEL, srcloc.print(1));
                        ignored++;
                        continue;
                    }

                    // insert to GEPTypeOffsetInstList (unique, no dup)
                    pair<Type *, vector<int>> p(type, indices);
                    pair<pair<Type *, vector<int>>, Value *> pp(p, gep_inst);
                    bool push_back = true;

                    if (push_back)
                    {
                        GEPTypeOffsetInstList.push_back(pp);
                        recorded++;
                    }
                }
            }
            if (next)
            {
                next = false;
                cnt += F->getInstructionCount();
                break;
            }
        }
    }
    endTime = clock();
    llvm::outs() << "Recording DONE. [" << (endTime - startTime) / CLOCKS_PER_SEC << "] secs"
                 << "\n";

    if(recorded != GEPTypeOffsetInstList.size())
    {
        llvm::outs() << "[ERROR] recorded != GEPTypeOffsetInstList.size(). Exit. check at line: " << __LINE__ <<"\n";
        exit(1);
    }
    llvm::outs() << "-   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -\n";
    llvm::outs() << "Record " << recorded << " gep_ins,  Ignore " << ignored << " unhandled gep_ins.\n";
    llvm::outs() << "---------------------------------------------------------------------\n";
    return 1;
}

struct ConfigVariableNameInfo* readConfigVariableNameByOne(string buf_str)
{
    std::istringstream iss(buf_str);
	std::string type, var_name, origin_config_name;
	iss >> type >> var_name >> origin_config_name;
	struct ConfigVariableNameInfo* config_name = new ConfigVariableNameInfo();
	config_name->OriginalConfigName = origin_config_name;
	if (type == "SINGLE")
	{
		config_name->VarType = SINGLE;
		config_name->SingleName = var_name;
	}
	else if (type == "STRUCT")
	{
		config_name->VarType = STRUCT;
		config_name->StructName = splitWithTag(var_name, ".");
	}
	else if (type == "CLASS")
	{
		config_name->VarType = CLASS;
		config_name->ClassName = splitWithTag(var_name, "::");
	}
	else if (type == "FIELD")
	{
		config_name->VarType = FIELD;
		config_name->FieldName = splitWithTag(var_name, ".");
	}
	return config_name;
}

std::vector<struct GlobalVariableInfo *> getGlobalVariableInfo(std::unique_ptr<llvm::Module> &module, string var_string)
{
    M = module.get();
    if (!M)
    {
		MY_DEBUG(_ERROR_LEVEL, llvm::outs() <<"Parese module error!\n");
        //MY_DEBUG(_ERROR_LEVEL, Err.print(errs()));
        exit(1);
    }

	struct ConfigVariableNameInfo* config_name = readConfigVariableNameByOne(var_string);
	std::vector<struct ConfigVariableNameInfo *> config_names;
	config_names.push_back(config_name);


    /*********************************
     **
     ** Search from
     **     M->global_begin() to M->global_end()
     ** to find if
     **     config_names  ~~  vector< ConfigVariableNameInfo* >
     ** exist in the LLVM IR. If yes, STORE it in
     **     gv_info_list  ~~  vector< GlobalVariableInfo* >
     **
     *********************************/
    int gv_cnt = 0;
    std::vector<struct GlobalVariableInfo *> gv_info_list;
    gv_info_list.clear();
    for (auto I = M->global_begin(), E = M->global_end(); I != E; ++I)
    {
        GlobalVariable *gv = &*I;

        if (!findConfigVariable(config_names, gv, gv_info_list))
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Not a Config Variable\n");
            continue;
        }
        else
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "It is indeed a Config Variable\n");
        }
    }
    /// ======= FIELD ========
    for(uint config_names_idx=0; config_names_idx<config_names.size(); config_names_idx++){
        if(config_names[config_names_idx]->VarType != FIELD)
            continue;
        vector<uint> offsets;
        offsets.push_back(std::atoi(config_names[config_names_idx]->FieldName[1].c_str()));
        struct GlobalVariableInfo *gv_info = new GlobalVariableInfo(config_names[config_names_idx], nullptr, offsets);
        gv_info_list.push_back(gv_info);
    }
    /// =====================

    /// NOTE: Print to llvm::outs
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n\n\n**********************************************************\n\n\n\n");
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "Found " << gv_info_list.size() << " GlobalVariables in total.\n");
    MY_DEBUG(
        _DEBUG_LEVEL,
        for (auto i = gv_info_list.begin(), e = gv_info_list.end(); i != e; i++) {
            struct GlobalVariableInfo *gv_info = *i;
            gv_info->printNameInfo(1);
            if(gv_info->Ptr){
                llvm::outs() << "                      ";
                gv_info->Ptr->print(llvm::outs());
                llvm::outs() << "\n";
            }else{
                llvm::outs() << "\t\tThis is FIELD type. Not a global variable.\n";
            }
            if (gv_info->NameInfo->VarType != SINGLE && gv_info->NameInfo->VarType != FIELD)
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "GV Type: " << gv_info->GlobalVariableType << "\n";);
        })
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n\n\n**********************************************************\n\n\n\n");
	return gv_info_list;
}

std::vector<struct GlobalVariableInfo *> getGlobalVariableInfo(std::unique_ptr<llvm::Module> &module, std::vector<struct ConfigVariableNameInfo *> config_names)
{
    M = module.get();
    if (!M)
    {
		MY_DEBUG(_ERROR_LEVEL, llvm::outs() <<"Parese module error!\n");
        //MY_DEBUG(_ERROR_LEVEL, Err.print(errs()));
        exit(1);
    }

    /*********************************
     **
     ** Search from
     **     M->global_begin() to M->global_end()
     ** to find if
     **     config_names  ~~  vector< ConfigVariableNameInfo* >
     ** exist in the LLVM IR. If yes, STORE it in
     **     gv_info_list  ~~  vector< GlobalVariableInfo* >
     **
     *********************************/
    int gv_cnt = 0;
    std::vector<struct GlobalVariableInfo *> gv_info_list;
    gv_info_list.clear();
    for (auto I = M->global_begin(), E = M->global_end(); I != E; ++I)
    {
        GlobalVariable *gv = &*I;

        if (!findConfigVariable(config_names, gv, gv_info_list))
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "Not a Config Variable\n");
            continue;
        }
        else
        {
            MY_DEBUG(_REDUNDENCY_LEVEL, llvm::outs() << "It is indeed a Config Variable\n");
        }
    }
    /// ======= FIELD ========
    for(uint config_names_idx=0; config_names_idx<config_names.size(); config_names_idx++){
        if(config_names[config_names_idx]->VarType != FIELD)
            continue;
        vector<uint> offsets;
        offsets.push_back(std::atoi(config_names[config_names_idx]->FieldName[1].c_str()));
        struct GlobalVariableInfo *gv_info = new GlobalVariableInfo(config_names[config_names_idx], nullptr, offsets);
        gv_info_list.push_back(gv_info);
    }
    /// =====================

    /// NOTE: Print to llvm::outs
    MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "\n\n\n**********************************************************\n\n\n\n");
    MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Found " << gv_info_list.size() << " GlobalVariables in total.\n");
    MY_DEBUG(
        _DEBUG_LEVEL,
        for (auto i = gv_info_list.begin(), e = gv_info_list.end(); i != e; i++) {
            struct GlobalVariableInfo *gv_info = *i;
            gv_info->printNameInfo(1);
            if(gv_info->Ptr){
                llvm::outs() << "                      ";
                gv_info->Ptr->print(llvm::outs());
                llvm::outs() << "\n";
            }else{
                llvm::outs() << "\t\tThis is FIELD type. Not a global variable.\n";
            }
            if (gv_info->NameInfo->VarType != SINGLE && gv_info->NameInfo->VarType != FIELD)
                MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "GV Type: " << gv_info->GlobalVariableType << "\n";);
        })
    MY_DEBUG(_DEBUG_LEVEL, llvm::outs() << "\n\n\n**********************************************************\n\n\n\n");
	return gv_info_list;
}

int startAnalysis(std::vector<struct GlobalVariableInfo *> gv_info_list, bool isAnalysisImplicitData, bool isAnalysisImplicitControl)
{
	clock_t startTime = clock();
    visitedStructGVCases.clear();
    for (unsigned gv_cnt = 0; gv_cnt < gv_info_list.size(); gv_cnt++)
    {
        //  gv_info_list    ~~  std::vector<GlobalVariableInfo*>
        struct GlobalVariableInfo *gv_info = gv_info_list[gv_cnt];
        GlobalVariable *gv = gv_info_list[gv_cnt]->Ptr;

        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "\n\n\n\n========= Progress: ["<< gv_cnt+1 << "/" << gv_info_list.size() <<"] ========\n\n");
        MY_DEBUG(_ERROR_LEVEL, llvm::outs() << "Current Analyzing GV is : ");
        MY_DEBUG(_ERROR_LEVEL, gv_info_list[gv_cnt]->NameInfo->print(0));

        /*============================================
         * This is the very core function of this tool.
          ============================================*/
        handleUser(gv, gv_info, nullptr, 0);
    }

    /// NOTE: Record information to 'records.dat' and DEBUG INFO.
    clock_t endTime = clock();
    llvm::outs() << "\nTainting DONE. [" << (endTime - startTime) / CLOCKS_PER_SEC << "] secs"
                 << "\n";
    llvm::outs() << "---------------------------------------------------------------------\n";
	return 1;
}

int main3(int argc, char** argv){
	std::unique_ptr<llvm::Module> module;
	LLVMContext context; SMDiagnostic Err;
	string ir_file = "/root/Desktop/confTainter-main/test/demo2/test.ll";
	string var_string = "SINGLE	sum haha";
	buildModule(module, context, Err, ir_file);
	std::vector<struct GlobalVariableInfo *> gvlist = getGlobalVariableInfo(module, var_string);
	startAnalysis(gvlist, true, true);
	
	for (auto i = gvlist.begin(), e = gvlist.end(); i != e; i++)
    {
        struct GlobalVariableInfo *gv_info = *i;
		vector<struct InstInfo *> hali = gv_info->getExplicitDataFlow();
		llvm::outs() <<"getExplicitDataFlow size= "<<hali.size()<<"\n";
		vector<struct InstInfo *> hali2 = gv_info->getImplicitDataFlow();
		llvm::outs() <<"getImplicitDataFlow size= "<<hali2.size()<<"\n";
    }
	return 0;
}

