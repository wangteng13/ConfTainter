# ConfTainter

`ConfTainter` is a **Static Taint Analysis Infrastructure** for configuration options.
It is based on LLVM IR, and analyzes the control and data dependency starting from the specified configuration variable(s)

- [ConfTainter: Static Taint Analysis For Configuration Options](https://leopard-lab.github.io/paper/ase23-ConfTainter.pdf)   
Teng Wang, Haochen He, Xiaodong Liu, Shanshan Li, Zhouyang Jia, Yu Jiang, Qing Liao, Wang Li. "ConfTainter: Static Taint Analysis For Configuration Options", In Proceedings of the 38th ACM/IEEE International Conference on Automated Software Engineering (ASE 2023), 11-15 September, 2023, Luxembourg. 


## Data flow

 - **Intra-procedural analysis ( basic LLVM "Use" support )**  
   <img width="250" alt="截屏2022-09-03 15 27 06" src="https://user-images.githubusercontent.com/18543932/188260809-360ee1bd-6966-4fa7-80fe-7c5a290174be.png">
 - **Field sensitive analysis**  
   <img width="582" alt="截屏2022-09-03 15 44 43" src="https://user-images.githubusercontent.com/18543932/188261335-a40776f5-6a85-4224-90a1-157dd872b1fb.png">
 - **Inter-procedure (with pointer)**  
   <img width="417" alt="截屏2022-09-03 15 53 39" src="https://user-images.githubusercontent.com/18543932/188261592-d35d625b-e808-4f43-9a01-01a431ed94bb.png">  
   <img width="417" alt="截屏2022-09-03 16 02 01" src="https://user-images.githubusercontent.com/18543932/188261868-e32bc710-4e42-4dfb-be9a-a6dfc957211b.png">
 - **Implicit data-flow (`phi-node`)**  
   <img width="600" alt="截屏2022-09-03 16 06 28" src="https://user-images.githubusercontent.com/18543932/188262007-992034c9-a3d2-4fce-96e3-2cc85589dae3.png">  
   - How to formaly determine if a `phi-node` will be tainted  
     Given a `phiNode` like:
     ```
        phi i32 [ %5, %bb1.i ], [ 0, %bb1 ]
                   pre_node      pre_node2
     ```
     we check if:
      <img width="400" alt="截屏2022-09-03 16 07 53" src="https://user-images.githubusercontent.com/18543932/188262045-11a5b2c0-48a3-4cf5-9039-077f8ffffb7c.png">

## Control flow

Formaly define how the control flow:
 - **Control Dependency on Configuration**: A block **Y** is control-dependent on a configuration option **C** if and only if (a) the branching instruction of block **X** is tainted by **C**; (b) **Y** is control-dependent on **X**. 
   - **Control Dependent**: A block **Y** is control dependent on block **X** if and only if: **Y** post-dominates at least one but not all successors of **X**.

An example, where the yellow square indicats the complicated code structures that motivate the use of the formal definition.  
<img width="1000" alt="截屏2022-09-03 16 39 03" src="https://user-images.githubusercontent.com/18543932/188263144-892a1294-d302-4dea-82bb-eedd21f18e78.png">

- **Implicit Control flow**: Except for explicit control-flow propagation, configuration options also implicitly propagate control dependency and dominate program blocks, using delay statements (e.g., sleep function). If a delay function occurs in a loop, and are tainted or dominated by the target option, other basic blocks in the loop can be considered as implicit control-flow dependencies.
   
## Usage

### Dependency

 - llvm-10.0.0
 - [wllvm](https://github.com/SRI-CSL/whole-program-llvm) or [gllvm](https://github.com/SRI-CSL/gllvm) 

### Build
```
cd src
cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++-10 -DCMAKE_C_COMPILER=/usr/bin/clang-10 -DLLVM_DIR=/usr/lib/llvm-10/cmake . 
make
```

### Run
```
cd src/test/demo
../../tainter test.bc test-var.txt
```
For real systems, use `wllvm` to obtain the `.bc` file (e.g., mysqld.bc).

### Check results
```
cat test-records.dat
```
 
### Specify the entry configuration variable
 - `SINGLE CONF_VAR_NAME` global variable with basic type (`int`, `bool`, etc.)
 - `STRUCT CONF_VAR_STRUCT.FIELD_NAME` **global** struct with field
 - `CLASS CONF_VAR_CLASS.FIELD_NAME` **global** class with field
 - `FIELD CONF_VAR_TPYE.FIELD_COUNT` **any** field of specified type, for example, use `FIELD some_type.2` to make `some_type.field_C` as the entry point.  
    ```
    STRUCT some_type{
       int field_A;
       bool field_B;
       float field_C;
    }
    ```

### How to debug:
 1. Make sure you have use the right compilation options: `-O0`、`-fno-discard-value-names`、`-g`; if you want the `PhiNode` analysis, also use [these two options](https://stackoverflow.com/questions/72123225).  
 2. Make sure the specified configuration variable name is right.
    - Check if it exists in source code via simple search `grep CONF_NAME /dir/of/src`.  
    - Check if it has been compiled into the target `.bc` file `grep CONF_VAR_NAME /dir/to/target.ll`.  
 
****
## How to scale ConfTainter

### Examples
An Demo of applying ConfTainter is shown in /src/TestMain.cpp

Users can use the following codes to conduct taint analysis.
```cpp
  string ir_file = string(argv[1]); //input the ir path
  string var_file = string(argv[2]);  //input the variable mapping path
  std::vector<struct ConfigVariableNameInfo *> config_names;
  if (!readConfigVariableNames(var_file, config_names)) exit(1);
  
  std::unique_ptr<llvm::Module> module;
  LLVMContext context; SMDiagnostic Err;
  buildModule(module, context, Err, ir_file); //build the llvm module based on ir
  std::vector<struct GlobalVariableInfo *> gvlist = getGlobalVariableInfo(module, config_names);  //conduct Configuration Variable Mapping
  startAnalysis(gvlist, true, true);    //Taint Analysis with implicit data-flow, and implicit control-flow
```

### Application Programming Interface (API)

The details of APIs can be found in /src/tainter.h

For example,
```cpp
bool readConfigVariableNames( std::string, std::vector< struct ConfigVariableNameInfo* >&);

int buildModule(std::unique_ptr<llvm::Module> &module, LLVMContext &context, SMDiagnostic &Err, string ir_file);

int startAnalysis(std::vector<struct GlobalVariableInfo *> gv_info_list, bool isAnalysisImplicitData, bool isAnalysisImplicitControl);

GlobalVariableInfo::vector<struct InstInfo *> getExplicitDataFlow();

```

### Apply/Scale ConfTainter to configuration-related tasks

We conduct three prototype experiments by applying `ConfTainter` to 
- **Misconfiguration Detection** (**TInfer** in /ApplicabilityExperiment_5_3_1)
- **Configuration-related Bug Detection** (**TCub** in /ApplicabilityExperiment_5_3_2) (**TFuzz** in /ApplicabilityExperiment_5_3_3)
