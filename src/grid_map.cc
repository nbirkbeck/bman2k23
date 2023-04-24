#include "grid_map.h"
#include "game.h"

GridMap::GridMap(const bman::GameConfig& config,
                 const bman::GameState& game_state)
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

bool GridMap::CanMove(const Point2i& pt) const {
  if (Game::IsStaticBrick(game_config_, pt.x, pt.y))
    return false;
  const State& state = data_[pt.y * width_ + pt.x];
  return !(state.solid || state.bomb_player_id >= 0);
}
