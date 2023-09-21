# CxxGen
CXX code generator using Clang.    

+ cxxgen-display     
  Generate `__display` function for given struct, which print all members with `std::cout`. (mostly used in debug, hahaa)  

+ cxxgen-arrow  
  Generate `__arrowgen` function for given struct, which return a `arrow::RecordBatch` with schema same as members in struct. 


# Quick Start
```sh
# usuage:
> cxxgen-display <cppfile> <struct_name> 

# e.g.
# will get output in output.cc and get compile error in stderr 
> cxxgen-display test/simple.h EClass > output.cc

# usuage:
> cxxgen-arrow <cppfile> <struct_name> 

# e.g.
# will get output in output.cc and get compile error in stderr 
> cxxgen-arrow test/derive.h > output.cc

```


# Build 
```sh 
$ git clone https://github.com/Guo-Shiyu/CxxGen .
$ cd CxxGen

# using xmake 
$ xmake 

# using cmake 
$ mkdir build && cd build && cmake .. 
$ make 
```

# Caution 
None, :)  


