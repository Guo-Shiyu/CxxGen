
#include <cstddef>
#include <cstdint>

enum class E  {
  kA,
  kBB,
  kCCC,
};

class EClass {
  int a;
  int b;
  double c, d;
  char e[32];
  E f;
  size_t g;
  int64_t h;
};

// clang-format off 
// $ clang-check -ast-dump  --ast-dump-filter EClass test/simple.hpp 
//
// Dumping EClass:
// CXXRecordDecl 0x55f38ecd1300 </home/lucas/github/CxxGen/test/simple.hpp:10:1, line:18:1> line:10:7 class EClass definition
// |-DefinitionData pass_in_registers standard_layout trivially_copyable trivial literal
// | |-DefaultConstructor exists trivial needs_implicit
// | |-CopyConstructor simple trivial has_const_param needs_implicit implicit_has_const_param
// | |-MoveConstructor exists simple trivial needs_implicit
// | |-CopyAssignment simple trivial has_const_param needs_implicit implicit_has_const_param
// | |-MoveAssignment exists simple trivial needs_implicit
// | `-Destructor simple irrelevant trivial needs_implicit
// |-CXXRecordDecl 0x55f38ecd1418 <col:1, col:7> col:7 implicit class EClass
// |-FieldDecl 0x55f38ecd14c0 <line:11:3, col:7> col:7 a 'int'
// |-FieldDecl 0x55f38ecd1528 <line:12:3, col:7> col:7 b 'int'
// |-FieldDecl 0x55f38ecd1590 <line:13:3, col:10> col:10 c 'double'
// |-FieldDecl 0x55f38ecd15f8 <col:3, col:13> col:13 d 'double'
// |-FieldDecl 0x55f38ecd16f8 <line:14:3, col:12> col:8 e 'char[32]'
// |-FieldDecl 0x55f38ecd1790 <line:15:3, col:5> col:5 f 'E':'E'
// |-FieldDecl 0x55f38ecd1850 <line:16:3, col:10> col:10 g 'size_t':'unsigned long'
// `-FieldDecl 0x55f38ecd1910 <line:17:3, col:11> col:11 h 'int64_t':'long'
// clang-format on 