#include <deque>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

#include "level.grpc.pb.h"
#include <memory>

namespace bman {
using bman::BManService;
using bman::JoinRequest;
using bman::JoinResponse;
using bman::MovePlayerRequest;
using bman::MovePlayerResponse;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;

// A client connection to the server (manages game_id and player_index)
class Client {
public:
  Client(std::shared_ptr<Channel> channel, int delay = 0)
      : stub_(BManService::NewStub(channel)), delay_(delay) {}

  JoinResponse Join(const std::string& user);
  MovePlayerResponse MovePlayer(MovePlayerRequest& request);
  MovePlayerResponse StreamingMovePlayer(MovePlayerRequest& request);

  static std::unique_ptr<Client> Create(const std::string& server, int delay);

private:
  void AdjustRequest(MovePlayerRequest& request);
  void UpdateTiming(const MovePlayerResponse& response);

  std::unique_ptr<bman::BManService::Stub> stub_;
  std::unique_ptr<ClientReaderWriter<MovePlayerRequest, MovePlayerResponse>>
      streaming_;
  std::unique_ptr<ClientContext> context_; // Used for streaming
  std::string game_id_;
  std::deque<MovePlayerRequest> request_queue_;

  int player_index_ = 0;
  int delay_ = 0;
  int first_move_clock_ = -1;
  int latest_time_ = 0;
};

} // namespace bman
