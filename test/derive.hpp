#include <cstddef>
#include <cstdint>

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

struct Meta {
  double _x;
};

struct Base : Meta {
  int a, b;
};

union U {
  size_t some;
  char other[8];
};

struct Sub : public Base {
  double x, y;
  EnumCase k;
  U u1, u2;
  SingleClass s;
  int64_t pp, p2, p23;
};