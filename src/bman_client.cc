#include "bman_client.h"

#include <grpcpp/grpcpp.h>
#include <glog/logging.h>
#include "level.grpc.pb.h"
#include <memory>

namespace bman {

JoinResponse Client::Join(const std::string& user) {
  JoinRequest request;
  request.set_user_name(user);
  
  JoinResponse response;
  ClientContext context;
  Status status = stub_->Join(&context, request, &response);
  
  if (!status.ok()) {
    LOG(WARNING) << status.error_code() << ": " << status.error_message()
                 << std::endl;
  }
  game_id_ = request.game_id();
  player_index_ = response.player_index();
  return response;
}

MovePlayerResponse Client::MovePlayer(MovePlayerRequest& request) {
  MovePlayerResponse response;
  ClientContext context;
  request.set_game_id(game_id_);
  request.set_player_index(player_index_);
  Status status = stub_->MovePlayer(&context, request, &response);
  
  if (!status.ok()) {
    LOG(WARNING) << status.error_code() << ": " << status.error_message()
                 << std::endl;
  }
  return response;
}

std::unique_ptr<Client> Client::Create(const std::string& server) {
  std::unique_ptr<Client> client(new Client(grpc::CreateChannel(server,
                                                                grpc::InsecureChannelCredentials())));
  return client;
}

}  // namespace
