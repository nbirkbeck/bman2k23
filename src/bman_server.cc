#include <iostream>
#include <memory>
#include <string>

//#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"

#include "level.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "game.h"
#include <memory>
#include <pthread.h>

// ABSL_FLAG(int, port, 8888, "Count of items to process");

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

class GameRunner  {
public:
  ~GameRunner() {
    stopped_ = true;
    pthread_mutex_init(&game_mutex_, nullptr);
    pthread_mutex_init(&request_mutex_, nullptr);
  }

  void Start() {
    game_.BuildSimpleLevel(2);
    pthread_create(&thread_, nullptr, &GameRunner::StaticLoop, this);
  }
  void Loop() {
    while (!stopped_) {
      if (game_.game_state().players_size() > 0) {
        std::vector<bman::MovePlayerRequest> move_requests(game_.game_state().players_size());
      
        pthread_mutex_lock(&request_mutex_);
        for (int i = 0; i < (int)pending_requests_.size(); ++i) {
          move_requests[i] = pending_requests_[i];
          pending_requests_[i].Clear();
        }
        pthread_mutex_unlock(&request_mutex_);

        pthread_mutex_lock(&game_mutex_);
        game_.Step(move_requests);
        pthread_mutex_unlock(&game_mutex_);
      }      
      usleep(1000 / 60.0 * 1000);
    }
  }
  static void* StaticLoop(void*arg) {
    GameRunner* runner = (GameRunner*)arg;
    runner->Loop();
    return nullptr;
  }

  bman::GameConfig AddPlayer() {
    pthread_mutex_lock(&game_mutex_);
    auto config = game_.config();
    game_.AddPlayer();
    pthread_mutex_unlock(&game_mutex_);
    return config;
  }

  void GetState(bman::GameState* game_state) {
    pthread_mutex_lock(&game_mutex_);
    *game_state = game_.game_state();
    pthread_mutex_unlock(&game_mutex_);
  }

  void PushRequest(const bman::MovePlayerRequest& request) {
    pthread_mutex_lock(&request_mutex_);
    if ((int)request.player_index() >= (int)pending_requests_.size()) {
      pending_requests_.resize(request.player_index() + 1);
    }
    pending_requests_[request.player_index()].set_player_index(request.player_index());
    for (const auto& a: request.actions()) {
      *pending_requests_[request.player_index()].add_actions() = a;
    }
    pthread_mutex_unlock(&request_mutex_);
  }

  bool stopped_ = false;
  pthread_t thread_;
  pthread_mutex_t game_mutex_;
  pthread_mutex_t request_mutex_;
  Game game_;
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
    reply->set_status_message(
        absl::StrFormat("Hello %s %d", request->user_name(), num_clients_));
    reply->set_player_index(num_clients_);
    *reply->mutable_game_config() = games[request->game_id()]->AddPlayer();
    
    ++num_clients_;
    if (num_clients_ == 1) {

    }
    std::cout << num_clients_ << "\n";
    return Status::OK;
  }

  Status MovePlayer(ServerContext* context,
                    const MovePlayerRequest* request,
                    MovePlayerResponse* response) {
    if (!games[request->game_id()]) return Status::OK;

   //std::vector<bman::MovePlayerRequest> move_requests;
   //    move_requests.push_back(*request);

    games[request->game_id()]->PushRequest(*request);

    games[request->game_id()]->GetState(response->mutable_game_state());
    return Status::OK;
  }
  
  int num_clients_;
  std::map<std::string, std::unique_ptr<GameRunner> > games;
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
  //  absl::ParseCommandLine(argc, argv);
  RunServer(8889); // absl::GetFlag(FLAGS_port));
  return 0;
}
