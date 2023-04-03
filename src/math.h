#ifndef _BMAN_UTILS_H_
#define _BMAN_UTILS_H_

inline int sign(int x) { return x < 0 ? -1 : (x > 0 ? 1 : 0); }

inline int GridRound(int x) {
  if (x >= 0)
    return x / kSubpixelSize;
  return (x - kSubpixelSize) / kSubpixelSize;
}

inline int SignedMin(int val, int mag) {
  return sign(val) * std::min(abs(val), abs(mag));
}

#endif
