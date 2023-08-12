#include "tainter.h"

struct DLCallGraphNode{
    Function * fun_ptr;
    uint calls;
    CallBase* callbase_inst;

    /*  
     *  TODO:
     *  CalledFunctionsVector = std::vector< CallRecord >
     *  using llvm::CallGraphNode::CallRecord = std::pair<Optional<WeakTrackingVH>, CallGraphNode *>
     */
    vector<Function *> called_func; 
    vector<Function *> caller_funcs;

    DLCallGraphNode(Function * f, uint c, CallBase* ci){
        fun_ptr = f; 
        calls = c; 
        callbase_inst = ci;
    }
};

struct DLCallGraph{

    DLCallGraphNode * enter;

    set<DLCallGraphNode *> allDLCallGraphNodes;

    vector<Function *> * getCallers(Function * f){

        for(set<DLCallGraphNode *>::iterator I = this->allDLCallGraphNodes.begin(); I != this->allDLCallGraphNodes.end(); I++){
            if ((*I)->fun_ptr == f)
                return &(*I)->caller_funcs;
        }
        llvm::outs() << "NOT FOUND: " << f->getName().str() << " in the DLCallGraph via `getCallers`.\n";
        return nullptr;
    }

    DLCallGraph(){}
    ~DLCallGraph(){}
    
    DLCallGraph(CallGraph *CG){

        uint remaining_calls = 0;
        DLCallGraphNode * cur_parent = nullptr;
        queue<DLCallGraphNode *> tobe_parent;
        bool checkA = true;
        
        for (auto IT = bf_begin(CG), EI = bf_end(CG); IT != EI; IT++) {

            if (Function *F = IT->getFunction()) {

                llvm::outs() << "Visiting function: " << getOriginalName(F->getName().str()) << ", callees: " << F->size() << "\n";
                //cur_parent = IT;

                for (CallGraphNode::iterator calledNode_IT = IT->begin(); calledNode_IT != IT->end(); calledNode_IT++){
                    
                    if(calledNode_IT->second->getFunction()->hasExternalLinkage()){
                        continue;

                    } else {
                        
                        if(CallBase* cb = llvm::dyn_cast<CallBase>(calledNode_IT->first)) {
                            
                            DLCallGraphNode* newnode = new DLCallGraphNode(calledNode_IT->second->getFunction(), calledNode_IT->second->size(), cb);
                            
                            cur_parent->called_func.push_back(newnode->fun_ptr);
                            if(cur_parent->fun_ptr) // when cur_parent == this->enter, cur_parent->fun_ptr is nullptr
                                newnode->caller_funcs.push_back(cur_parent->fun_ptr);

                            this->allDLCallGraphNodes.insert(newnode);

                        } else {

                            llvm::outs() << "STRAGE. NOT a CallBase for the calledNode_IT->first" <<getClassType(calledNode_IT->first) << "\n";
                        }
                        
                    }
                }

            } else {

                llvm::outs() << "Visiting function: nullptr. This is usually the root node of the CallGraph.\n";
                this->enter = new DLCallGraphNode(nullptr, IT->size(), nullptr);
                cur_parent = this->enter;
                continue;
            }
            /*
            if (!checkA) {
                llvm::outs() << "This should be very wrong, since CallGraph traversing is not done but tobe_parent is empty.\n";
            }

            // only this branch in first iteration.
            if (this->enter == nullptr){
                this->enter = new DLCallGraphNode(IT->getFunction(), IT->size());
                this->allDLCallGraphNodes.insert(this->enter);
                cur_parent = this->enter;
                remaining_calls = cur_parent->calls;
                continue;
            }

            // the last sub-parent-son-tree has all been visited.
            if (remaining_calls == 0){
                
                if (tobe_parent.empty()) {
                    checkA = false;
                    llvm::outs() << "This should be the last node when building DLCallGraph\n";
                    continue;
                }

                cur_parent = tobe_parent.front();
                tobe_parent.pop();
                remaining_calls = cur_parent->calls;
            
            } else {
            
                DLCallGraphNode * cur = new DLCallGraphNode(IT->getFunction(), IT->size());
                this->allDLCallGraphNodes.insert(cur);
                tobe_parent.push(cur);

                // Double-linked.
                cur->caller_funcs.push_back(cur_parent->fun_ptr);
                cur_parent->called_func.push_back(cur->fun_ptr);

                remaining_calls--;
            }
        */
        }
    }

};