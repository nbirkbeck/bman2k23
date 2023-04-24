#ifndef _BMAN_GAME_H_
#define _BMAN_GAME_H_ 1

#include <glog/logging.h>

#include "level.grpc.pb.h"
#include <iostream>
#include <memory>
#include <unordered_map>

#include "constants.h"
#include "grid_map.h"
#include "math.h"
#include "point.h"
#include "types.h"

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
    if (true) {
      auto* brick = level_state->add_bricks();
      brick->set_x(1);
      brick->set_y(0);
      brick->set_powerup(bman::PUP_KICK);

      brick = level_state->add_bricks();
      brick->set_x(0);
      brick->set_y(1);
      brick->set_powerup(bman::PUP_DETONATOR);

      brick = level_state->add_bricks();
      brick->set_x(0);
      brick->set_y(3);
      brick->set_powerup(bman::PUP_DEATH);
    }

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
    int player_index = game_state_.players_size();
    auto* player = game_state_.add_players();
    const Point2i point = GetSpawnPoint(player_index);
    player->set_x(point.x);
    player->set_y(point.y);
    player->set_num_bombs(config_.player_config().num_bombs());
    player->set_health(config_.player_config().health());
    player->set_strength(config_.player_config().strength());
  }

  Point2i GetSpawnPoint(int player_index) {
    if (player_index % 4 == 0) {
      return Point2i(kSubpixelSize / 2, kSubpixelSize / 2);
    } else if (player_index % 4 == 1) {
      return Point2i(config_.level_width() * kSubpixelSize - kSubpixelSize / 2,
                     config_.level_height() * kSubpixelSize -
                         kSubpixelSize / 2);
    } else if (player_index % 4 == 2) {
      return Point2i(kSubpixelSize / 2, config_.level_height() * kSubpixelSize -
                                            kSubpixelSize / 2);
    }
    return Point2i(config_.level_width() * kSubpixelSize - kSubpixelSize / 2,
                   kSubpixelSize / 2);
  }

  bool Step(const std::vector<bman::MovePlayerRequest>& move_requests) {
    if ((int)move_requests.size() != (int)game_state_.players_size()) {
      LOG(ERROR) << "Move request has invalid number of players";
      return false;
    }
    // Build map of bombs and bricks
    BombMap bomb_map = MakeBombMap(game_state_);
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
      bomb_map = MakeBombMap(game_state_);
    }
    GridMap grid_map(config_, game_state_);

    // Decrement timer on any active explosions
    for (auto& explosion : *game_state_.mutable_level()->mutable_explosions()) {
      explosion.set_timer(explosion.timer() - 1);
      // Do damage to any player.
      if (explosion.timer() > 0) {
        for (int player_index = 0; player_index < game_state_.players_size();
             player_index++) {
          auto* player = game_state_.mutable_players(player_index);
          Point2i player_pos(GridRound(player->x()), GridRound(player->y()));
          for (const auto& point : explosion.points()) {
            if (Point2i(point.x(), point.y()) == player_pos) {
              MaybeDoDamage(*player, player_index, explosion.player_id());
            }
          }
        }
      }
    }

    // Go through any bombs and decrement timer.
    for (auto& bomb : *game_state_.mutable_level()->mutable_bombs()) {
      const bool active = bomb.timer() > 0;
      bomb.set_timer(bomb.timer() - 1);
      if (active && bomb.timer() <= 0) {
        auto* explosion = game_state_.mutable_level()->add_explosions();
        explosion->set_player_id(bomb.player_id());
        ExplodeBomb(bomb_map, brick_map, &bomb, explosion);
      } else if (active && bomb.has_dir()) {

        // This should use all the possible reasons that grid point could be
        // fixed.
        Point2i next_point(
            GridRound(bomb.moving_x() +
                      (kSubpixelSize / 2 + 1) * kDirs[bomb.dir()][0]),
            GridRound(bomb.moving_y() +
                      (kSubPixelSize / 2 + 1) * kDirs[bomb.dir()][1]));
        Point2i cur_point(bomb.x(), bomb.y());
        if (next_point == cur_point || grid_map.CanMove(next_point)) {
          bomb.set_moving_x(bomb.moving_x() +
                            kBombSpeed * kDirs[bomb.dir()][0]);
          bomb.set_moving_y(bomb.moving_y() +
                            kBombSpeed * kDirs[bomb.dir()][1]);
          bomb.set_x(GridRound(bomb.moving_x()));
          bomb.set_y(GridRound(bomb.moving_y()));
        } else {
          bomb.clear_moving_x();
          bomb.clear_moving_y();
          bomb.clear_dir();
        }
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

  static BombMap MakeBombMap(bman::GameState& game_state) {
    BombMap bomb_map = {};
    for (auto& bomb : *game_state.mutable_level()->mutable_bombs()) {
      bomb_map[Point2i(bomb.x(), bomb.y())] = &bomb;
    }
    return bomb_map;
  }
  static BombMapConst MakeBombMap(const bman::GameState& game_state) {
    BombMapConst bomb_map = {};
    for (const auto& bomb : game_state.level().bombs()) {
      bomb_map[Point2i(bomb.x(), bomb.y())] = &bomb;
    }
    return bomb_map;
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

        if (player->state() == bman::PlayerState::STATE_DYING) {
          player->set_anim_counter(player->anim_counter() + 1);
          if (player->anim_counter() >= kDyingTimer) {
            player->set_anim_counter(0);
            player->set_state(bman::PlayerState::STATE_SPAWNING);
            Point2i pt = GetSpawnPoint(player_index);
            player->set_x(pt.x);
            player->set_y(pt.y);
          }
          continue;
        } else if (player->state() == bman::PlayerState::STATE_SPAWNING) {
          player->set_anim_counter(player->anim_counter() + 1);
          if (player->anim_counter() >= kDyingTimer) {
            player->set_anim_counter(0);
            player->set_state(bman::PlayerState::STATE_ALIVE);
            player->set_health(1); // TODO(birkbeck): Set from world
          }
          continue;
        }

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
              (!brick_map.count(cur) || !brick_map.at(cur)->solid())) {
            PlayerTryPlaceBomb(new_bombs, player, player_index, cur);
          }
        }

        // If player is over a power-up, give it to them.
        if (brick_map.count(new_pt)) {
          PlayerGivePowerup(player, player_index, brick_map.at(new_pt));
        }

        // If player is using power-up, do it.
        if (action.use_powerup()) {
          PlayerUsePowerup(player, player_index, bomb_map);
        }
      }
      player_index++;
    }
    return new_bombs;
  }

  void PlayerUsePowerup(
      bman::PlayerState* player, int player_index,
      const BombMap& bomb_map) { // TODO(birkbeck): make it non-const
    if (player->powerup() == bman::PUP_KICK) {
      bman::LevelState::Bomb* bomb = nullptr;
      Point2i cur(GridRound(player->x()), GridRound(player->y()));
      if (bomb_map.count(cur)) {
        bomb = const_cast<bman::LevelState::Bomb*>(bomb_map.at(cur));
      } else {
        Point2i adj_point(GridRound(player->x() + kDirs[player->dir()][0] *
                                                      kSubpixelSize / 2),
                          GridRound(player->y() + kDirs[player->dir()][1] *
                                                      kSubpixelSize / 2));
        if (bomb_map.count(adj_point)) {
          bomb = const_cast<bman::LevelState::Bomb*>(bomb_map.at(adj_point));
        }
      }
      if (bomb) {
        bomb->set_dir(player->dir());
        bomb->set_moving_x(bomb->x() * kSubpixelSize + kSubpixelSize / 2);
        bomb->set_moving_y(bomb->y() * kSubpixelSize + kSubpixelSize / 2);
      }
    } else if (player->powerup() == bman::PUP_DETONATOR) {
      // Find the players earliest bomb and explode it.
      for (auto& bomb : *game_state_.mutable_level()->mutable_bombs()) {
        if (bomb.player_id() == player_index) {
          bomb.set_timer(1);
          break;
        }
      }
    }
  }

  void PlayerGivePowerup(bman::PlayerState* player, int player_index,
                         bman::LevelState::Brick* brick) {
    if (!brick->solid()) {
      if (brick->has_powerup()) {
        game_state_.set_score(player_index,
                              game_state_.score(player_index) + kPointsPowerUp);
        switch (brick->powerup()) {
        case bman::PUP_NONE:
          break;
        case bman::PUP_DEATH:
          MaybeDoDamage(*player, player_index);
          break;
        case bman::PUP_EXTRA_BOMB:
          player->set_num_bombs(player->num_bombs() + 1);
          break;
        case bman::PUP_FLAME:
          player->set_strength(player->strength() + 1);
          break;
        case bman::PUP_KICK:
          player->set_powerup(bman::PUP_KICK);
          break;
        case bman::PUP_DETONATOR:
          player->set_powerup(bman::PUP_DETONATOR);
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
        int player_index = 0;
        for (auto& player : *game_state_.mutable_players()) {
          const int min_x = GridRound(player.x());
          const int min_y = GridRound(player.y());
          const int max_x = min_x;
          const int max_y = min_y;
          if (min_x <= point.x && point.x <= max_x && min_y <= point.y &&
              point.y <= max_y) {
            MaybeDoDamage(player, player_index, bomb->player_id());
          }
          player_index++;
        }

        // Damage world.
        if (brick_map.count(point) && brick_map[point]->solid()) {
          game_state_.set_score(bomb->player_id(),
                                game_state_.score(bomb->player_id()) +
                                    kPointsBrick);
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

  void MaybeDoDamage(bman::PlayerState& player, int player_index,
                     int bomb_player_id = -1) {
    if (player.health() > 0) {
      player.set_health(player.health() - 1);
      if (player.health() <= 0) {
        LOG(INFO) << "Player " << player_index << " is dead\n";
        player.set_state(bman::PlayerState::STATE_DYING);
        player.set_anim_counter(0);

        if (bomb_player_id == -1 || bomb_player_id == player_index) {
          game_state_.set_score(player_index,
                                game_state_.score(player_index) - kPointsKill);
        } else {
          game_state_.set_score(
              bomb_player_id, game_state_.score(bomb_player_id) + kPointsKill);
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
