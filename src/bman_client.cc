/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>

//#include "absl/flags/flag.h"
//#include "absl/flags/parse.h"

#include <grpcpp/grpcpp.h>

#include "level.grpc.pb.h"

// ABSL_FLAG(std::string, target, "localhost:50051", "Server address");

using bman::BManService;
using bman::JoinRequest;
using bman::JoinResponse;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

class Client {
public:
  Client(std::shared_ptr<Channel> channel)
      : stub_(BManService::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string Join(const std::string& user) {
    // Data we are sending to the server.
    JoinRequest request;
    request.set_user_name(user);

    JoinResponse response;
    ClientContext context;
    Status status = stub_->Join(&context, request, &response);

    // Act upon its status.
    if (status.ok()) {
      return response.status_message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

private:
  std::unique_ptr<bman::BManService::Stub> stub_;
};

int main(int argc, char** argv) {
  // We indicate that the channel isn't authenticated (use of
  // InsecureChannelCredentials()).
  Client client(grpc::CreateChannel("localhost:8889",
                                    grpc::InsecureChannelCredentials()));
  std::string user("world");
  std::string reply = client.Join(user);
  std::cout << "Received: " << reply << std::endl;

  return 0;
}
