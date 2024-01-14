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
#include <random>
#include <unordered_set>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#ifdef BAZEL_BUILD
#include "examples/protos/distributedKV.grpc.pb.h"
#else
#include "distributedKV.grpc.pb.h"

#include <cassert>
#include "leveldb/db.h"

#endif

using grpc::Channel;
using grpc::ClientContext;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using distributedKV::Greeter;
using distributedKV::HelloReply;
using distributedKV::HelloRequest;

using distributedKV::kvMethods;
using distributedKV::KVRequest;
using distributedKV::KVResponse;

using distributedKV::workerRegister;
using distributedKV::workerSetup;
using distributedKV::survivalList;

using distributedKV::workerSpreader;
using distributedKV::updateNotice;
using distributedKV::updateResponse;

// Default port (master)
ABSL_FLAG(uint16_t, port, 50051, "Server port for the service");

//! @brief Greeter Server End
//! 
class GreeterServiceImpl final : public Greeter::Service {
  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }
};

// The list recording that which worker is active.
std::unordered_set<uint16_t> survival_list;

//! @brief Register Client End
//! 
class workerRegisterClient {
 public:
  workerRegisterClient(std::shared_ptr<Channel> channel)
      : stub_(workerRegister::NewStub(channel)) {}

  bool Register(const std::string& message, const int& port) {
    workerSetup request;
    request.set_message("Hi i am worker.");
    request.set_port(port);

    survivalList response;

    ClientContext context;

    // actual rpc
    Status status = stub_->Register(&context, request, &response);

    if (status.ok()) {
      std:: cout << "Message: " << response.message() << std::endl;
      int surList_size = response.ports_size();
      for (int i = 0; i < surList_size; ++i) {
        survival_list.insert(response.ports(i));
      }
      return true;
    } else {
      std::cout << "Code "<< status.error_code() << ": " 
                << status.error_message() << std::endl;
      return false;
    }
  }

 private:
  std::unique_ptr<workerRegister::Stub> stub_;
};

//! @brief Spreader Client End
//! 
class workerSpreaderClient {
 public:
  workerSpreaderClient(std::shared_ptr<Channel> channel)
      : stub_(workerSpreader::NewStub(channel)) {}

  std::string Spread(const bool rollBackFlag, const std::string method,
                     const std::string& key, const std::string& value) {
    updateNotice request;
    request.set_rollbackflag(rollBackFlag);
    request.set_method(method);
    request.set_key(key);
    request.set_value(value);

    updateResponse response;

    ClientContext context;

    // actual rpc
    Status status = stub_->Spread(&context, request, &response);

    return "null";
  }

 private:
  std::unique_ptr<workerSpreader::Stub> stub_;
};

// Spreader Server End
class workerSpreaderServiceImpl final : public workerSpreader::Service {

};

//! @brief KV Server End <--- Master Server
//! 
class kvMethodsServiceImpl final : public kvMethods::Service {
  // Status Get()
};

//! @brief Server Runtime.
//! 
//! @param port : working port
void RunServer(uint16_t port) {
  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);
  GreeterServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  // Generate a random port for this worker to serve.
  std::random_device rd;
  std::mt19937 eng(rd());
  std::uniform_int_distribution<uint16_t> dist(51051, 55051);
  uint16_t random_port = dist(eng);
  
  // Init the database
  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  std::string database_dir = "/tmp/testdb/" + std::to_string(random_port);
  leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
  assert(status.ok());
  // std::cout << status.ok() << std::endl;

  // Contact master for registering


  // Run server
  RunServer(random_port);

  delete db;
  return 0;
}
