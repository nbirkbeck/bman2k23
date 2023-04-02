#include <SDL2/SDL.h>

#include "game.h"
#include "bman_client.h"
#include "level.grpc.pb.h"
#include <iostream>
#include <memory>
#include <unordered_map>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

const int kGridSize = 32;
const int kOffsetX = 48;
const int kOffsetY = 32;
std::unordered_map<int, int> keys;

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
    if (x_offset == 2 || x_offset == 0)
      x_offset = 0;
    else if (x_offset == 3)
      x_offset--;
    SDL_Rect src_rect = {32 * x_offset, 64 * dir, kGridSize, 64};
    SDL_Rect dest_rect = {kOffsetX + ps.x() * kGridSize / kSubpixelSize -
                              kGridSize / 2,
                          kOffsetY + ps.y() * kGridSize / kSubpixelSize -
                              kGridSize / 2 - kGridSize,
                          kGridSize, 64};
    SDL_BlitScaled(surface_, &src_rect, dest, &dest_rect);
  }
  SDL_Surface* surface_;
};

class ExplosionTexture {
public:
  bool Load() {
    surface_ = SDL_LoadBMP("data/explosion.bmp");
    SDL_SetColorKey(surface_, 1, 0);
    return true;
  }
  void Draw(const bman::LevelState::Explosion& explosion, SDL_Surface* dest) {
    std::unordered_map<std::pair<int32_t, int32_t>, bool, PointHash> exp_map;
    for (const auto& p : explosion.points()) {
      exp_map[std::make_pair(p.x(), p.y())] = 1;
    }

    int y_offset =
        (8 * (kExplosionTimer - explosion.timer())) / kExplosionTimer;
    if (y_offset >= 4) {
      y_offset = 7 - y_offset;
    }

    for (const auto& p : explosion.points()) {
      const bool left = exp_map[std::make_pair(p.x() - 1, p.y())];
      const bool right = exp_map[std::make_pair(p.x() + 1, p.y())];
      const bool up = exp_map[std::make_pair(p.x(), p.y() - 1)];
      const bool down = exp_map[std::make_pair(p.x(), p.y() + 1)];
      const bool corner = (left || right) && (up || down);
      int x_offset = 2;
      if (p.bomb_center() || corner) {
        x_offset = 2;
      } else if (left && right) {
        x_offset = 1;
      } else if (right) {
        x_offset = 0;
      } else if (left) {
        x_offset = 3;
      } else if (up && down) {
        x_offset = 6;
      } else if (up) {
        x_offset = 4;
      } else if (down) {
        x_offset = 5;
      }
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
        SDL_Rect src_rect = {kOffsetX + 64 * Game::IsStaticBrick(config_, x, y), kOffsetY,
                             kGridSize, kGridSize};
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
    SDL_Rect src_rect = {0 + kGridSize * ((bomb.timer() / 16) % 4), 0,
                         kGridSize, kGridSize};
    SDL_Rect dest_rect = {kOffsetX + bomb.x() * kGridSize,
                          kOffsetY + bomb.y() * kGridSize, kGridSize,
                          kGridSize};
    SDL_BlitScaled(bomb_, &src_rect, dest, &dest_rect);
  }

  void set_config(const bman::GameConfig& config) {
    config_ = config;
  }

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
      printf("Couldn't initialize SDL: %s\n", SDL_GetError());
      exit(1);
    }

    window_ = SDL_CreateWindow("Shooter 01", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
                               SCREEN_HEIGHT, window_flags);

    if (!window_) {
      printf("Failed to open %d x %d window: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT,
             SDL_GetError());
      exit(1);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    sdl_renderer_ = SDL_CreateRenderer(window_, -1, renderer_flags);

    if (!sdl_renderer_) {
      printf("Failed to create renderer: %s\n", SDL_GetError());
      exit(1);
    }
    game_renderer_.Load();
    
    client_ = bman::Client::Create();
    if (client_) {
      auto response = client_->Join("user");
      game_renderer_.set_config(response.game_config());
    } else {
      game_.BuildSimpleLevel(2);
      game_.AddPlayer();
      game_renderer_.set_config(game_.config());
    }
  }

  void Loop() {
    while (true) {
      HandleInput();

      // Process input
      char move_keys[5] = {'a', 'd', 'w', 's', ' '};
      int dir = -1;
      for (int i = 0; i < 4; ++i) {
        if (keys[move_keys[i]]) {
          dir = i;
          break;
        }
      }
      int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

      std::vector<bman::MovePlayerRequest> moves(1);
      auto* action = moves[0].add_actions();
      if (dir >= 0) {
        int speed = 4;
        action->set_dir(static_cast<bman::Direction>(dir));
        action->set_dx(speed * dirs[dir][0]);
        action->set_dy(speed * dirs[dir][1]);
      }
      if (keys[move_keys[4]]) {
        action->set_place_bomb(true);
        keys[move_keys[4]] = 0;
      }
      bman::GameState state;
      if (client_) {
        state = client_->MovePlayer(moves[0]).game_state();
      } else {
        game_.Step(moves);
        state = game_.game_state();
      }
      
      game_renderer_.Draw(state, SDL_GetWindowSurface(window_));
      SDL_UpdateWindowSurface(window_);

      usleep(1000 / 60.0 * 1000);
    }
  }

  int HandleInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event))
      switch (event.type) {
      case SDL_QUIT:
        exit(0);
        break;

      case SDL_KEYDOWN: {
        SDL_Keysym keysym = event.key.keysym;
        keys[keysym.sym] = true;
        return 0;
      }

      case SDL_KEYUP: {
        SDL_Keysym keysym = event.key.keysym;
        keys[keysym.sym] = false;
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
};

int main(int ac, char* av[]) {
  GameWindow window;
  window.Loop();
  return 0;
}
