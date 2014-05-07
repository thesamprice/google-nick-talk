#include "common/problem.h"

Colour test1(int i) {
  Colour c = static_cast<Colour>(i);
  return c;
}

template <typename T, T V> struct X {};
typedef X<Colour, static_cast<Colour>(NUM_COLOURS - 1)> Y;
