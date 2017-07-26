#Testing a basic version of Qnial7

One of the features of Nial as a language is that it has many testable
relationships within its functionality. It's data structures are based 
on Trenchard More's array theory ideas that were refined as we developed 
Nial. A central idea in array theory is to have the semantic functions obey
laws or identities. Many of these laws are universal, that is, they are
true for all possible array values. In designing the linguistic aspects of
the language, Mike Jenkins tried to have similar universal relations hold 
for the execution capabilities of the langauage. A benefit of this approach
is that the implementation can be tested for compliance of these universal 
laws under program control.

This directory is used to test a basic version of nial to ensure that the 
universal laws have not been compromised in making changes to the core routines
or by unexpected behaviour of C code on a new platform.

Use pkgblder to build package "basic_nial.txt" or "basic_debug.txt" and 
copy the executable "nial" created in build to this directory.

The file autotest  runs the automated testing, producing files *.out to
indicate errors that have been detected in the run, and files *.log that
document the tests that have been executed. If no errors are detected then
there are no *.out files. 

To run the automated testing enter: 

   $ ./autotest

This runs the Unix script:

rm *.out
rm *.log
./nial +size 1000000 -defs autoids
./nial +size 1000000 -defs autocov
./nial +size 1000000 -defs autoops
./nial +size 1000000 -defs autoeval
./nial +size 1000000 -defs autopic
./nial +size 1000000 -defs autorand

The first test is autoids that tests the identities that should hold for Nial
data. The test is driven by the routine testid in file testid.ndf that checks
that a named identity holds for each item of each variable in a list of
variable names. The autoids.ndf file loads a sequence of identities and 
tests that check the universal identities. It also checks a number of other
properties:
    - that pervasive data operations give consistent answers for numeric 
      arguments of various types, 
    - that the general pervasive identities hold for pervasive operations,
      and that the implemented transformers match their formal definitions.

The output of the tests is recorded in ids.out whenever a test fails. The 
output is logged into ids.log to capture what tests have actually been run.

The second test is autocov that does coverage tests for the Nial operations
using the file coverops.ndf that contains a methodology for exercising most
of the Nial operations with a wide variety of data inputs. The goal is to 
check for system failure due to unexpected values and to monitor for space 
usage. The program uses three files uniops, binops  and pictops to 
systematically run through  the unary data, binary data nad picture-related
operations respectively. Operations that are simple renamings of other ones
are not included.

The third test is autoops that checks that the data operations perform as
expected on some standard data examples The specific tests are in the file
alltests. Output from the tests goes to a file testops.out if a test fails
and a log of the tests is put into testops.log in order to keep track of what
tests have actually been done. 

The fourth test is autoeval that tests the interpreter evaluation mechanisms.
The main part of the evaluator is checked by ensuring that predicate 
expressions that test a case for each path through the parser and evaluator
are satisfied and that the two canonical identities:
   canonical canonical a = canonical a
   execute a = execute canonical a
hold, where 
   canonical is link descan deparse parse scan

The following test sets are used:
   atcases	juxtaposition table, strands, list notation
   control	sequencing, selection, loops, exit
   scope	blocks, comments, opforms, trforms
   assign	assignment
   indexing	left and right side indexing nnotations
   quoted	program quotes using the !() construct
   defines	definitions, defn sequences, externals
   recurs	recursive constructs and difficult recursions from the past
   constant	tests that constants are handled corectly
   limits	tests of limits of lexical tokens
   pclimits tests of limits of Intel-PC arithmetic

The fifth test is autopic that tests the array diagramming code. Models of
the diagram and sketch operations that work in both decor and nodecor modes
have been constructed. These can be run against generated data. The tests
make use of the Nial paste operation. The cases where the results do not 
match are logged in pic.out.

The final test is autorand that does randomized testing on many of the 
identities by generating random array data. The file initializes a seed
value for the random numbers and sets the number of cases generated. You
may want edit this file to initialize the random generator or to change
the number of cases.

Several of the above testing routines also measure the amount of space
consumed during execution of the tests. The fact that space consumption
in the workspace remains extremely small indicates that the interpreter
is very robust.

The tests run by autotest have been thoroughly exercised in the development
of Q'Nial V7.  This testing should only need to be done if you are changing
one of the fundamental design aspects of the interpreter. If you do so, we
recommend that you build a system with DEBUG on and test with it before
testing a production version.

