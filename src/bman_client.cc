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
  AdjustRequest(request);
  request_queue_.push_back(request);

  if ((int)request_queue_.size() > delay_) {
    request = request_queue_.front();

    ClientContext context;
    Status status = stub_->MovePlayer(&context, request, &response);

    if (!status.ok()) {
      LOG(WARNING) << status.error_code() << ": " << status.error_message()
                   << std::endl;
    }
    request_queue_.pop_front();
  }
  UpdateTiming(response);
  return response;
}

MovePlayerResponse Client::StreamingMovePlayer(MovePlayerRequest& request) {
  if (!streaming_) {
    context_.reset(new ClientContext);
    streaming_ = std::move(stub_->StreamingMovePlayer(context_.get()));
  }

  AdjustRequest(request);
  if (!streaming_->Write(request)) {
    LOG(ERROR) << "Unable to write request";
    return {};
  }
  MovePlayerResponse response;
  streaming_->Read(&response);
  UpdateTiming(response);
  return response;
}

void Client::AdjustRequest(MovePlayerRequest& request) {
  request.set_game_id(game_id_);
  request.set_player_index(player_index_);
  request.set_client_clock(latest_time_);
}

void Client::UpdateTiming(const MovePlayerResponse& response) {
  if (response.game_state().has_clock() && response.game_state().clock() >= 0) {
    if (first_move_clock_ < 0) {
      first_move_clock_ = response.game_state().clock();
    }
    latest_time_ = response.game_state().clock();
    const int latency = response.game_state().clock() - response.client_clock();
    LOG_EVERY_N(INFO, 60) << "Latency:" << latency << " " << latest_time_;
  }
}

std::unique_ptr<Client> Client::Create(const std::string& server, int delay) {
  std::unique_ptr<Client> client(new Client(
      grpc::CreateChannel(server, grpc::InsecureChannelCredentials()), delay));
  return client;
}

} // namespace bman
