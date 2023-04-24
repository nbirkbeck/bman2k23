#ifndef SIMPLE_AGENT_H
#define SIMPLE_AGENT_H

#include "agent.h"
#include "game.h"
#include "grid_map.h"
#include "level.grpc.pb.h"
#include "math.h"
#include <deque>

struct PlanPoint {
  Point2i pos;
  bool place_bomb = false;
  bool operator==(const Point2i& p) { return pos == p; }
};

class SimpleAgent : public Agent {
public:
  SimpleAgent(const bman::GameConfig& config, int player_index)
      : game_config_(config), player_index_(player_index) {
    replan_prob_ = 0.5;
  }

  bman::MovePlayerRequest
  GetPlayerAction(const bman::GameState& game_state_const);

private:
  std::unordered_set<Point2i, PointHash> FindNoGoZones(const GridMap& grid_map);

  void MaybeCreateNewPlan(bool reached_waypoint, const GridMap& grid_map,
                          const Point2i& pos, int player_strength);

  bman::GameConfig game_config_;
  const int player_index_;

  double replan_prob_;

  std::deque<PlanPoint> plan_;
};

#endif
