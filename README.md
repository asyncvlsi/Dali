# Dali
## Gridded Cell Placement Flow

### Recommended compilation toolchain
  * Ubuntu >= 18.04
  * GNU Compiler Collection (GCC), version >= 4.8.5
  * CMake, version >= 3.9.6
  * GNU Make

### Pre-requisite
  * [ACT](https://github.com/asyncvlsi/act)
  * Boost, version >= 1.71.0 (lower version may work, not tested)
  * Si2 LEF/DEF parser, a mirror can be found [here](https://github.com/asyncvlsi/lefdef)
  * [PhyDB](https://github.com/asyncvlsi/phyDB)
  * OpenMP (for MacOS user, `libomp` from Homebrew will work)
  
### Clone repo and compile
    $ git clone --recursive https://github.com/asyncvlsi/Dali.git
    $ cd Dali/
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
this will create a binary __dali__ in folder _Dali/bin_. You can also do
    
    $ make hpwl
to create a binary __hpwl__ to report Half-Perimeter Wire-length (HPWL). Based on the preference, one an also do
    
    $ make install
Use cmake to specify the installation destination:

    $ cmake .. -DCMAKE_INSTALL_PREFIX=path/to/installation
The default installation destination is the repo directory.

### 3rd Party Module List
  * Eigen: sparse matrix iterative linear solver
  
### Miscellaneous
  * Eigen gives different results for different C++ compilers, because floating point addition is not necessarily associative
  * g++ in MacOS is an alias of clang instead of GCC
  * 32bit and 64bit version g++ also give different results
