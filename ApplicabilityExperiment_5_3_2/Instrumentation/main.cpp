
#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <map>

using namespace std;
using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

ofstream LogOut;
vector<string> SourceLocationSet;
static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");
static llvm::cl::opt<std::string> SoftwareName(
    "software",
    llvm::cl::cat(ToolingSampleCategory));

string longestCommonHeadStr(string str1, string str2)
{
    unsigned len = min(str1.length(), str2.length());
    for(unsigned i=0; i<len; i++)
    {
        if( str1[i] != str2[i])
            return str1.substr(0, i);
    }
    return "";
}

inline bool file_exist_check (const std::string& name) {
    ifstream f(name.c_str());
    return f.good();
}

bool readSourcePathInfo(std::vector<std::string>& SourcePathList)
{
    string json_path = SourcePathList[0];
    SourcePathList.clear();
    ifstream fin(json_path.c_str());
    if( ! fin )
    {
        std::cerr<<"Open JsonFilePath Failed!\n";
        return false;
    }

    string tmp;
    while( getline(fin, tmp) )
    {
        if(tmp.length() < 10)
            continue;

        if( tmp.substr(2,7) == "\"file\":")
        {
            string current_path = tmp.substr(11, tmp.find_last_of("\"")-11);
            if( longestCommonHeadStr(current_path, json_path).length() > 10 && 
                file_exist_check(current_path) &&   // filter tmp files
                std::find(SourcePathList.begin(), SourcePathList.end(), current_path) == SourcePathList.end())
            {
                SourcePathList.push_back(current_path);
            }
        }
    }
    fin.close();
    return true;
}

bool compareEarlyLoc(string loc1, string loc2){
  string::size_type pos1 = loc1.find(":");
  string line1 = loc1.substr(0, pos1);
  string::size_type pos2 = loc2.find(":");
  string line2 = loc2.substr(0, pos1);
  cout<<line1<<" "<<line2<<endl;
  cout<<atoi(line1.c_str()) << " "<<atoi(line2.c_str())<<endl;
  if(atoi(line1.c_str()) <= atoi(line2.c_str()))
    return true;
  else
    return false;
}


bool selectOneLocInMethod(){
  map<string, string> LocMap; LocMap.clear();
  for(int i=0; i<SourceLocationSet.size();i++){
    string item = SourceLocationSet[i];

    string::size_type pos = item.find(":");
    if (pos == string::npos){
      std::cerr<<"SourceLocationSet Format Error!\n";
      return false;
    }
    string filename = item.substr(0, pos);
    string::size_type pos2 = item.find("[");
    if (pos == string::npos){
      std::cerr<<"SourceLocationSet Format Error!\n";
      return false;
    }
    string methondname = item.substr(pos2);
    string loc = item.substr(pos+1, pos2-pos-1);

    string key = filename +  methondname;
    if (LocMap.find(key) == LocMap.end())
      LocMap[key] = loc;
    else{
      if (compareEarlyLoc(LocMap[key], loc) == false)
          LocMap[key] = loc;
    }
  }

  
  SourceLocationSet.clear();
  map<string, string>::const_iterator map_it = LocMap.begin();
  while (map_it != LocMap.end()) {
    string::size_type pos = (map_it->first).find("[");
    string filename = (map_it->first).substr(0, pos);
    SourceLocationSet.push_back(filename + ":" +  (map_it->second) );
    map_it++;
  }
}

bool readSourcePathInfo2(std::vector<std::string>& SourcePathList)
{
    string json_path = SourcePathList[0];
    SourcePathList.clear();
    ifstream fin(json_path.c_str());
    if( ! fin )
    {
        std::cerr<<"Open Tainted File Failed!\n";
        return false;
    }

    string tmp;
    while( getline(fin, tmp) )
    {
        if(tmp.length() < 10)
            continue;
        string ss;
        for(int i=0;i<tmp.length();i++)
          if(((tmp[i]=='\t')||(tmp[i]=='\r')||(tmp[i]=='\n')||(tmp[i]==' ')) == false)
            ss.append(tmp,i,1);
        if (std::find(SourceLocationSet.begin(), SourceLocationSet.end(), ss) == SourceLocationSet.end())
          SourceLocationSet.push_back(ss);

        string::size_type pos = ss.find(":");
        if (pos == string::npos)
        {
          std::cerr<<"Tainted File Format Error!\n";
          return false;
        }
        string filename = ss.substr(0, pos);
        if(file_exist_check(filename) &&   // filter tmp files
            std::find(SourcePathList.begin(), SourcePathList.end(), filename) == SourcePathList.end())
        {
          SourcePathList.push_back(filename);
        }
    }

    selectOneLocInMethod();
    cout<<"SourceLocationSet:\n";
    for(int i=0;i<SourceLocationSet.size();i++)
      cout << SourceLocationSet[i]<<endl;
    return true;
}

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods. 
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> { 
public:
  MyASTVisitor(clang::CompilerInstance* CI, Rewriter &R) : TheCI(CI), TheRewriter(R) {}

  // find the SourceLocation of the tail of the stmt
  SourceLocation findTailOfStmt(Stmt *stmt){
    SourceLocation sl = stmt->getBeginLoc();
    PresumedLoc PLoc = TheRewriter.getSourceMgr().getPresumedLoc(sl);
    int theLine = PLoc.getLine();
    int add = 0;
    while(1){
      SourceLocation nsl = sl.getLocWithOffset(add+1);
      PresumedLoc nPLoc = TheRewriter.getSourceMgr().getPresumedLoc(nsl);
      int newLine = nPLoc.getLine();
      if (newLine != theLine)
        break;
      add = add + 1;
    }
    return sl.getLocWithOffset(add);
  }

  // find the SourceLocation of the head of the stmt
  SourceLocation findHeadOfStmt(Stmt *stmt){
    SourceLocation sl = stmt->getBeginLoc();
    PresumedLoc PLoc = TheRewriter.getSourceMgr().getPresumedLoc(sl);
    int theLine = PLoc.getLine();
    int add = 0;
    while(1){
      SourceLocation nsl = sl.getLocWithOffset(add-1);
      PresumedLoc nPLoc = TheRewriter.getSourceMgr().getPresumedLoc(nsl);
      int newLine = nPLoc.getLine();
      if (newLine != theLine)
        break;
      add = add - 1;
    }
    return sl.getLocWithOffset(add-1);
  }

  bool findInSet(string sl){
    int length = SourceLocationSet.size();
    for(int i=0; i<length; i++)
      if (SourceLocationSet[i].find(sl) != string::npos)
        return true;
    return false;
  }

  Stmt* findNoOperatorParent(Stmt* child){
    const Stmt* res = child;
    while(true){
      
      const auto& parents = TheCI->getASTContext().getParents(*res);
      if ( parents.empty() ) {
        llvm::errs() << "Can not find parent \n";
        break;
      }
      const Stmt* par = parents[0].get<Stmt>();
      if(par == nullptr)
        break;
      if(!(isa<DeclRefExpr>(par) || isa<ImplicitCastExpr>(par) 
        || isa<BinaryOperator>(par) || isa<ParenExpr>(par)))
        break;

      res = par;
    }
    return (Stmt*)res;
  }

  string changeCase(string &value, bool lowerCase) {
    int len = value.length();
    string newvalue(value);
    for (string::size_type i = 0, l = newvalue.length(); i < l; ++i)
      newvalue[i] = lowerCase ? tolower(newvalue[i]) : toupper(newvalue[i]);
    return newvalue;
  }

  string lowerCase(string value) {
    return changeCase(value, true);
  }

  string generateInsertCode(string ExprName, string TypeStr, string insertLoc){
    TypeStr = lowerCase(TypeStr);
    string insertcode = "\nprintf(\"Print "+ ExprName + " in " + insertLoc + " = ";
    if (TypeStr.find("int")!=-1 || TypeStr.find("uLong")!=-1 || TypeStr.find("bool")!=-1 || TypeStr.find("long")!=-1)
      insertcode += "%lld";
    else if (TypeStr.find("float")!=-1)
      insertcode += "%lf";
    else if (TypeStr.find("double")!=-1)
      insertcode += "%lf";
    else if (TypeStr.find("char")!=-1)
      insertcode += "%s";
    insertcode += "\\n\", " + ExprName + ");\n";
    return insertcode;
  }
  string generateInsertCodeForSoftware(string Software, string ExprName, string TypeStr, string insertLoc){
    TypeStr = lowerCase(TypeStr);
    string outputcode = "\"Print "+ ExprName + " in " + insertLoc + " = ";
    if (TypeStr.find("int")!=-1 || TypeStr.find("uLong")!=-1 || TypeStr.find("bool")!=-1 || TypeStr.find("long")!=-1)
      outputcode += "%lld";
    else if (TypeStr.find("float")!=-1)
      outputcode += "%lf";
    else if (TypeStr.find("double")!=-1)
      outputcode += "%lf";
    else if (TypeStr.find("char")!=-1)
      outputcode += "%s";
    outputcode += "\\n\", " + ExprName ;

    string insertcode = "";
    if (Software.find("MySQL") != string::npos)
      insertcode = "\nfprintf(stderr, " + outputcode + ");\n";
    if (Software.find("MariaDB") != string::npos)
      insertcode = "\nfprintf(stderr, " + outputcode + ");\n";
    if (Software.find("PostgreSQL") != string::npos)
      insertcode = "\nelog(ERROR, " + outputcode + ");\n";
    if (Software.find("Redis") != string::npos)
      insertcode = "\nFILE *fp = fopen(\"/root/tests/rlog.txt\", \"a+\"); fprintf(fp, " + outputcode + "); fclose(fp);\n";
    if (Software.find("Nginx") != string::npos)
      insertcode = "\nngx_log_error(NGX_LOG_ALERT, cf->log, 0, " + outputcode + ");\n";
    if (Software.find("Squid") != string::npos)
      insertcode = "\nFILE *fp = fopen(\"/root/tests/rlog.txt\", \"a+\"); fprintf(fp, " + outputcode + "); fclose(fp);\n";
    if (Software.find("Httpd") != string::npos)
      insertcode = "\nFILE *fp = fopen(\"/root/tests/rlog.txt\", \"a+\"); fprintf(fp, " + outputcode + "); fclose(fp);\n";

    return insertcode;
  }
  
  bool VisitStmt(Stmt *s) {
    SourceLocation sll = s->getEndLoc();
    string ssll = sll.printToString(TheRewriter.getSourceMgr());
    if (findInSet(ssll) == false) return true;

    if (isa<DeclRefExpr>(s)){
      cout<<ssll<<endl;
      DeclRefExpr *dre = cast<DeclRefExpr>(s);
      DeclarationName DeclName = dre->getNameInfo().getName();
      std::string ExprName = DeclName.getAsString();
      cout<<"name :"<<ExprName<<endl;

      QualType QT = dre->getType();
      std::string TypeStr = QT.getAsString();
      cout<<"TypeStr :"<<TypeStr<<endl;

      Stmt* father = findNoOperatorParent(dre);

      string insertLoc = findHeadOfStmt(father).printToString(TheRewriter.getSourceMgr());
      //string insertcode = generateInsertCode(ExprName, TypeStr, insertLoc);
      string insertcode = generateInsertCodeForSoftware(SoftwareName, ExprName, TypeStr, insertLoc);

      TheRewriter.InsertText(findHeadOfStmt(father), insertcode, true, true);
      cout<<"insert in "<<insertLoc<<endl;
    }
    return true;

    if (isa<BinaryOperator>(s)){
      BinaryOperator *stmt = cast<BinaryOperator>(s);
      if (stmt->isAssignmentOp() == false) return true;

      SourceLocation sl = stmt->getEndLoc();
      cout<<endl<<sl.printToString(TheRewriter.getSourceMgr())<<endl;
      Expr *lhs = stmt->getLHS()->IgnoreImpCasts();

      if (isa<DeclRefExpr>(lhs)){
        DeclRefExpr *dre = cast<DeclRefExpr>(lhs);
        DeclarationName DeclName = dre->getNameInfo().getName();
        std::string FuncName = DeclName.getAsString();
        cout<<"name :"<<FuncName<<endl;

        QualType QT = dre->getType();
        std::string TypeStr = QT.getAsString();
        cout<<"TypeStr :"<<TypeStr<<endl;
      }

    }

    return true;
  }


private:
  Rewriter &TheRewriter;
  clang::CompilerInstance* TheCI;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(clang::CompilerInstance* CI, Rewriter &R) : Visitor(CI, R) {}

  // Override the method that gets called for each parsed top-level declaration.
  bool HandleTopLevelDecl(DeclGroupRef DR) override {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
      // Traverse the declaration using our AST visitor.
      Visitor.TraverseDecl(*b);
      //(*b)->dump();
    }
    return true;
  }
private:
  MyASTVisitor Visitor;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  void EndSourceFileAction() override {
    SourceManager &SM = TheRewriter.getSourceMgr();
    llvm::errs() << "** EndSourceFileAction for: "
                 << SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";

    // Now emit the rewritten buffer.
    string outName (SM.getFileEntryForID(SM.getMainFileID())->getName());
    outName = outName.append(".tmp");
    std::error_code EC;
    llvm::raw_fd_ostream outFile(outName.c_str(), EC, llvm::sys::fs::F_Text);
    TheRewriter.getEditBuffer(SM.getMainFileID()).write(outFile);
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    llvm::errs() << "** Creating AST consumer for: " << file << "\n";
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<MyASTConsumer>(&CI, TheRewriter);
  }

public:
  Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
  LogOut.open("./out.txt");
  SourceLocationSet.clear();

  std::unique_ptr<clang::tooling::FrontendActionFactory> FrontendFactory;
  clang::tooling::CommonOptionsParser OptionsParser(argc, argv, ToolingSampleCategory);
  std::vector<std::string> SourcePathList = OptionsParser.getSourcePathList();

  cout<<"software= "<<SoftwareName<<endl;

  readSourcePathInfo2(SourcePathList);

  int total_files = SourcePathList.size();

  vector<string> cur_source;
  
  for(unsigned i=0; i<SourcePathList.size(); i++)
  {
      cur_source.clear();
      cur_source.push_back(SourcePathList[i]);
      LogOut<<SourcePathList[i]<<endl;

      FrontendFactory = clang::tooling::newFrontendActionFactory<MyFrontendAction>();
      clang::tooling::ClangTool Tool(OptionsParser.getCompilations(), cur_source);
      clang::IgnoringDiagConsumer* idc = new clang::IgnoringDiagConsumer();
      Tool.clearArgumentsAdjusters();
      Tool.setDiagnosticConsumer(idc);
      Tool.run(FrontendFactory.get());
  }
  LogOut.close();
  return 0;
}



