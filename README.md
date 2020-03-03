# Dali
## Hierarchical Placer for Custom Cell

### Recommended compilation toolchain
  * Ubuntu 18.04
  * GNU Compiler Collection (GCC), version >= 4.8.5
  * CMake, version >= 3.9.6
  * GNU Make

### Pre-requisite for submodules: OpenDB
  * Tcl/Tk-dev, version >= ? (8.6.8, may need to install this package manually)
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
to create a binary __hpwl__ to report Half-Perimeter Wire-length (HPWL). Based on the preference, one an also do
    
    $ make install
Use cmake to specify the installation destination:

    $ cmake .. -DCMAKE_INSTALL_PREFIX=path/to/installation
The default installation destination is the repo directory.

### 3rd Party Module List
  * Eigen: sparse matrix iterative linear solver
  * OpenDB
  * Si2 open source LEF/DEF parser
  
### Miscellaneous
  * Eigen gives different results for different C++ compilers, because floating point addition is not necessarily associative
  * g++ in MacOS is an alias of clang instead of GCC
  * 32bit and 64bit version g++ also give different results
