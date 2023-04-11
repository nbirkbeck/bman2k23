#include <SDL2/SDL.h>

#include "bman_client.h"
#include "game.h"
#include "level.grpc.pb.h"
#include "timer.h"
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <memory>
#include <unordered_map>

static constexpr int kScreenWidth = 640;
static constexpr int kScreenHeight = 480;
static constexpr int kGridSize = 32;
static constexpr int kOffsetX = 48;
static constexpr int kOffsetY = 32;

DEFINE_string(username, "[name]", "User name to use when connecting to server");
DEFINE_string(server, "", "Server to connect to (with :port)");
DEFINE_bool(stream, false, "Use streaming RPC");
DEFINE_int32(client_delay, 0, "Introduce latency in the client request");

// A class to help manage the Player texture.
class PlayerTexture {
public:
  bool Load() {
    surface_ = SDL_LoadBMP("data/man.bmp");
    SDL_SetColorKey(surface_, 1, 0);
    return true;
  }
  void Draw(const bman::PlayerState& ps, SDL_Surface* dest) {
    const int dir = static_cast<int>(ps.dir());
    int x_offset = (ps.anim_counter() / 8) % 4;
    int y_offset = 0;
    int size_increase_x = 0;
    int size_increase_y = 0;

    if (ps.state() == bman::PlayerState::STATE_DYING) {
      x_offset = (ps.anim_counter() / 8) % 3;
      y_offset = (ps.anim_counter() / 8) / 3 + 4;
      size_increase_x = ps.anim_counter() / 4;
      size_increase_y = ps.anim_counter() / 4;
    } else if (ps.state() == bman::PlayerState::STATE_SPAWNING) {
      size_increase_x = -(16 - 16 * ps.anim_counter() / kDyingTimer);
      size_increase_y = (128 - 128 * ps.anim_counter() / kDyingTimer);
    } else {
      y_offset = dir;
      if (x_offset == 2 || x_offset == 0)
        x_offset = 0;
      else if (x_offset == 3)
        x_offset--;
    }
    SDL_Rect src_rect = {kGridSize * x_offset, 2 * kGridSize * y_offset,
                         kGridSize, 2 * kGridSize};
    SDL_Rect dest_rect = {kOffsetX + ps.x() * kGridSize / kSubpixelSize -
                              kGridSize / 2 - size_increase_x,
                          kOffsetY + ps.y() * kGridSize / kSubpixelSize -
                              kGridSize / 2 - kGridSize - size_increase_y * 2,
                          kGridSize + size_increase_x * 2,
                          (kGridSize * 2 + size_increase_y * 4)};
    SDL_BlitScaled(surface_, &src_rect, dest, &dest_rect);
  }
  SDL_Surface* surface_;
};

// A class to help manage the Explosion texture.
class ExplosionTexture {
public:
  bool Load() {
    surface_ = SDL_LoadBMP("data/explosion.bmp");
    SDL_SetColorKey(surface_, 1, 0);
    return true;
  }
  void Draw(const bman::LevelState::Explosion& explosion, SDL_Surface* dest) {
    std::unordered_map<Point2i, int, PointHash> exp_map;
    for (const auto& p : explosion.points()) {
      exp_map[Point2i(p.x(), p.y())] = 1;
    }

    // Animate the explosion width using the timer
    int y_offset =
        (8 * (kExplosionTimer - explosion.timer())) / kExplosionTimer;
    if (y_offset >= 4) {
      y_offset = 7 - y_offset;
    }

    for (const auto& p : explosion.points()) {
      const int left = exp_map[Point2i(p.x() - 1, p.y())];
      const int right = exp_map[Point2i(p.x() + 1, p.y())];
      const int up = exp_map[Point2i(p.x(), p.y() - 1)];
      const int down = exp_map[Point2i(p.x(), p.y() + 1)];
      const int index = left | (right << 1) | (up << 2) | (down << 3);
      static const int mapping[16] = {
          2, 3, 0, 1, /* none, left, right, left+right */
          4, 2, 2, 2, /* up, up+left, up+right, up+left+right */
          5, 2, 2, 2, /* down, down+left, down+right, down+left+right*/
          6, 2, 2, 2, /* up+down, up+down+left, ... */
      };
      const int x_offset = p.bomb_center() ? 2 : mapping[index];

      SDL_Rect src_rect = {x_offset * kGridSize, y_offset * kGridSize,
                           kGridSize, kGridSize};
      SDL_Rect dest_rect = {kOffsetX + p.x() * kGridSize,
                            kOffsetY + p.y() * kGridSize, kGridSize, kGridSize};
      SDL_BlitScaled(surface_, &src_rect, dest, &dest_rect);
    }
  }
  SDL_Surface* surface_;
};

class GameRenderer {
public:
  bool Load() {
    background_ = SDL_LoadBMP("data/background.bmp");
    bomb_ = SDL_LoadBMP("data/bomb.bmp");
    powerup_ = SDL_LoadBMP("data/powerup.bmp");
    SDL_SetColorKey(bomb_, 1, 0);
    return player_texture_.Load() && explosion_texture_.Load();
  }

  void Draw(const bman::GameState& game_state, SDL_Surface* dest) {
    DrawBackground(game_state, dest);

    for (const auto& player : game_state.players()) {
      player_texture_.Draw(player, dest);
    }

    const auto& level = game_state.level();
    for (const auto& brick : level.bricks()) {
      DrawBrick(brick, dest);
    }

    for (const auto& bomb : level.bombs()) {
      DrawBomb(bomb, dest);
    }

    for (const auto& explosion : level.explosions()) {
      explosion_texture_.Draw(explosion, dest);
    }

    for (const auto& player : game_state.players()) {
      player_texture_.Draw(player, dest);
    }
  }

  void DrawBackground(const bman::GameState& game, SDL_Surface* dest) {
    SDL_BlitScaled(background_, nullptr, dest, nullptr);
    for (int y = 0; y < config_.level_height(); ++y) {
      for (int x = 0; x < config_.level_width(); ++x) {
        SDL_Rect src_rect = {kOffsetX + 64 * Game::IsStaticBrick(config_, x, y),
                             kOffsetY, kGridSize, kGridSize};
        SDL_Rect dest_rect = {kOffsetX + x * kGridSize,
                              kOffsetY + y * kGridSize, kGridSize, kGridSize};
        SDL_BlitScaled(background_, &src_rect, dest, &dest_rect);
      }
    }
  }

  void DrawBrick(const bman::LevelState::Brick& brick, SDL_Surface* dest) {
    SDL_Rect src_rect = {kOffsetX + 96, kGridSize, kGridSize, kGridSize};
    SDL_Rect dest_rect = {kOffsetX + brick.x() * kGridSize,
                          kOffsetY + brick.y() * kGridSize, kGridSize,
                          kGridSize};
    if (brick.solid()) {
      SDL_BlitScaled(background_, &src_rect, dest, &dest_rect);
    } else {
      if (brick.powerup() != bman::PUP_NONE) {
        const int y_index = 2 * static_cast<int>(brick.powerup() - 1);
        SDL_Rect pup_rect = {0, y_index * kGridSize, kGridSize, kGridSize};
        SDL_BlitScaled(powerup_, &pup_rect, dest, &dest_rect);
      }
    }
  }

  void DrawBomb(const bman::LevelState::Bomb& bomb, SDL_Surface* dest) {
    SDL_Rect src_rect = {kGridSize * ((bomb.timer() / 16) % 4), 0, kGridSize,
                         kGridSize};
    SDL_Rect dest_rect = {kOffsetX + bomb.x() * kGridSize,
                          kOffsetY + bomb.y() * kGridSize, kGridSize,
                          kGridSize};
    SDL_BlitScaled(bomb_, &src_rect, dest, &dest_rect);
  }

  void set_config(const bman::GameConfig& config) { config_ = config; }

private:
  bman::GameConfig config_;
  SDL_Surface* background_;
  SDL_Surface* bomb_;
  SDL_Surface* powerup_;
  PlayerTexture player_texture_;
  ExplosionTexture explosion_texture_;
};

class GameWindow {
public:
  GameWindow() {
    const int renderer_flags = SDL_RENDERER_SOFTWARE, window_flags = 0;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
      LOG(FATAL) << "Couldn't initialize SDL:" << SDL_GetError();
    }

    window_ = SDL_CreateWindow("Shooter 01", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, kScreenWidth,
                               kScreenHeight, window_flags);

    if (!window_) {
      LOG(FATAL) << "Failed to open window:" << SDL_GetError();
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    sdl_renderer_ = SDL_CreateRenderer(window_, -1, renderer_flags);

    if (!sdl_renderer_) {
      LOG(FATAL) << "Failed to create renderer:" << SDL_GetError();
    }

    if (!game_renderer_.Load()) {
      LOG(FATAL) << "Failed to load the renderer";
    }

    if (!FLAGS_server.empty()) {
      client_ = bman::Client::Create(FLAGS_server, FLAGS_client_delay);
      auto response = client_->Join(FLAGS_username);
      game_renderer_.set_config(response.game_config());
    } else {
      game_.BuildSimpleLevel(2);
      game_.AddPlayer();
      game_renderer_.set_config(game_.config());
    }
  }

  void Loop() {
    while (true) {
      bman::Timer timer;
      HandleInput();

      // Process input to get user action.
      std::vector<bman::MovePlayerRequest> moves = {
          GetPlayerActionFromKeyboard()};

      // Send the move to server (or advance local game) and get
      // game state so we can render it.
      bman::GameState state;
      if (client_) {
        state = FLAGS_stream
                    ? client_->StreamingMovePlayer(moves[0]).game_state()
                    : client_->MovePlayer(moves[0]).game_state();
      } else {
        game_.Step(moves);
        state = game_.game_state();
      }

      game_renderer_.Draw(state, SDL_GetWindowSurface(window_));
      SDL_UpdateWindowSurface(window_);

      timer.Wait(1000 / 60);
    }
  }

  bman::MovePlayerRequest GetPlayerActionFromKeyboard() {
    char move_keys[5] = {'a', 'd', 'w', 's', ' '};
    int dir = -1;
    for (int i = 0; i < 4; ++i) {
      if (keys_[move_keys[i]]) {
        dir = i;
        break;
      }
    }

    bman::MovePlayerRequest move;
    auto* action = move.add_actions();
    if (dir >= 0) {
      int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
      int speed = 4;
      action->set_dir(static_cast<bman::Direction>(dir));
      action->set_dx(speed * dirs[dir][0]);
      action->set_dy(speed * dirs[dir][1]);
    }
    if (keys_[move_keys[4]]) {
      action->set_place_bomb(true);
      keys_[move_keys[4]] = 0;
    }
    return move;
  }

  // Read and process any pending SDL events
  int HandleInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event))
      switch (event.type) {
      case SDL_QUIT:
        exit(0);
        break;

      case SDL_KEYDOWN: {
        SDL_Keysym keysym = event.key.keysym;
        keys_[keysym.sym] = true;
        return 0;
      }

      case SDL_KEYUP: {
        SDL_Keysym keysym = event.key.keysym;
        keys_[keysym.sym] = false;
        return 0;
      }
      default:
        break;
      }
    return 0;
  }

private:
  SDL_Renderer* sdl_renderer_;
  SDL_Window* window_;
  GameRenderer game_renderer_;
  Game game_;
  std::unique_ptr<bman::Client> client_;
  std::unordered_map<int, int> keys_;
};

int main(int ac, char* av[]) {
  gflags::ParseCommandLineFlags(&ac, &av, true);

  GameWindow window;
  window.Loop();
  return 0;
}
