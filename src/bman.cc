#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "agent.h"
#include "bman_client.h"
#include "constants.h"
#include "game_renderer.h"
#include "game.h"
#include "glog/logging.h"
#include "level.grpc.pb.h"
#include "simple_agent.h"
#include "timer.h"

#include <gflags/gflags.h>
#include <iostream>
#include <memory>
#include <unordered_map>


DEFINE_string(agent, "", "Name of agent to use");
DEFINE_string(username, "[name]", "User name to use when connecting to server");
DEFINE_string(server, "", "Server to connect to (with :port)");
DEFINE_bool(stream, false, "Use streaming RPC");
DEFINE_int32(client_delay, 0, "Introduce latency in the client request");

class UserAgent : public Agent {
public:
  UserAgent(std::unordered_map<int, int>& keys) : keys_(keys) {}

  bman::MovePlayerRequest GetPlayerAction(const bman::GameState&) {
    char move_keys[6] = {'a', 'd', 'w', 's', ' ', 'f'};
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
      int dx = 0, dy = 0;
      Agent::GetDeltaFromDir(dir, &dx, &dy);
      action->set_dir(static_cast<bman::Direction>(dir));
      action->set_dx(dx);
      action->set_dy(dy);
    }
    if (keys_[move_keys[4]]) {
      action->set_place_bomb(true);
      keys_[move_keys[4]] = 0;
    }
    if (keys_[move_keys[5]]) {
      LOG(INFO) << "Using powerup";
      action->set_use_powerup(true);
      keys_[move_keys[5]] = 0;
    }
    return move;
  }

private:
  std::unordered_map<int, int>& keys_;
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

    bman::GameConfig config;
    int player_index = 0;
    if (!FLAGS_server.empty()) {
      client_ = bman::Client::Create(FLAGS_server, FLAGS_client_delay);
      auto response = client_->Join(FLAGS_username);
      config = response.game_config();
      player_index = response.player_index();
    } else {
      game_.BuildSimpleLevel(2);
      game_.AddPlayer();
      config = game_.config();
    }
    game_renderer_.set_config(config);

    if (FLAGS_agent.empty()) {
      agent_.reset(new UserAgent(keys_));
    } else {
      agent_.reset(new SimpleAgent(config, player_index));
    }
  }

  void Loop() {
    bman::GameState state;

    while (true) {
      bman::Timer timer;
      HandleInput();

      // Process input to get user action.
      std::vector<bman::MovePlayerRequest> moves = {
          agent_->GetPlayerAction(state)};
      // Send the move to server (or advance local game) and get
      // game state so we can render it.
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
  std::unique_ptr<Agent> agent_;
};

int main(int ac, char* av[]) {
  gflags::ParseCommandLineFlags(&ac, &av, true);

  GameWindow window;
  window.Loop();
  return 0;
}
