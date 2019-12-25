# Dali
## Hierarchical Placer for Custom Cell

### Important
  * Eigen gives different results for different C++ compilers, because floating point addition is not necessarily associative
  * Recommended compiler: g++ (GNU Compiler Collection) version >= 4.8.5 
  * g++ in MacOS is an alias of clang
  * 32bit and 64bit version g++ also give different results

### Recommended compilation toolchain
  * GCC, version >= 7.4.0
  * Cmake, version >= 3.10.2

### Pre-requisite for submodules: OpenDB
  * Tcl/Tk, version >= ? (8.6.8)
  * GCC, version >= ? (7.4.0)
  * Cmake, version >= ? (3.10.2)
  * Bison, version >= ? (3.0.4)
  * Flex, version >= ? (2.6.4)
  * Swig, version >= ? (3.0.12)

Minimum version requirement is unknown. Version numbers in parenthesis are used by the developer.
  
### Clone repo and compile
    $ git clone --recursive https://github.com/Yang-Yihang/Dali.git
    $ cd Dali/
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
this will create a binary __dali__ in folder _bin_. You can also do
    
    $ make hpwl
to create a binary __hpwl__ to report Half-Perimeter Wirelength (HPWL). Based on the preference, one an also do
    
    $ make install
Use cmake to specify the installation destination.

### 3rd Party Module List
  * Eigen: sparse matrix iterative linear solver
  * OpenDB: LEF/DEF parser