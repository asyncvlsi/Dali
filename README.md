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
  * GoogleTest is optional. If CMake cannot find it, tests in `tests/common` are
    skipped while the rest of the build remains available. On Ubuntu/Debian,
    install it with `sudo apt install libgtest-dev`.
  
### Clone repo and compile
    $ git clone --recursive https://github.com/asyncvlsi/Dali.git
    $ cd Dali/
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ make install
this will create a binary __dali__ in folder _Dali/bin_. 
The default installation destination is `$ACT_HOME`. 
One can use the following command to specify the installation destination and install this package:

    $ cmake .. -DCMAKE_INSTALL_PREFIX=path/to/installation

### Run tests
After configuring and building from the `build/` directory, run:

    $ make test-unit

This runs the fast GoogleTest-based unit tests in `tests/application`,
`tests/common`, and `tests/circuit`.

To include integration tests such as the I/O placer benchmarks, run:

    $ make test-integration

To run every test registered with CTest, run:

    $ make test-all

The equivalent raw CTest commands are:

    $ ctest --output-on-failure -L unit
    $ ctest --output-on-failure -L integration
    $ ctest --output-on-failure

GoogleTest is optional. If CMake cannot find it, GoogleTest-based unit tests are
skipped while integration tests and the rest of the build remain available. If
the test executables have not been built yet, build first:

    $ make

### 3rd Party Module List
  * Eigen: sparse matrix iterative linear solver
  
### Miscellaneous
  * Eigen gives different results for different C++ compilers, because floating point addition is not necessarily associative
  * g++ in MacOS is an alias of clang instead of GCC
  * 32bit and 64bit version g++ also give different results
