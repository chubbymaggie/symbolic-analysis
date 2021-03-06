#!/usr/bin/perl 
use Getopt::Long;
use strict;
use warnings;


#################### PATHS #################
my $SCRIPTDIR       = "/home/sdasgup3/SymbolicAnalysis/Scripts/";
my $llvmpalib       = "/home/sdasgup3/SymbolicAnalysis/zesti/Release+Asserts/lib//";

##############################################

my $test = "";
my $testll = "";
my $maxt = "";
my $result = "";
my $reuse = "";
my $genexec = "";
my $dsa = "";
my $run = "";
my $watch = "";
my $modifiedll = "";
my $withoutcheker = "";
my @progargs = "";
my $zest = "1";
my $offset = "3";
my $klee_src = "";
my $klee_obj = "";
my $llvm_dir = "";

GetOptions ("wc"        => \$withoutcheker, 
            "test=s"    => \$test, 
            "testll=s"    => \$testll, 
            "offset=s"  => \$offset, 
            "zest"      => \$zest, 
            "result"    => \$result, 
            "reuse"     => \$reuse, 
            "genexec"   => \$genexec, 
            "dsa"       => \$dsa, 
            "run"       => \$run, 
            "watch"     => \$watch, 
            "args=s"    => \@progargs, 
            "maxt=s"    => \$maxt, 
            "klee_src=s"    => \$klee_src, 
            "klee_obj=s"    => \$klee_obj, 
            "llvm_dir=s"    => \$llvm_dir, 
            "mdll"      => \$modifiedll) 
 or die("Error in command line arguments\n");


if("" eq $test && "" eq $testll) {
  print "Specify -test [-zest offset=N] [-wc -mdll] -klee_src -klee_obj -llvm_dir \n";
  print "Usage: ~/SymbolicAnalysis/Scripts/build.pl -wc -zest -offset 3 -klee_src ~/SymbolicAnalysis/zesti/ -klee_obj ~/SymbolicAnalysis/zesti-obj/ -llvm_dir ~/llvm/llvm-3.4.2/llvm-build/ -test ?\n";
  exit(1);
}

if("" eq $klee_src || "" eq $klee_obj) {
  $klee_src = "~/SymbolicAnalysis/zesti/";
  $klee_obj = "~/SymbolicAnalysis/zesti-obj/";
  print "Using klee_src as $klee_src\n";
  print "Using klee_obj as $klee_obj\n";
}
if("" eq $llvm_dir) {
  $llvm_dir = "~/llvm/llvm-3.4.2/llvm-build/";
  print "Using llvm_dir as $llvm_dir\n";
}

print "Using offset as $offset" . "\n";
print "Using -zest as $zest" . "\n";

my $make        = "make -f $SCRIPTDIR/Makefile"; 

###  LLVM Args
my $llvm_bin    = "$llvm_dir/Release+Asserts/bin/";
my $clang       = "$llvm_bin/clang";
my $llvmdis     = "$llvm_bin/llvm-dis";
my $llvmas      = "$llvm_bin/llvm-as";
my $llvmld      = "$llvm_bin/llvm-ld";
my $palib       = "$klee_src/lib/";
my $painclude   = "$klee_src/include/";

if(!(-e "$llvmld")) { # We llvm-ld not there, so search for llvm-link
  $llvmld = "$llvm_bin/llvm-link";
} else {
  $llvmld = $llvmld  . " -disable-opt "
}

###  Klee/Zesti Args
my $runkleetest = "$SCRIPTDIR/runseq";
my $watchV      = "$SCRIPTDIR/watchV";
my $kleeexec    = "$klee_obj/Release+Asserts/bin/klee";
my $kleeincl    = "$klee_src/include/klee";
my $maxtime     = "";
my $kleeargs    = "";

if("" eq $maxt) {
  print "Setting max time to default 172800\n";
  $maxtime = "172800";
} else {
  $maxtime = $maxt;
}


if($zest eq "") {

  $kleeargs = 
        " --simplify-sym-indices --write-cvcs --write-cov --output-module"
      . " --max-memory=1000 --disable-inlining "
      . " --use-forked-solver" 
      . " --libc=uclibc --allow-external-sym-calls --only-output-states-covering-new" 
      . " --use-query-log=solver:pc"
      . " --max-sym-array-size=4096 --max-instruction-time=120 --max-time=$maxtime" 
      . " --watchdog --max-memory-inhibit=false --max-static-fork-pct=1" 
      . " --max-static-solve-pct=1 --max-static-cpfork-pct=1 --switch-type=internal" 
      . " --randomize-fork --search=random-path --search=nurs:covnew" 
      . " --use-batching-search --batch-instructions=10000"; 
} else {
  $kleeargs = " --zest  -aachecks  --disable-opt --debug-print-aachecks -debug-print-instructions   --zest-depth-offset=$offset       --use-symbex=2 --symbex-for=10 --search=zest --zest-search-heuristic=br --zest-discard-far-states=false"; 
#$kleeargs = " --zest            --disable-opt                        -debug-print-instructions   --zest-depth-offset=$offset       --use-symbex=2 --symbex-for=10 --search=zest --zest-search-heuristic=br --zest-discard-far-states=false"; 

  if("" ne $dsa) {
    $kleeargs = $kleeargs .  " --dsa";
  }

}


if(defined($test) || defined($testll)) {

  if("" ne $test) {
    execute("$make clean");
    execute("$make $test LLVM_BIN=$llvm_bin   KLEEINCLUDE=$kleeincl");
    execute("cp $test.bc $test.a.out.bc");
  } else {
    $test = $testll;
    execute("$llvmas $test.ll -o $test.bc");
    execute("cp $test.bc $test.a.out.bc");
  }

  &runKlee;
  execute("echo");
  execute("echo");
  exit(0);
}


sub runKlee {
  if(-e "$test.a.out.bc") {
    execute("$kleeexec $kleeargs ./$test.a.out.bc @progargs");
  } else {
    execute("echo ");
  }
}

sub execute {
  my $args = shift @_;
  print "$args \n";
  system("$args");
}

if("" ne $result) {
  system("cat $runkleetest");
  system("tcsh $runkleetest");
  system("cat $watchV");
  system("tcsh $watchV");
  exit(0);
}
if("" ne $run) {
  system("cat $runkleetest");
  system("tcsh $runkleetest");
  exit(0);
}
if("" ne $watch) {
  system("cat $watchV");
  system("tcsh $watchV");
  exit(0);
}

if("" ne $reuse) {
  if(-e "$test.a.out.bc") {
    &runKlee;
  }
  exit(0);
}

#if($zest eq "") {
#
## $kleeargs  = "-write-test-info";
## $kleeargs  = "--libc=uclibc  --allow-external-sym-calls";
## $kleeargs  = "--emit-all-errors";
##$kleeargs = "--libc=uclibc --allow-external-sym-calls  --max-time=$maxtime";
#  $kleeargs = 
#        " --simplify-sym-indices --write-cvcs --write-cov --output-module"
#      . " --max-memory=1000 --disable-inlining "
##    . " --optimize"
#      . " --use-forked-solver" 
##    . " --use-cex-cache --libc=uclibc --posix-runtime"
#      . " --libc=uclibc --allow-external-sym-calls --only-output-states-covering-new" 
##    . " --environ=$SCRIPTDIR/test.env --run-in=/tmp/sandbox" 
##  . " --use-query-log=all:pc"
#      . " --use-query-log=solver:pc"
#      . " --max-sym-array-size=4096 --max-instruction-time=120 --max-time=$maxtime" 
#      . " --watchdog --max-memory-inhibit=false --max-static-fork-pct=1" 
#      . " --max-static-solve-pct=1 --max-static-cpfork-pct=1 --switch-type=internal" 
#      . " --randomize-fork --search=random-path --search=nurs:covnew" 
#      . " --use-batching-search --batch-instructions=10000"; 
##./paste.bc --sym-args 0 1 10 --sym-args 0 2 2 --sym-files 1 8 --sym-stdout "; 
#} else {
##$kleeargs = " --zest --zest-depth-offset=$offset -debug-print-instructions  --use-symbex=2 --symbex-for=10 --search=zest --zest-search-heuristic=br "; 
##$kleeargs = " --zest --zest-depth-offset=$offset     -debug-print-instructions         --use-symbex=2 --symbex-for=10 --search=zest --zest-search-heuristic=br --zest-discard-far-states=false"; 
#  $kleeargs = " --zest      -debug-print-instructions         --use-symbex=2 --symbex-for=10 --search=zest --zest-search-heuristic=br --zest-discard-far-states=false"; 
##-watchdog --max-time=30 --optimize --max-cex-size=0 --zest-continue-after-error=true --output-source=false --no-std-out --output-level=error --use-cex-cache=false ---dump-states-on-halt=false -use-forked-stp --max-stp-time=10 --posix-runtime --libc=uclibc $CU/src/TEMPLATE-EXE.bc ${1+"$@"}    
#}
    #execute("cat $test-kleecheck.ll | sed 's/target datalayout.*//' | sed 's/\!llvm.ident =.*//' | sed 's/\!0 = metadata.*//' | sed 's/^attributes \#[0-9]*.*//' | sed 's/\#[0-9][0-9]*//' >  temp ");
    #execute("mv temp $test-kleecheck.ll") ;

#  }


#  if($modifiedll eq "") {
#    execute("$make clean");
#    execute("$make $test LLVM_BIN=$llvm_bin   KLEEINCLUDE=$kleeincl");
#    execute("echo");
#    execute("$make $test-kleecheck LLVM_BIN=$llvm_bin LLVMPALIB=$llvmpalib KLEEINCLUDE=$kleeincl JFCHECKERMAP=$palib/jf-rt/ JFINCLUDE=$painclude/");
#    execute("cp $test-kleecheck.bc $test.a.out.bc");
#    execute("echo");
#    execute("echo");
#  } else {
#    execute("$llvmas < $test-kleecheck.ll  > $test-kleecheck.bc");
#    execute("cp $test-kleecheck.bc $test.a.out.bc");
#  }
#  if("" eq $enexec) {
#    &runKlee;
#  }
