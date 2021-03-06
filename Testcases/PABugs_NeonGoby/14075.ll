; ModuleID = 'test-thread-specific.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

;The return value of pthread_getspecific may alias with previous arguments to pthread_setspecific.  In the attached ll, %call2 and %data alias. But, DSAA reports it as NoAlias. 
;It seems impractical to add summary for all external library calls. 
;http://llvm.org/bugs/show_bug.cgi?id=14075

@key = common global i32 0, align 4
@.str = private unnamed_addr constant [15 x i8] c"&data == data2\00", align 1
@.str1 = private unnamed_addr constant [23 x i8] c"test-thread-specific.c\00", align 1
@__PRETTY_FUNCTION__.main = private unnamed_addr constant [23 x i8] c"int main(int, char **)\00", align 1

define i32 @main(i32 %argc, i8** %argv) nounwind uwtable {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %data = alloca i32, align 4                                   ; HERE
  %data2 = alloca i32*, align 8
  store i32 0, i32* %retval
  store i32 %argc, i32* %argc.addr, align 4
  store i8** %argv, i8*** %argv.addr, align 8
  store i32 0, i32* %data, align 4
  %0 = bitcast i32* %data to i8*
  call void @klee_make_symbolic(i8* %0, i64 4, i8* getelementptr inbounds ([15 x i8]* @.str, i32 0, i32 0))
  %call = call i32 @pthread_key_create(i32* @key, void (i8*)* null) nounwind
  %1 = load i32* @key, align 4
  %2 = bitcast i32* %data to i8*
  %call1 = call i32 @pthread_setspecific(i32 %1, i8* %2) nounwind ; %data passed as argument 
  %3 = load i32* @key, align 4
  %call2 = call i8* @pthread_getspecific(i32 %3) nounwind         ; call2 should to aliased with %data
  %4 = bitcast i8* %call2 to i32*
  store i32* %4, i32** %data2, align 8
  %5 = load i32** %data2, align 8
  %cmp = icmp eq i32* %data, %5
  %6 = load i8* %call2                                            ; load added to have the pointer analysis check     
  br i1 %cmp, label %cond.true, label %cond.false

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  call void @__assert_fail(i8* getelementptr inbounds ([15 x i8]* @.str, i32 0, i32 0), i8* getelementptr inbounds ([23 x i8]* @.str1, i32 0, i32 0), i32 13, i8* getelementptr inbounds ([23 x i8]* @__PRETTY_FUNCTION__.main, i32 0, i32 0)) noreturn nounwind
  unreachable
                                                  ; No predecessors!
  br label %cond.end

cond.end:                                         ; preds = %5, %cond.true
  ret i32 0
}

declare i32 @pthread_key_create(i32*, void (i8*)*) nounwind

declare i32 @pthread_setspecific(i32, i8*) nounwind

declare i8* @pthread_getspecific(i32) nounwind

declare void @__assert_fail(i8*, i8*, i32, i8*) noreturn nounwind

declare void @klee_make_symbolic(i8*, i64, i8*)
