#include "common/problem.h"

template <typename T, bool IsValid(int)> struct Container {
  typedef Container<T, IsValid> Self;
  Self *next;
  T t;
};

Container<Colour, Colour_IsValid> list;
