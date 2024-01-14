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

#endif

using grpc::Channel;
using grpc::ClientContext;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using distributedKV::Greeter;
using distributedKV::HelloRequest;
using distributedKV::HelloReply;

using distributedKV::kvMethods;
using distributedKV::KVRequest;
using distributedKV::KVResponse;

using distributedKV::workerRegister;
using distributedKV::workerSetup;
using distributedKV::survivalList;

// Default port (master)
ABSL_FLAG(uint16_t, port, 50051, "Server port for the service");
ABSL_FLAG(std::string, addr, "localhost", "Server address");

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }
};

//!< The list recording that which worker is active.
std::unordered_set<uint16_t> survival_list;

//! @brief Register Server End <--- Worker Server
//! 
//! @details Get register request from a new setup worker.
class workerRegisterServiceImpl final : public workerRegister::Service {
  Status Register(ServerContext* context, const workerSetup* request,
                  survivalList* response) {
    // Parse segment from request.
    const std::string& message = request->message();
    uint16_t port = request->port();

    // Record this worker.
    survival_list.insert(port); 

    // Set the response.
    response->set_message("Register Successfully!");
    for (const auto& port : survival_list) {
      response->add_ports(port);
    }

    return Status::OK;
  }
};


//! @brief Register Client End ---> Worker Server
//! 
//! @details Broadcast latest survival list to all worker.
class workerRegisterClient {
  // bool  
};


//! @brief KV Client End ---> Worker Server
//! 
//! @details It's actually the same as client's Client End.
class kvMethodsClient {
 public:
  kvMethodsClient(std::shared_ptr<Channel> channel)
      : stub_(kvMethods::NewStub(channel)) {}

  //! @brief Get the value from remoteDB with key.
  //! 
  //! @param key : to query.
  //! @return KVResponse : the response
  KVResponse Get(const std::string& key) {
    KVRequest request;
    request.set_key(key);

    KVResponse response;
    
    ClientContext context;
    
    // actual rpc
    Status status = stub_->Get(&context, request, &response);

    if (status.ok()) {
      std:: cout << "Message: " << response.message() << std::endl;
      return response;
    } else {
      std::cout << "Code "<< status.error_code() << ": " 
                << status.error_message() << std::endl;
      response.set_error(true);
      return response;
    }
  }

  //! @brief Put the new value to the remoteDB with key,
  //!        can be `update` or `insert`.
  //! 
  //! @param key : to update.
  //! @param value : new value
  //! @return KVResponse : The reponse loaded with new value.
  KVResponse Put(const std::string& key, const std::string& value) {
    KVRequest request;
    request.set_key(key);
    request.set_value(value);

    KVResponse response;
    
    ClientContext context;
    
    // actual rpc
    Status status = stub_->Put(&context, request, &response);

    if (status.ok()) {
      std:: cout << "Message: " << response.message() << std::endl;
      return response;
    } else {
      std::cout << "Code "<< status.error_code() << ": " 
                << status.error_message() << std::endl;
      response.set_error(true);
      return response;
    }
  }

  //! @brief Delete the entry on the remoteDB with key.
  //! 
  //! @param key : to delete.
  //! @return KVResponse : the response
  KVResponse Del(const std::string& key) {
    KVRequest request;
    request.set_key(key);

    KVResponse response;
    
    ClientContext context;
    
    // actual rpc
    Status status = stub_->Del(&context, request, &response);

    if (status.ok()) {
      std:: cout << "Message: " << response.message() << std::endl;
      return response;
    } else {
      std::cout << "Code "<< status.error_code() << ": " 
                << status.error_message() << std::endl;
      response.set_error(true);
      return response;
    }
  }

 private:
  std::unique_ptr<kvMethods::Stub> stub_;
};


//! @brief KV Server End <--- Client
//! 
//! @details Will forward the request ---> Worker
class kvMethodsMasterServiceImpl final : public kvMethods::Service {
  //!< The pointer point to the next worker : impl for load balancing
  int next_worker = 0;
  //!< Key Lock
  std::unordered_set<std::string> locks;

  //! @brief Get the Worker Port object : roll pooling
  uint16_t getWorkerPort() {
    int cnt = (next_worker++) % survival_list.size();
    for (const auto& port: survival_list) {
      if (cnt == 0) {
        return port;
      }
      cnt -= 1;
    }
    return 0;
  }

  //! @brief Get the Worker Socket object
  std::string getWorkerSocket() {
    uint16_t port = getWorkerPort();
    std::string socket = absl::GetFlag(FLAGS_addr) + ":" + std::to_string(port);
    return socket;
  }

  Status Get(ServerContext* context, const KVRequest* reqeust,
            KVResponse* response) {
    // Parse segment from request.
    const std::string& key = reqeust->key();
    const std::string& value = reqeust->value();

    // Forward the request to worker server
    kvMethodsClient methods = kvMethodsClient(
      grpc::CreateChannel(getWorkerSocket(), grpc::InsecureChannelCredentials()));
    *response = methods.Get(key);

    return Status::OK;
  }

  Status Put(ServerContext* context, const KVRequest* reqeust,
            KVResponse* response) {
    // Parse segment from request.
    const std::string& key = reqeust->key();
    const std::string& value = reqeust->value();

    // Check if the key is locked.
    if (locks.count(key) != 0) {
      return Status::CANCELLED;
    } 
    // Get the lock.
    locks.insert(key);

    // Forward the request to worker server
    kvMethodsClient methods = kvMethodsClient(
      grpc::CreateChannel(getWorkerSocket(), grpc::InsecureChannelCredentials()));
    *response = methods.Put(key, value);

    // Release the lock.
    locks.erase(key);

    return Status::OK;
  }

  Status Del(ServerContext* context, const KVRequest* reqeust,
            KVResponse* response) {
    // Parse segment from request.
    const std::string& key = reqeust->key();
    const std::string& value = reqeust->value();

    // Check if the key is locked.
    if (locks.count(key) != 0) {
      return Status::CANCELLED;
    } 
    // Get the lock.
    locks.insert(key);

    // Forward the request to worker server
    kvMethodsClient methods = kvMethodsClient(
      grpc::CreateChannel(getWorkerSocket(), grpc::InsecureChannelCredentials()));
    *response = methods.Del(key);

    // Release the lock.
    locks.erase(key);

    return Status::OK;
  }
};


//! @brief Server Runtime.
//! 
//! @param port : working port
void RunServer(uint16_t port) {
  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);
  GreeterServiceImpl greeter_service;
  kvMethodsMasterServiceImpl kvMethods_service;
  workerRegisterServiceImpl workerRegister_service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&greeter_service);
  builder.RegisterService(&kvMethods_service);
  builder.RegisterService(&workerRegister_service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}


int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  RunServer(absl::GetFlag(FLAGS_port));

  return 0;
}
