#ifndef _POINT_H_
#define _POINT_H_ 1

template <typename T> class Point2 {
public:
  Point2(const T& _x = 0, const T& _y = 0) : x(_x), y(_y) {}
  Point2 operator+(const Point2& other) const {
    return Point2<T>(x + other.x, y + other.y);
  }
  Point2 operator-(const Point2& other) const {
    return Point2<T>(x - other.x, y - other.y);
  }
  Point2 operator*(int s) const { return Point2<T>(x * s, y * s); }
  bool operator==(const Point2& other) const {
    return x == other.x and y == other.y;
  }
  bool operator!=(const Point2& other) const { return !(*this == other); }

  T x, y;
};
typedef Point2<int> Point2i;

struct PointHash {
  size_t operator()(const std::pair<int32_t, int32_t>& point) const {
    return point.first * 1024 + point.second;
  }
  size_t operator()(const Point2i& point) const {
    return point.x * 1024 + point.y;
  }
};

#endif
