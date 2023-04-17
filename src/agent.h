#ifndef BMAN_AGENT_H
#define BMAN_AGENT_H

#include "constants.h"
#include "level.grpc.pb.h"
#include "math.h"

class Agent {
public:
  virtual ~Agent() {}
  virtual bman::MovePlayerRequest
  GetPlayerAction(const bman::GameState& game_state) = 0;

  static void GetDeltaFromDir(int dir, int* dx, int* dy) {
    static constexpr int kDirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    if (dir < 0 || dir >= 4) {
      *dx = *dy = 0;
      return;
    }
    *dx = (kAgentSpeed * kDirs[dir][0]);
    *dy = (kAgentSpeed * kDirs[dir][1]);
  }

  static int GetDirFromDelta(int dx, int dy) {
    static constexpr int kDirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    if (abs(dx) > abs(dy))
      dy = 0;
    else
      dx = 0;
    for (int k = 0; k < 4; ++k) {
      if (sign(dx) == kDirs[k][0] && sign(dy) == kDirs[k][1])
        return k;
    }
    return -1;
  }
};

#endif
