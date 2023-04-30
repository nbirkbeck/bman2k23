#ifndef GAME_RENDERER_H
#define GAME_RENDERER_H

#include "level.grpc.pb.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// A class to help manage the Player texture.
class PlayerTexture {
public:
  bool Load();
  void Draw(const bman::PlayerState& ps, SDL_Surface* dest);

private:
  SDL_Surface* surface_;
};

// A class to help manage the Explosion texture.
class ExplosionTexture {
public:
  bool Load();
  void Draw(const bman::LevelState::Explosion& explosion, SDL_Surface* dest);

private:
  SDL_Surface* surface_;
};

class GameRenderer {
public:
  bool Load();

  void Draw(const bman::GameState& game_state, SDL_Surface* dest);
  void DrawBackground(const bman::GameState& game, SDL_Surface* dest);
  void DrawBrick(const bman::LevelState::Brick& brick, SDL_Surface* dest);
  void DrawBomb(const bman::LevelState::Bomb& bomb, SDL_Surface* dest);

  void set_config(const bman::GameConfig& config) { config_ = config; }

private:
  bman::GameConfig config_;
  SDL_Surface* background_;
  SDL_Surface* bomb_;
  SDL_Surface* powerup_;
  PlayerTexture player_texture_;
  ExplosionTexture explosion_texture_;
  TTF_Font* font_;
};

#endif
