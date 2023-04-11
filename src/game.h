#ifndef _BMAN_GAME_H_
#define _BMAN_GAME_H_ 1

#include <glog/logging.h>

#include "level.grpc.pb.h"
#include <iostream>
#include <memory>
#include <unordered_map>

#include "constants.h"
#include "math.h"
#include "point.h"

struct PointHash {
  size_t operator()(const std::pair<int32_t, int32_t>& point) const {
    return point.first * 1024 + point.second;
  }
  size_t operator()(const Point2i& point) const {
    return point.x * 1024 + point.y;
  }
};

typedef std::unordered_map<Point2i, bman::LevelState::Bomb*, PointHash> BombMap;
typedef std::unordered_map<Point2i, bman::LevelState::Brick*, PointHash>
    BrickMap;

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

  bool Step(const std::vector<bman::MovePlayerRequest>& move_requests) {
    if ((int)move_requests.size() != (int)game_state_.players_size()) {
      LOG(ERROR) << "Move request has invalid number of players";
      return false;
    }
    // Build map of bombs and bricks
    BombMap bomb_map = MakeBombMap();
    BrickMap brick_map;
    for (auto& brick : *game_state_.mutable_level()->mutable_bricks()) {
      brick_map[Point2i(brick.x(), brick.y())] = &brick;
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

  const bman::GameConfig& config() const { return config_; }
  const bman::GameState& game_state() const { return game_state_; }
  bman::GameState& game_state() { return game_state_; }

  bool IsStaticBrick(int x, int y) const {
    return IsStaticBrick(config_, x, y);
  }

  static bool IsStaticBrick(const bman::GameConfig& config, int x, int y) {
    if (x < 0 || y < 0 || x >= config.level_width() ||
        y >= config.level_height())
      return true;
    return (x % 2 == 1 && y % 2 == 1);
  }

private:
  std::vector<bman::LevelState::Bomb>
  MovePlayers(const std::vector<bman::MovePlayerRequest>& move_requests,
              const BrickMap& brick_map, const BombMap& bomb_map) {
    std::vector<bman::LevelState::Bomb> new_bombs;
    int player_index = 0;
    for (auto& move : move_requests) {
      for (auto& action : move.actions()) {
        auto* player = game_state_.mutable_players(player_index);
        const Point2i delta(action.dx(), action.dy());
        Point2i min_delta(0, 0);
        int x = player->x(), y = player->y();
        Point2i other(0, 0);
        const Point2i cur(GridRound(x), GridRound(y));

        if (abs(delta.x)) {
          const int test_x =
              x + delta.x + sign(delta.x) * (kSubPixelSize / 2 - kMovePadding);
          other.x = GridRound(test_x);
          other.y = GridRound(y);
          min_delta.x =
              (delta.x > 0)
                  ? std::max(0, cur.x * kSubPixelSize + kSubPixelSize / 2 +
                                    kMovePadding - x)
                  : std::min(0, cur.x * kSubPixelSize + kSubPixelSize / 2 -
                                    kMovePadding - x);
        } else if (abs(delta.y)) {
          const int test_y =
              y + delta.y + sign(delta.y) * (kSubPixelSize / 2 - kMovePadding);
          other.y = GridRound(test_y);
          other.x = GridRound(x);
          min_delta.y =
              (delta.y > 0)
                  ? std::max(0, cur.y * kSubPixelSize + kSubPixelSize / 2 +
                                    kMovePadding - y)
                  : std::min(0, cur.y * kSubPixelSize + kSubPixelSize / 2 -
                                    kMovePadding - y);
        }
        const bool can_move =
            (!IsStaticBrick(other.x, other.y) &&
             (!brick_map.count(other) || !brick_map.at(other)->solid()) &&
             !bomb_map.count(other)) ||
            (other == cur);

        player->set_x(x + (can_move ? delta.x : min_delta.x));
        player->set_y(y + (can_move ? delta.y : min_delta.y));
        if (action.has_dir()) {
          player->set_anim_counter(player->anim_counter() + 1);
          player->set_dir(action.dir());
        } else {
          player->set_anim_counter(0);
        }

        // Move player closer to the square that they've moved into
        const Point2i new_pt(GridRound(player->x()), GridRound(player->y()));
        if (new_pt != cur) {
          if (abs(delta.x)) {
            const int d =
                player->y() - (new_pt.y * kSubpixelSize + kSubpixelSize / 2);
            player->set_y(player->y() + SignedMin(-d, delta.x));
          } else if (abs(delta.y)) {
            const int d =
                player->x() - (new_pt.x * kSubpixelSize + kSubpixelSize / 2);
            player->set_x(player->x() + SignedMin(-d, delta.y));
          }
        }

        // If player wants to place a bomb, do it
        if (action.place_bomb()) {
          if (!bomb_map.count(cur) &&
              (!brick_map.count(cur) || !brick_map.at(cur)->solid()))  {
            PlayerTryPlaceBomb(new_bombs, player, player_index, cur);
          }
        }

        // If player is over a power-up, give it to them.
        if (brick_map.count(new_pt)) {
          PlayerGivePowerup(player, brick_map.at(new_pt));
        }
      }
      player_index++;
    }
    return new_bombs;
  }

  void PlayerGivePowerup(bman::PlayerState* player,
                         bman::LevelState::Brick* brick) {
    if (!brick->solid()) {
      if (brick->has_powerup()) {
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
  void PlayerTryPlaceBomb(std::vector<bman::LevelState::Bomb>& new_bombs,
                          bman::PlayerState* player, int player_index,
                          const Point2i& cur) {
    if (player->num_used_bombs() < player->num_bombs()) {
      new_bombs.resize(new_bombs.size() + 1);
      auto& bomb = new_bombs.back();
      bomb.set_x(cur.x);
      bomb.set_y(cur.y);
      bomb.set_strength(player->strength());
      bomb.set_timer(kDefaultBombTimer + 1);
      bomb.set_player_id(player_index);
      player->set_num_used_bombs(player->num_used_bombs() + 1);
    }
  }

  BombMap MakeBombMap() {
    BombMap bomb_map;
    for (auto& bomb : *game_state_.mutable_level()->mutable_bombs()) {
      bomb_map[Point2i(bomb.x(), bomb.y())] = &bomb;
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
        auto point = Point2i(bomb->x() + dx * i, bomb->y() + dy * i);
        if (IsStaticBrick(point.x, point.y))
          break;

        // Track the point in the explosion.
        p = explosion->add_points();
        p->set_x(point.x);
        p->set_y(point.y);

        // Do damage to players
        for (auto& player : *game_state_.mutable_players()) {
          const int min_x = GridRound(player.x() - kSubpixelSize / 2);
          const int min_y = GridRound(player.y() - kSubpixelSize / 2);
          const int max_x = GridRound(player.x() + kSubpixelSize / 2 - 1);
          const int max_y = GridRound(player.y() + kSubpixelSize / 2 - 1);
          if (min_x <= point.x && point.x <= max_x && min_y <= point.y &&
              point.y <= max_y) {
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

protected:
  int32_t clock_ = 0;
  bman::GameConfig config_;
  bman::GameState game_state_;
};

#endif
