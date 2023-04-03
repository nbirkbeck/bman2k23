#ifndef _POINT_H_
#define _POINT_H_ 1

template <typename T>
class Point2 {
 public:
  Point2(const T& _x = 0, const T& _y = 0) : x(_x), y(_y) {}
  Point2 operator+(const Point2& other) {
    return Point2<T>(x + other.x, y + other.y);
  }
  bool operator==(const Point2& other) const {
    return x== other.x and y == other.y;
  }
  bool operator!=(const Point2& other) const {
    return !(*this == other);
  }
  T x, y;
};
typedef Point2<int> Point2i;

#endif
