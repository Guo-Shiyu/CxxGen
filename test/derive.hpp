#include <cstddef>
#include <iterator>

enum class EnumCase {
  Bad,
  Good,
  Ok = -1,
  Error = 999,
};

struct ArrayElem {
  int e[4];
};

struct SingleClass {
  int wow, owo;
};

struct Base {
  int a, b;
};

union U {
  size_t some;
  char other[8];
};

struct Sub : public Base {
  double x, y;
  EnumCase k;
  ArrayElem array[4];
  U u1, u2;
  SingleClass s;
};