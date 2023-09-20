# CxxGen
CXX code generator using Clang.

# Quick Start
```sh
# usuage:
> cxxgen <cppfile> <struct_name> 

# e.g.
# will get output in output.cc and get compile error in stderr 
> cxxgen test/simple.h EClass > output.cc

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


