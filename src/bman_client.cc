#include "bman_client.h"

#include "level.grpc.pb.h"
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
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

MovePlayerResponse Client::StreamingMovePlayer(MovePlayerRequest& request) {
  if (!streaming_) {
    context_.reset(new ClientContext);
    streaming_ = std::move(stub_->StreamingMovePlayer(context_.get()));
  }

  request.set_game_id(game_id_);
  request.set_player_index(player_index_);
  if (!streaming_->Write(request)) {
    LOG(ERROR) << "Unable to write request";
    return {};
  }
  MovePlayerResponse response;
  if (streaming_->Read(&response)) {
    return response;
  }
  return response;
}

std::unique_ptr<Client> Client::Create(const std::string& server) {
  std::unique_ptr<Client> client(new Client(
      grpc::CreateChannel(server, grpc::InsecureChannelCredentials())));
  return client;
}

} // namespace bman
