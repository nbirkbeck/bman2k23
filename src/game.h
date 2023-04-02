#ifndef _BMAN_GAME_H_
#define _BMAN_GAME_H_ 1

#include <glog/logging.h>

#include "level.grpc.pb.h"
#include <iostream>
#include <memory>
#include <unordered_map>

const int kDefaultWidth = 17;
const int kDefaultHeight = 13;

const int kSubpixelSize = 64;
const int kSubPixelSize = 64;
const int kDefaultBombTimer = 5 * 60;
const int kExplosionTimer = 30;
const int kMovePadding = kSubpixelSize / 16;

struct PointHash {
  size_t operator()(const std::pair<int32_t, int32_t>& point) const {
    return point.first * 1024 + point.second;
  }
};

typedef std::unordered_map<std::pair<int32_t, int32_t>,
                           bman::LevelState::Bomb*,
                           PointHash> BombMap;
typedef std::unordered_map<std::pair<int32_t, int32_t>,
                           bman::LevelState::Brick*,
                           PointHash> BrickMap;

int sign(int x) { return x < 0 ? -1 : (x > 0 ? 1 : 0); }

int GridRound(int x) {
  if (x >= 0)
    return x / kSubpixelSize;
  return (x - kSubpixelSize) / kSubpixelSize;
}

int SignedMin(int val, int mag) {
  return sign(val) * std::min(abs(val), abs(mag));
}

class Game {
public:
  Game() {}

  void BuildSimpleLevel(int padding) {
    config_.set_level_width(kDefaultWidth);
    config_.set_level_height(kDefaultHeight);
    auto* player_config = config_.mutable_player_config();
    player_config->set_num_bombs(2);
    player_config->set_strength(1);
    player_config->set_max_players(4);
    player_config->set_health(1);

    auto* level_state = config_.mutable_level_state();
    int num = 0;
    int num_powerups = 0;
    for (int y = padding; y < kDefaultHeight - padding; ++y) {
      for (int x = padding; x < kDefaultWidth - padding; ++x) {
        if (!IsStaticBrick(x, y)) {
          if (num % 3 == 0) {
            auto* brick = level_state->add_bricks();
            brick->set_x(x);
            brick->set_y(y);
            brick->set_solid(true);
            if ((num / 3) % 3 == 1) {
              num_powerups++;
              brick->set_powerup(
                  static_cast<bman::Powerup>(1 + (num_powerups % 3)));
            }
          }
          num++;
        }
      }
    }
    game_state_.set_clock(0);
    *game_state_.mutable_level() = *level_state;
  }

  void AddPlayer() {
    game_state_.add_score(0);
    auto* player = game_state_.add_players();
    if (game_state_.players_size() == 1) {
      player->set_x(kSubpixelSize / 2);
      player->set_y(kSubpixelSize / 2);
    } else if (game_state_.players_size() == 2) {
      player->set_x(config_.level_width() * kSubpixelSize - kSubpixelSize / 2);
      player->set_y(config_.level_height() * kSubpixelSize - kSubpixelSize / 2);
    }
    player->set_num_bombs(config_.player_config().num_bombs());
    player->set_health(config_.player_config().health());
    player->set_strength(config_.player_config().strength());
  }

  const bman::GameConfig& config() const { return config_; }
  const bman::GameState& game_state() const { return game_state_; }
  bman::GameState& game_state() { return game_state_; }

  bool Step(const std::vector<bman::MovePlayerRequest>& move_requests) {
    if ((int)move_requests.size() != (int)game_state_.players_size()) {
      LOG(ERROR) << "Move request has invalid number of players";
      return false;
    }
    // Build map of bombs and bricks
    BombMap bomb_map = MakeBombMap();
    BrickMap brick_map;
    for (auto& brick : *game_state_.mutable_level()->mutable_bricks()) {
      brick_map[std::make_pair(brick.x(), brick.y())] = &brick;
    }

    // Apply any player actions (e.g., move, drop bomb, etc.)
    std::vector<bman::LevelState::Bomb> new_bombs =
        MovePlayers(move_requests, brick_map, bomb_map);
    if (!new_bombs.empty()) {
      for (const auto& bomb : new_bombs) {
        *game_state_.mutable_level()->add_bombs() = bomb;
      }
      bomb_map = MakeBombMap();
    }

    // Decrement timer on any active explosions
    for (auto& explosion : *game_state_.mutable_level()->mutable_explosions()) {
      explosion.set_timer(explosion.timer() - 1);
    }

    // Go through any bombs and decrement timer.
    for (auto& bomb : *game_state_.mutable_level()->mutable_bombs()) {
      const bool active = bomb.timer() > 0;
      bomb.set_timer(bomb.timer() - 1);
      if (active && bomb.timer() <= 0) {
        auto* explosion = game_state_.mutable_level()->add_explosions();
        ExplodeBomb(bomb_map, brick_map, &bomb, explosion);
      }
    }

    // Remove inactive explosions / bombs.
    RemoveInactiveExplosions();
    RemoveInactiveBombs();

    game_state_.set_clock(game_state_.clock() + 1);
    return true;
  }

  std::vector<bman::LevelState::Bomb>
  MovePlayers(const std::vector<bman::MovePlayerRequest>& move_requests,
              const BrickMap& brick_map, const BombMap& bomb_map) {
    std::vector<bman::LevelState::Bomb> new_bombs;
    int player_index = 0;
    for (auto& move : move_requests) {
      for (auto& action : move.actions()) {
        auto* player = game_state_.mutable_players(player_index);
        int dx = action.dx(), dy = action.dy();
        int min_dx = 0, min_dy = 0;
        int x = player->x(), y = player->y();
        int other_x = 0;
        int other_y = 0;
        const int cur_x = GridRound(x);
        const int cur_y = GridRound(y);

        if (dx > 0 || dx < 0) {
          const int test_x =
              x + dx + sign(dx) * (kSubPixelSize / 2 - kMovePadding);
          other_x = GridRound(test_x);
          other_y = GridRound(y);
          min_dx = (dx > 0)
                       ? std::max(0, cur_x * kSubPixelSize + kSubPixelSize / 2 +
                                         kMovePadding - x)
                       : std::min(0, cur_x * kSubPixelSize + kSubPixelSize / 2 -
                                         kMovePadding - x);
        } else if (dy > 0 || dy < 0) {
          const int test_y =
              y + dy + sign(dy) * (kSubPixelSize / 2 - kMovePadding);
          other_y =  GridRound(test_y);
          other_x = GridRound(x);
          min_dy = (dy > 0)
                       ? std::max(0, cur_y * kSubPixelSize + kSubPixelSize / 2 +
                                         kMovePadding - y)
                       : std::min(0, cur_y * kSubPixelSize + kSubPixelSize / 2 -
                                         kMovePadding - y);
        }
        const bool can_move =
            !IsStaticBrick(other_x, other_y) &&
            (!brick_map.count(std::make_pair(other_x, other_y)) ||
             !brick_map.at(std::make_pair(other_x, other_y))->solid()) &&
            (!bomb_map.count(std::make_pair(other_x, other_y)) ||
             (other_x == cur_x && other_y == cur_y));

        player->set_x(x + (can_move ? dx : min_dx));
        player->set_y(y + (can_move ? dy : min_dy));
        if (action.has_dir()) {
          player->set_anim_counter(player->anim_counter() + 1);
          player->set_dir(action.dir());
        } else {
          player->set_anim_counter(0);
        }
        int new_x = GridRound(player->x());
        int new_y = GridRound(player->y());

        // Move player closer to the square that they've moved into
        if (new_x != cur_x || new_y != cur_y) {
          if (abs(dx)) {
            const int delta =
                player->y() - (new_y * kSubpixelSize + kSubpixelSize / 2);
            int update_y = SignedMin(-delta, dx);
            player->set_y(player->y() + update_y);
          } else if (abs(dy)) {
            const int delta =
                player->x() - (new_x * kSubpixelSize + kSubpixelSize / 2);
            int update_x = SignedMin(-delta, dy);
            player->set_x(player->x() + update_x);
          }
        }

        if (action.place_bomb()) {
          LOG(INFO) << "num_used=" << player->num_used_bombs()
                    << " num_bombs=" << player->num_bombs() << std::endl;
          if (player->num_used_bombs() < player->num_bombs()) {
            if (!bomb_map.count(std::make_pair(cur_x, cur_y)) &&
                !brick_map.count(std::make_pair(cur_x, cur_y))) {
              new_bombs.resize(new_bombs.size() + 1);
              auto& bomb = new_bombs.back();
              bomb.set_x(cur_x);
              bomb.set_y(cur_y);
              bomb.set_strength(player->strength());
              bomb.set_timer(kDefaultBombTimer + 1);
              bomb.set_player_id(player_index);
              player->set_num_used_bombs(player->num_used_bombs() + 1);
            }
          }
        }

        auto new_point =
            std::make_pair(GridRound(player->x()), GridRound(player->y()));
        if (brick_map.count(new_point)) {
          if (!brick_map.at(new_point)->solid()) {
            auto* brick = brick_map.at(new_point);
            if (brick->has_powerup()) {
              LOG(INFO) << "Brick has a powerup.";
              switch (brick->powerup()) {
              case bman::PUP_NONE:
                break;
              case bman::PUP_EXTRA_BOMB:
                player->set_num_bombs(player->num_bombs() + 1);
                break;
              case bman::PUP_FLAME:
                player->set_strength(player->strength() + 1);
                break;
              case bman::PUP_KICK:
                break;
              case bman::PUP_DETONATOR:
                player->set_has_detonator(true);
                break;
              default:
                break;
              }
              brick->clear_powerup();
            }
          }
        }
      }
      player_index++;
    }
    return new_bombs;
  }

  BombMap MakeBombMap() {
    BombMap bomb_map;
    for (auto& bomb : *game_state_.mutable_level()->mutable_bombs()) {
      bomb_map[std::make_pair(bomb.x(), bomb.y())] = &bomb;
    }
    return bomb_map;
  }

  void RemoveInactiveExplosions() {
    const auto& old_end =
        game_state_.mutable_level()->mutable_explosions()->end();
    const auto& new_end = std::remove_if(
        game_state_.mutable_level()->mutable_explosions()->begin(),
        game_state_.mutable_level()->mutable_explosions()->end(),
        [](bman::LevelState::Explosion& explosion) -> bool {
          return explosion.timer() <= 0;
        });
    int num_remove = old_end - new_end;
    for (int i = 0; i < num_remove; ++i) {
      game_state_.mutable_level()->mutable_explosions()->RemoveLast();
    }
  }

  void RemoveInactiveBombs() {
    const auto& old_end = game_state_.mutable_level()->mutable_bombs()->end();
    const auto& new_end = std::remove_if(
        game_state_.mutable_level()->mutable_bombs()->begin(),
        game_state_.mutable_level()->mutable_bombs()->end(),
        [](bman::LevelState::Bomb& bomb) -> bool { return bomb.timer() <= 0; });
    int num_remove = old_end - new_end;
    for (int i = 0; i < num_remove; ++i) {
      game_state_.mutable_level()->mutable_bombs()->RemoveLast();
    }
  }

  void ExplodeBomb(const BombMap& bomb_map, BrickMap& brick_map,
                   bman::LevelState::Bomb* bomb,
                   bman::LevelState::Explosion* explosion) {
    LOG(INFO) << "Exploding bomb\n";
    bomb->set_timer(0); // Marks bomb as inactive

    // Give the player back a bomb
    {
      auto* player = game_state_.mutable_players(bomb->player_id());
      player->set_num_used_bombs(player->num_used_bombs() - 1);
    }

    explosion->set_timer(kExplosionTimer);

    // Go through blast radius and do damage
    auto* p = explosion->add_points();
    p->set_x(bomb->x());
    p->set_y(bomb->y());
    p->set_bomb_center(true);
    for (int dir = 0; dir < 4; ++dir) {
      const int sign = dir < 2 ? 1 : -1;
      const int dx = sign * (dir & 0x1);
      const int dy = sign * (!(dir & 0x1));

      for (int i = 1; i <= bomb->strength(); ++i) {
        auto point = std::make_pair(bomb->x() + dx * i, bomb->y() + dy * i);
        if (IsStaticBrick(point.first, point.second))
          break;

        // Track the point in the explosion.
        p = explosion->add_points();
        p->set_x(point.first);
        p->set_y(point.second);

        // Do damage to players
        for (auto& player : *game_state_.mutable_players()) {
          const int min_x = GridRound(player.x() - kSubpixelSize / 2);
          const int min_y = GridRound(player.y() - kSubpixelSize / 2);
          const int max_x = GridRound(player.x() + kSubpixelSize / 2 - 1);
          const int max_y = GridRound(player.y() + kSubpixelSize / 2 - 1);
          if (min_x <= point.first && point.first <= max_x &&
              min_y <= point.second && point.second <= max_y) {
            player.set_health(player.health() - 1);
            if (player.health() < 0) {
              // TODO: kill player and respawn
            }
          }
        }

        // Damage world.
        if (brick_map.count(point) && brick_map[point]->solid()) {
          brick_map[point]->set_solid(false);
          break;
        }
        // Explode other bombs
        if (bomb_map.count(point)) {
          if (bomb_map.at(point)->timer() > 0) {
            ExplodeBomb(bomb_map, brick_map, bomb_map.at(point), explosion);
          }
          break;
        }
      }
    }
  }

  bool IsStaticBrick(int x, int y) const {
    return IsStaticBrick(config_, x, y);
  }

  static bool IsStaticBrick(const bman::GameConfig& config, int x, int y) {
    if (x < 0 || y < 0 || x >= config.level_width() ||
        y >= config.level_height())
      return true;
    return (x % 2 == 1 && y % 2 == 1);
  }


protected:
  int32_t clock_ = 0;
  bman::GameConfig config_;
  bman::GameState game_state_;
};

#endif
