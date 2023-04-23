#ifndef SIMPLE_AGENT_H
#define SIMPLE_AGENT_H

#include "agent.h"
#include "game.h"
#include "level.grpc.pb.h"
#include "math.h"
#include <deque>

struct PlanPoint {
  Point2i pos;
  bool place_bomb = false;
  bool operator==(const Point2i& p) { return pos == p; }
};

class GridMap {
public:
  struct State {
    State(bool s = 0, const bman::LevelState::Bomb* bomb = nullptr,
          const bman::LevelState::Brick* brick = nullptr)
        : solid(s), has_brick(brick && brick->solid()),
          bomb_player_id(bomb ? bomb->player_id() : -1),
          powerup(brick ? brick->powerup() : bman::PUP_NONE) {}
    bool solid = false;
    bool has_brick = false;
    int bomb_player_id = -1;
    bman::Powerup powerup = bman::PUP_NONE;
    int explosion_timer = 0;
  };
  GridMap(const bman::GameConfig& config, const bman::GameState& game_state)
      : game_config_(config), width_(config.level_width()) {
    data_.resize(config.level_width() * config.level_height());

    BombMapConst bomb_map = Game::MakeBombMap(game_state);
    BrickMapConst brick_map;
    for (auto& brick : game_state.level().bricks()) {
      brick_map[Point2i(brick.x(), brick.y())] = &brick;
    }

    for (int y = 0; y < config.level_height(); ++y) {
      for (int x = 0; x < config.level_width(); ++x) {
        Point2i pt(x, y);
        State state = {
            Game::IsStaticBrick(config, x, y) ||
                (brick_map.count(pt) && brick_map.at(pt)->solid()),
            bomb_map.count(pt) ? bomb_map.at(pt) : nullptr,
            brick_map.count(pt) ? brick_map.at(pt) : nullptr,
        };
        data_[y * width_ + x] = state;
      }
    }
    // Add in any explosions
    for (const auto explosion : game_state.level().explosions()) {
      for (const auto pt : explosion.points()) {
        data_[pt.y() * width_ + pt.x()].explosion_timer = explosion.timer();
      }
    }
  }
  bool IsExplosion(const Point2i& pt) const {
    const State& state = data_[pt.y * width_ + pt.x];
    return (state.explosion_timer > 0);
  }
  bool CanMove(const Point2i& pt) const {
    if (Game::IsStaticBrick(game_config_, pt.x, pt.y))
      return false;
    const State& state = data_[pt.y * width_ + pt.x];
    return !(state.solid || state.bomb_player_id >= 0);
  }
  void PlaceBomb(const Point2i& pt, int player_id) {
    data_[pt.y * width_ + pt.x].bomb_player_id = player_id;
  }
  bool HasBomb(const Point2i& pt) const {
    return data_[pt.y * width_ + pt.x].bomb_player_id >= 0;
  }
  bool HasPowerup(const Point2i& pt) const {
    return data_[pt.y * width_ + pt.x].powerup != bman::PUP_NONE;
  }
  bool HasSolidBrick(const Point2i& pt) const {
    const State& state = data_[pt.y * width_ + pt.x];
    return state.has_brick;
  }

private:
  bman::GameConfig game_config_;
  std::vector<State> data_;
  int width_;
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
