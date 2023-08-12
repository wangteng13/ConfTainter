#include "test.h"

int sum = 0; // parameter 1

int foo(int in){
    int y;
    if(in >0)
        y=1;
    else
        y=2;

    int out = in;
    return out + y;
}
/*
define dso_local i32 @_Z3fooi(i32 %in) #0 !dbg !862 {
entry:
  %in.addr = alloca i32, align 4
  %y = alloca i32, align 4
  %out = alloca i32, align 4
  store i32 %in, i32* %in.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %in.addr, metadata !863, metadata !DIExpression()), !dbg !864
  call void @llvm.dbg.declare(metadata i32* %y, metadata !865, metadata !DIExpression()), !dbg !866
  %0 = load i32, i32* %in.addr, align 4, !dbg !867
  %cmp = icmp sgt i32 %0, 0, !dbg !869
  br i1 %cmp, label %if.then, label %if.else, !dbg !870

if.then:                                          ; preds = %entry
  store i32 1, i32* %y, align 4, !dbg !871
  br label %if.end, !dbg !872

if.else:                                          ; preds = %entry
  store i32 2, i32* %y, align 4, !dbg !873
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  call void @llvm.dbg.declare(metadata i32* %out, metadata !874, metadata !DIExpression()), !dbg !875
  %1 = load i32, i32* %in.addr, align 4, !dbg !876
  store i32 %1, i32* %out, align 4, !dbg !875
  %2 = load i32, i32* %out, align 4, !dbg !877
  %3 = load i32, i32* %y, align 4, !dbg !878
  %add = add nsw i32 %2, %3, !dbg !879
  ret i32 %add, !dbg !880
}
*/



int testFun(struct TTT* ttt_hhc, int & k){
    int a, b;
    b = sum + 1;
    b += 1;
    a = b *4;
    k = a-1;
    ttt_hhc->field_i = foo(sum);
    int c = k * 8;
    return a;
}
/*
define dso_local i32 @_Z7testFunP3TTTRi(%struct.TTT* %ttt_hhc, i32* dereferenceable(4) %k) #0 !dbg !870 {
entry:
  %ttt_hhc.addr = alloca %struct.TTT*, align 8
  %k.addr = alloca i32*, align 8
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %c = alloca i32, align 4
  store %struct.TTT* %ttt_hhc, %struct.TTT** %ttt_hhc.addr, align 8
  call void @llvm.dbg.declare(metadata %struct.TTT** %ttt_hhc.addr, metadata !874, metadata !DIExpression()), !dbg !875
  store i32* %k, i32** %k.addr, align 8
  call void @llvm.dbg.declare(metadata i32** %k.addr, metadata !876, metadata !DIExpression()), !dbg !877
  call void @llvm.dbg.declare(metadata i32* %a, metadata !878, metadata !DIExpression()), !dbg !879
  call void @llvm.dbg.declare(metadata i32* %b, metadata !880, metadata !DIExpression()), !dbg !881
  %0 = load i32, i32* @sum, align 4, !dbg !882
  %add = add nsw i32 %0, 1, !dbg !883
  store i32 %add, i32* %b, align 4, !dbg !884
  %1 = load i32, i32* %b, align 4, !dbg !885
  %add1 = add nsw i32 %1, 1, !dbg !885
  store i32 %add1, i32* %b, align 4, !dbg !885
  %2 = load i32, i32* %b, align 4, !dbg !886
  %mul = mul nsw i32 %2, 4, !dbg !887
  store i32 %mul, i32* %a, align 4, !dbg !888
  %3 = load i32, i32* %a, align 4, !dbg !889
  %sub = sub nsw i32 %3, 1, !dbg !890
  %4 = load i32*, i32** %k.addr, align 8, !dbg !891
  store i32 %sub, i32* %4, align 4, !dbg !892
  %5 = load i32, i32* @sum, align 4, !dbg !893
  %call = call i32 @_Z3fooi(i32 %5), !dbg !894
  %6 = load %struct.TTT*, %struct.TTT** %ttt_hhc.addr, align 8, !dbg !895 
  %field_i = getelementptr inbounds %struct.TTT, %struct.TTT* %6, i32 0, i32 8, !dbg !896 <------------------------------ GEP
  store i32 %call, i32* %field_i, align 4, !dbg !897 <------------------------------------------------------------------- store
  call void @llvm.dbg.declare(metadata i32* %c, metadata !898, metadata !DIExpression()), !dbg !899
  %7 = load i32*, i32** %k.addr, align 8, !dbg !900
  %8 = load i32, i32* %7, align 4, !dbg !900
  %mul2 = mul nsw i32 %8, 8, !dbg !901
  store i32 %mul2, i32* %c, align 4, !dbg !899
  %9 = load i32, i32* %a, align 4, !dbg !902
  ret i32 %9, !dbg !903
}
*/




int testBar(struct TTT* ttt_hhc){

    int l = 1;
    if(ttt_hhc->field_i > 1){
        l = l + 1;
        return l;
    }
    return 0;
}

/*
define dso_local i32 @_Z7testBarP3TTT(%struct.TTT* %ttt_hhc) #0 !dbg !915 {
entry:
  %retval = alloca i32, align 4
  %ttt_hhc.addr = alloca %struct.TTT*, align 8
  %l = alloca i32, align 4
  store %struct.TTT* %ttt_hhc, %struct.TTT** %ttt_hhc.addr, align 8
  call void @llvm.dbg.declare(metadata %struct.TTT** %ttt_hhc.addr, metadata !918, metadata !DIExpression()), !dbg !919
  %0 = load %struct.TTT*, %struct.TTT** %ttt_hhc.addr, align 8, !dbg !920
  %field_i = getelementptr inbounds %struct.TTT, %struct.TTT* %0, i32 0, i32 8, !dbg !922
  %1 = load i32, i32* %field_i, align 4, !dbg !922
  %cmp = icmp sgt i32 %1, 1, !dbg !923
  br i1 %cmp, label %if.then, label %if.end, !dbg !924

if.then:                                          ; preds = %entry
  call void @llvm.dbg.declare(metadata i32* %l, metadata !925, metadata !DIExpression()), !dbg !927
  store i32 1, i32* %l, align 4, !dbg !927
  %2 = load i32, i32* %l, align 4, !dbg !928
  %add = add nsw i32 %2, 1, !dbg !929
  store i32 %add, i32* %l, align 4, !dbg !930
  %3 = load i32, i32* %l, align 4, !dbg !931
  store i32 %3, i32* %retval, align 4, !dbg !932
  br label %return, !dbg !932

if.end:                                           ; preds = %entry
  store i32 0, i32* %retval, align 4, !dbg !933
  br label %return, !dbg !933

return:                                           ; preds = %if.end, %if.then
  %4 = load i32, i32* %retval, align 4, !dbg !934
  ret i32 %4, !dbg !934
}

*/

/*
struct TTT ttt;
struct TTT* ttt_ptr = (struct TTT*)malloc(sizeof(struct TTT));

int foo(int x)
{
    static int yyyy = 2;
    return x*yyyy/2;
}


void bar(int x, int y)
{
    x = x*2;
}

int zed(int& zed, int m, int n)
{
    zed = m + n;
    return 0;
}
*/


void testAddress(int & a, int b){
    a=sum|b;
    a+=a;
}
/**************** -O0 ********************
define dso_local void @_Z11testAddressRii(i32* dereferenceable(4) %a, i32 %b) #0 !dbg !935 {
entry:
  %a.addr = alloca i32*, align 8
  %b.addr = alloca i32, align 4
  store i32* %a, i32** %a.addr, align 8
                            call void @llvm.dbg.declare(metadata i32** %a.addr, metadata !938, metadata !DIExpression()), !dbg !939
  store i32 %b, i32* %b.addr, align 4
                            call void @llvm.dbg.declare(metadata i32* %b.addr, metadata !940, metadata !DIExpression()), !dbg !941
  
  %0 = load i32, i32* @sum, align 4, !dbg !942
  %1 = load i32, i32* %b.addr, align 4, !dbg !943
  %or = or i32 %0, %1, !dbg !944
  %2 = load i32*, i32** %a.addr, align 8, !dbg !945
  store i32 %or, i32* %2, align 4, !dbg !946

  %3 = load i32*, i32** %a.addr, align 8, !dbg !947
  %4 = load i32, i32* %3, align 4, !dbg !947
  %5 = load i32*, i32** %a.addr, align 8, !dbg !948
  %6 = load i32, i32* %5, align 4, !dbg !949
  %add = add nsw i32 %6, %4, !dbg !949
  store i32 %add, i32* %5, align 4, !dbg !949
  
  ret void, !dbg !950
}
*/


/**************** -O2 ********************
define dso_local void @_Z11testAddressRii(i32* nocapture dereferenceable(4) %a, i32 %b) local_unnamed_addr #1 !dbg !964 {
entry:
  call void @llvm.dbg.value(metadata i32* %a, metadata !968, metadata !DIExpression()), !dbg !970
  call void @llvm.dbg.value(metadata i32 %b, metadata !969, metadata !DIExpression()), !dbg !970
  %0 = load i32, i32* @sum, align 4, !dbg !971, !tbaa !931
  %or = or i32 %0, %b, !dbg !972
  store i32 %or, i32* %a, align 4, !dbg !973, !tbaa !931  <------- i32* %a === store->getPointerOperand()
  ret void, !dbg !974
}
*/

int testphi = 0;
int main()
{
    int tmp = 0;
    struct TTT* ttt_ptr2 = (struct TTT*)malloc(sizeof(struct TTT));
    testFun(ttt_ptr2, tmp);
    int continue_conf = testBar(ttt_ptr2);
    delete ttt_ptr2;

    int should_out=0;
    testAddress(should_out, 2);
    int kkk = should_out + 3;
	
	if(sum > 0)
		testphi = should_out+continue_conf;
	else testphi = should_out-tmp;

    return continue_conf + kkk;
}

/*
define dso_local i32 @main() #2 !dbg !948 {
entry:
  %retval = alloca i32, align 4
  %tmp = alloca i32, align 4
  %ttt_ptr2 = alloca %struct.TTT*, align 8
  %continue_conf = alloca i32, align 4
  %should_out = alloca i32, align 4
  %kkk = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  call void @llvm.dbg.declare(metadata i32* %tmp, metadata !949, metadata !DIExpression()), !dbg !950
  store i32 0, i32* %tmp, align 4, !dbg !950
  call void @llvm.dbg.declare(metadata %struct.TTT** %ttt_ptr2, metadata !951, metadata !DIExpression()), !dbg !952
  %call = call noalias i8* @malloc(i64 72) #5, !dbg !953
  %0 = bitcast i8* %call to %struct.TTT*, !dbg !954
  store %struct.TTT* %0, %struct.TTT** %ttt_ptr2, align 8, !dbg !952
  %1 = load %struct.TTT*, %struct.TTT** %ttt_ptr2, align 8, !dbg !955
  %call1 = call i32 @_Z7testFunP3TTTRi(%struct.TTT* %1, i32* dereferenceable(4) %tmp), !dbg !956
  call void @llvm.dbg.declare(metadata i32* %continue_conf, metadata !957, metadata !DIExpression()), !dbg !958
  %2 = load %struct.TTT*, %struct.TTT** %ttt_ptr2, align 8, !dbg !959
  %call2 = call i32 @_Z7testBarP3TTT(%struct.TTT* %2), !dbg !960
  store i32 %call2, i32* %continue_conf, align 4, !dbg !958
  %3 = load %struct.TTT*, %struct.TTT** %ttt_ptr2, align 8, !dbg !961
  %isnull = icmp eq %struct.TTT* %3, null, !dbg !962
  br i1 %isnull, label %delete.end, label %delete.notnull, !dbg !962

delete.notnull:                                   ; preds = %entry
  call void @_ZN3TTTD2Ev(%struct.TTT* %3) #5, !dbg !962
  %4 = bitcast %struct.TTT* %3 to i8*, !dbg !962
  call void @_ZdlPv(i8* %4) #6, !dbg !962
  br label %delete.end, !dbg !962

delete.end:                                       ; preds = %delete.notnull, %entry
  call void @llvm.dbg.declare(metadata i32* %should_out, metadata !963, metadata !DIExpression()), !dbg !964
  store i32 0, i32* %should_out, align 4, !dbg !964
  call void @_Z11testAddressRii(i32* dereferenceable(4) %should_out, i32 2), !dbg !965
  call void @llvm.dbg.declare(metadata i32* %kkk, metadata !966, metadata !DIExpression()), !dbg !967
  %5 = load i32, i32* %should_out, align 4, !dbg !968
  %add = add nsw i32 %5, 3, !dbg !969
  store i32 %add, i32* %kkk, align 4, !dbg !967
  %6 = load i32, i32* %continue_conf, align 4, !dbg !970
  %7 = load i32, i32* %kkk, align 4, !dbg !971
  %add3 = add nsw i32 %6, %7, !dbg !972
  ret i32 %add3, !dbg !973
}
*/
