namespace common {
namespace base {

enum Colour {
  RED,
  GREEN,
  TRANSPARENT,
  WHITE,
  OFFWHITE,
  // In the real code, this had O(100) entries.
  NUM_COLOURS
};

bool Colour_IsValid(int value);
const Colour Colour_MIN = RED;
const Colour Colour_MAX = NUM_COLOURS;

struct Coord {
  int x, y;
};

struct Drawable {
  Drawable();
  ~Drawable();
  Coord top_left, bottom_right;
  Colour background, foreground;
};

struct DrawState {
  Colour colour;
  enum Winding { CW, CCW } winding;
};
}
}
