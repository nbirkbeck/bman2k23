#include <iostream>
#include <memory>
#include <string>

#include "absl/strings/str_format.h"

#include "level.grpc.pb.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "game.h"
#include <memory>
#include <pthread.h>

DEFINE_int32(port, 8888, "Count of items to process");

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using bman::JoinRequest;
using bman::JoinResponse;
using bman::MovePlayerRequest;
using bman::MovePlayerResponse;

class GameRunner {
public:
  GameRunner() {
    pthread_mutex_init(&game_mutex_, nullptr);
    pthread_mutex_init(&request_mutex_, nullptr);
  }
  ~GameRunner() { stopped_ = true; }

  void Start() {
    game_.BuildSimpleLevel(2);
    pthread_create(&thread_, nullptr, &GameRunner::StaticLoop, this);
  }
  void Loop() {
    while (!stopped_) {
      if (game_.game_state().players_size() > 0) {
        std::vector<int> request_times(game_.game_state().players_size());
        std::vector<bman::MovePlayerRequest> move_requests(
            game_.game_state().players_size());

        pthread_mutex_lock(&request_mutex_);
        for (int i = 0; i < (int)pending_requests_.size(); ++i) {
          move_requests[i] = pending_requests_[i];
          request_times[i] =
              std::max(client_times_[i], pending_requests_[i].client_clock());
          pending_requests_[i].Clear();
        }
        pthread_mutex_unlock(&request_mutex_);

        pthread_mutex_lock(&game_mutex_);
        game_.Step(move_requests);
        client_times_ = request_times;
        pthread_mutex_unlock(&game_mutex_);
      }
      usleep(1000 / 60.0 * 1000);
    }
  }
  static void* StaticLoop(void* arg) {
    GameRunner* runner = (GameRunner*)arg;
    runner->Loop();
    return nullptr;
  }

  bman::GameConfig AddPlayer() {
    pthread_mutex_lock(&game_mutex_);
    auto config = game_.config();
    game_.AddPlayer();
    client_times_.push_back(0);
    pthread_mutex_unlock(&game_mutex_);
    return config;
  }

  int GetState(int player_index, bman::GameState* game_state) {
    int client_time = 0;
    pthread_mutex_lock(&game_mutex_);
    *game_state = game_.game_state();
    if (player_index >= 0 && player_index < (int32_t)client_times_.size())
      client_time = client_times_[player_index];
    pthread_mutex_unlock(&game_mutex_);
    return client_time;
  }

  void PushRequest(const bman::MovePlayerRequest& request) {
    pthread_mutex_lock(&request_mutex_);
    if ((int)request.player_index() >= (int)pending_requests_.size()) {
      pending_requests_.resize(request.player_index() + 1);
    }

    pending_requests_[request.player_index()].set_player_index(
        request.player_index());
    pending_requests_[request.player_index()].set_client_clock(
        std::max(pending_requests_[request.player_index()].client_clock(),
                 request.client_clock()));

    for (const auto& a : request.actions()) {
      *pending_requests_[request.player_index()].add_actions() = a;
    }
    pthread_mutex_unlock(&request_mutex_);
  }

  bool stopped_ = false;
  pthread_t thread_;
  pthread_mutex_t game_mutex_;
  pthread_mutex_t request_mutex_;
  Game game_;
  std::vector<int> client_times_;
  std::vector<bman::MovePlayerRequest> pending_requests_;
};

class BManServiceImpl final : public bman::BManService::Service {
public:
  BManServiceImpl(const std::string&) { num_clients_ = 0; }

  Status Join(ServerContext* context, const JoinRequest* request,
              JoinResponse* reply) override {
    if (!games[request->game_id()]) {
      games[request->game_id()].reset(new GameRunner);
      games[request->game_id()]->Start();
    }
    LOG(INFO) << "Player " << request->user_name()
              << " has connected as player=" << num_clients_;
    reply->set_status_message(
        absl::StrFormat("Hello %s %d", request->user_name(), num_clients_));
    reply->set_player_index(num_clients_);
    *reply->mutable_game_config() = games[request->game_id()]->AddPlayer();

    ++num_clients_;
    return Status::OK;
  }

  Status MovePlayer(ServerContext* context, const MovePlayerRequest* request,
                    MovePlayerResponse* response) {
    if (!games[request->game_id()])
      return Status::OK;

    // Push the move request and return whatever the current game state is
    games[request->game_id()]->PushRequest(*request);
    int client_clock = games[request->game_id()]->GetState(
        request->player_index(), response->mutable_game_state());
    response->set_client_clock(client_clock);
    return Status::OK;
  }

  Status
  StreamingMovePlayer(ServerContext* context,
                      ServerReaderWriter<MovePlayerResponse, MovePlayerRequest>*
                          stream) override {
    MovePlayerRequest request;
    while (stream->Read(&request)) {
      if (!games[request.game_id()])
        return Status::OK;

      // Push the move request
      games[request.game_id()]->PushRequest(request);
      // and return whatever the current game state is
      MovePlayerResponse response;
      games[request.game_id()]->GetState(request.player_index(),
                                         response.mutable_game_state());
      int client_clock = games[request.game_id()]->GetState(
          request.player_index(), response.mutable_game_state());
      response.set_client_clock(client_clock);
      stream->Write(response);
    }
    return Status::OK;
  }

  int num_clients_;
  std::map<std::string, std::unique_ptr<GameRunner>> games;
};

void RunServer(uint16_t port) {
  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);
  BManServiceImpl service("");

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  RunServer(FLAGS_port);
  return 0;
}
