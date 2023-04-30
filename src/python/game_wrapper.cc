#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "agent.h"
#include "game.h"
#include "game_renderer.h"
#include "timer.h"

namespace py = pybind11;

class Map {
public:
  Map(const Map& m): w_(m.w_), h_(m.h_), expand_(m.expand_), data_(m.data_) {}

  Map(const bman::GameConfig& config, const bman::GameState& game_state) {
    int expand = 3;
    w_ = config.level_width();
    h_ = config.level_height();
    data_.resize(w_ * h_ * expand * expand);
    GridMap gm(config, game_state);
    for (int y = 0; y < h_; ++y) {
      for (int x = 0; x < w_; ++x) {
        Point2i pt(x, y);
        float value = 0;
        if (gm.HasSolidBrick(pt)) {
          value = 0.75;
        } else if (gm.HasBomb(pt)) {
          value = -0.5;
        } else if (gm.IsExplosion(pt)) {
          value = -1.0;
        } else if (gm.HasPowerup(pt)) {
          value = 1.0;
        } else if (!gm.CanMove(pt)) {
          value = 0.5;
        }
        for (int i = 0; i < expand; ++i) {
          for (int j = 0; j < expand; ++j) {
            data_[(y * expand + i) * w_ * expand + (x * expand + j)] = value;
          }
        }
      }
    }
    if (game_state.players_size() > 0) {
      const auto& player = game_state.players(0);
      Point2i pt(GridRound(player.x() * expand), GridRound(player.y() * expand));
      data_[pt.y * w_ * expand + pt.x] = 0.25;
    }
    expand_ = expand;
  }
  const int w() const { return w_ * expand_; }
  const int h() const { return h_ * expand_; }
  const std::vector<float>& data() const { return data_; }

private:
  int w_, h_, expand_;
  std::vector<float> data_;
};

class GameWrapper {
public:
  GameWrapper() {}

  void BuildSimpleLevel(int n) {
    game_.BuildSimpleLevel(n);
    game_.AddPlayer();
    initial_state_ = game_.game_state();
  }

  void Reset() {
    game_.set_game_state(initial_state_);
  }

  bool PlayerIsDead() const {
    return game_.game_state().players(0).health() <= 0;
  }

  std::vector<float> pos() const {
    std::vector<float> p(2);
    p[0] = game_.game_state().players(0).x();
    p[1] = game_.game_state().players(0).y();
    return p;
  }

  float num_bombs() const {
    return game_.game_state().players(0).num_bombs() - game_.game_state().players(0).num_used_bombs();
  }

  int GetScore(int player_index) {
    if (player_index >= game_.game_state().score_size()) return 0;
    return game_.game_state().score(player_index);
  }

  Map GetMap() const {
    Map m(game_.config(), game_.game_state());
    return m;
  }

  bool MoveAgent(int dir, bool place_bomb, bool use_powerup) {
    std::vector<bman::MovePlayerRequest> move_requests(1);

    bman::MovePlayerRequest move;
    auto* action = move.add_actions();
    if (dir >= 0 && dir < 4) {
      int dx = 0, dy = 0;
      Agent::GetDeltaFromDir(dir, &dx, &dy);
      action->set_dir(static_cast<bman::Direction>(dir));
      action->set_dx(dx);
      action->set_dy(dy);
    }
    if (place_bomb) {
      action->set_place_bomb(true);
    }
    if (use_powerup) {
      action->set_use_powerup(true);
    }
    move_requests[0] = move;
    bool ret = true;
    for (int i = 0; i < 8; ++i) {
      ret &= game_.Step(move_requests);
      move_requests[0].mutable_actions(0)->set_place_bomb(false);
    }
    return ret;
  }

  const bman::GameState& game_state() const {
    return game_.game_state();
  }

  const bman::GameConfig& config() const {
    return game_.config();
  }

public:
  Game game_;
  bman::GameState initial_state_;
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
    if (TTF_Init() == -1) {
      LOG(FATAL) << "Failed to init TTF";
    }
    if (!game_renderer_.Load()) {
      LOG(FATAL) << "Failed to load the renderer";
    }
  }

  void DrawGame(const GameWrapper& game) {
    bman::Timer timer;

    game_renderer_.set_config(game.config());
    HandleInput();
    game_renderer_.Draw(game.game_state(), SDL_GetWindowSurface(window_));
    SDL_UpdateWindowSurface(window_);

    timer.Wait(4 * 1000 / 60);
  }

  // Read and process any pending SDL events
  int HandleInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        exit(0);
        break;
      default:
        break;
      }
    }
    return 0;
  }

private:
  SDL_Renderer* sdl_renderer_;
  SDL_Window* window_;
  GameRenderer game_renderer_;
};

PYBIND11_MODULE(game_wrapper, m) {
  py::class_<GameWrapper>(m, "GameWrapper")
    .def(py::init<>())
    .def("build_simple_level", &GameWrapper::BuildSimpleLevel)
    .def("get_score", &GameWrapper::GetScore)
    .def("get_map", &GameWrapper::GetMap)
    .def("reset", &GameWrapper::Reset)
    .def("player_is_dead", &GameWrapper::PlayerIsDead)
    .def("move_agent", &GameWrapper::MoveAgent)
    .def("pos", &GameWrapper::pos)
    .def("num_bombs", &GameWrapper::num_bombs);

  py::class_<Map>(m, "Map")
    .def("w", &Map::w)
    .def("h", &Map::h)
    .def("data", &Map::data);

  py::class_<GameWindow>(m, "GameWindow")
    .def(py::init<>())
    .def("draw_game", &GameWindow::DrawGame);
}
