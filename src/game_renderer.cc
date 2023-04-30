#include "game_renderer.h"

#include "constants.h"
#include "game.h"
#include "point.h"

// A class to help manage the Player texture.
bool PlayerTexture::Load() {
  surface_ = SDL_LoadBMP("data/man.bmp");
  SDL_SetColorKey(surface_, 1, 0);
  return true;
}

void PlayerTexture::Draw(const bman::PlayerState& ps, SDL_Surface* dest) {
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

// A class to help manage the Explosion texture.
bool ExplosionTexture::Load() {
  surface_ = SDL_LoadBMP("data/explosion.bmp");
  SDL_SetColorKey(surface_, 1, 0);
  return true;
}

void ExplosionTexture::Draw(const bman::LevelState::Explosion& explosion,
                            SDL_Surface* dest) {
  std::unordered_map<Point2i, int, PointHash> exp_map;
  for (const auto& p : explosion.points()) {
    exp_map[Point2i(p.x(), p.y())] = 1;
  }

  // Animate the explosion width using the timer
  int y_offset = (8 * (kExplosionTimer - explosion.timer())) / kExplosionTimer;
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

    SDL_Rect src_rect = {x_offset * kGridSize, y_offset * kGridSize, kGridSize,
                         kGridSize};
    SDL_Rect dest_rect = {kOffsetX + p.x() * kGridSize,
                          kOffsetY + p.y() * kGridSize, kGridSize, kGridSize};
    SDL_BlitScaled(surface_, &src_rect, dest, &dest_rect);
  }
}

bool GameRenderer::Load() {
  background_ = SDL_LoadBMP("data/background.bmp");
  bomb_ = SDL_LoadBMP("data/bomb.bmp");
  powerup_ = SDL_LoadBMP("data/powerup.bmp");
  font_ = TTF_OpenFont("data/Vera.ttf", 14);
  SDL_SetColorKey(bomb_, 1, 0);
  return player_texture_.Load() && explosion_texture_.Load();
}

void GameRenderer::Draw(const bman::GameState& game_state, SDL_Surface* dest) {
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
  // Draw the scores
  int y = 8, x = 32;
  for (const auto& score : game_state.score()) {
    SDL_Color color = {200, 200, 200, 255};
    char message[64];
    snprintf(message, sizeof(message), "score: %d", score);

    SDL_Surface* surface_message = TTF_RenderText_Solid(font_, message, color);
    SDL_Rect rect = {x, y, 10, 100};
    SDL_BlitSurface(surface_message, nullptr, dest, &rect);

    SDL_FreeSurface(surface_message);
    x += 64;
  }
}

void GameRenderer::DrawBackground(const bman::GameState& game,
                                  SDL_Surface* dest) {
  SDL_BlitScaled(background_, nullptr, dest, nullptr);
  for (int y = 0; y < config_.level_height(); ++y) {
    for (int x = 0; x < config_.level_width(); ++x) {
      SDL_Rect src_rect = {kOffsetX + 64 * Game::IsStaticBrick(config_, x, y),
                           kOffsetY, kGridSize, kGridSize};
      SDL_Rect dest_rect = {kOffsetX + x * kGridSize, kOffsetY + y * kGridSize,
                            kGridSize, kGridSize};
      SDL_BlitScaled(background_, &src_rect, dest, &dest_rect);
    }
  }
}

void GameRenderer::DrawBrick(const bman::LevelState::Brick& brick,
                             SDL_Surface* dest) {
  SDL_Rect src_rect = {kOffsetX + 96, kGridSize, kGridSize, kGridSize};
  SDL_Rect dest_rect = {kOffsetX + brick.x() * kGridSize,
                        kOffsetY + brick.y() * kGridSize, kGridSize, kGridSize};
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

void GameRenderer::DrawBomb(const bman::LevelState::Bomb& bomb,
                            SDL_Surface* dest) {
  SDL_Rect src_rect = {kGridSize * ((bomb.timer() / 16) % 4), 0, kGridSize,
                       kGridSize};
  SDL_Rect dest_rect = {
      bomb.has_moving_x()
          ? (kOffsetX + bomb.moving_x() * kGridSize / kSubpixelSize -
             kGridSize / 2)
          : (kOffsetX + bomb.x() * kGridSize),
      bomb.has_moving_y()
          ? (kOffsetY + bomb.moving_y() * kGridSize / kSubpixelSize -
             kGridSize / 2)
          : (kOffsetY + bomb.y() * kGridSize),
      kGridSize, kGridSize};

  SDL_BlitScaled(bomb_, &src_rect, dest, &dest_rect);
}
