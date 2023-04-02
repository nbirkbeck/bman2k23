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

class BManServiceImpl final : public bman::BManService::Service {
public:
  BManServiceImpl(const std::string&) { num_clients_ = 0; }
  Status Join(ServerContext* context, const JoinRequest* request,
              JoinResponse* reply) override {
    reply->set_status_message(
        absl::StrFormat("Hello %s %d", request->user_name(), num_clients_));
    std::cout << num_clients_ << "\n";
    ++num_clients_;
    return Status::OK;
  }
  int num_clients_;
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
