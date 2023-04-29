#ifndef GRID_MAP_H
#define GRID_MAP_H

#include "level.grpc.pb.h"
#include "point.h"
#include <vector>

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
  GridMap(const bman::GameConfig& config, const bman::GameState& game_state);

  bool IsExplosion(const Point2i& pt) const {
    const State& state = data_[pt.y * width_ + pt.x];
    return (state.explosion_timer > 0);
  }
  bool CanMove(const Point2i& pt) const;

  void PlaceBomb(const Point2i& pt, int player_id) {
    data_[pt.y * width_ + pt.x].bomb_player_id = player_id;
  }
  bool HasBomb(const Point2i& pt) const {
    return data_[pt.y * width_ + pt.x].bomb_player_id >= 0;
  }
  bool HasPowerup(const Point2i& pt) const {
    const State& state = data_[pt.y * width_ + pt.x];
    return !state.has_brick && state.powerup != bman::PUP_NONE;
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

#endif
