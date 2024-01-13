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
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "examples/protos/helloworld.grpc.pb.h"
#else
#include "distributedKV.grpc.pb.h"
#endif

ABSL_FLAG(std::string, target, "localhost:50051", "Server address");

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;

class GreeterClient {
 public:
  GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string SayHello(const std::string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);

    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<Greeter::Stub> stub_;
};

void processCommand(const std::vector<std::string>& args) {
  std::string method = args[0];
  std::string key = "";
  std::string value = "";
  if (method.compare("help") == 0) {
    std::cout << "usage: <method> [-<args> <value>]" << std::endl;
    std::cout << std::endl;
    std::cout << "These are methods' examples:" << std::endl;
    std::cout << "get -k 16         Get the value from remoteDB with key=16." << std::endl;
    std::cout << "del -k 32         Delete the entry on the remoteDB with key=32." << std::endl;
    std::cout << "put -k 64  -v 8   Put the value=8 to the remoteDB with key=64" << std::endl;
    std::cout << std::endl;
  } else if (method.compare("get") == 0) {
    if (args.size() != 3) {
      std::cout << "pandaRDB: Incorrect parameters for `get`. See 'help'." << std::endl;
    } else {
      if (args[1].compare("-k") == 0) {
        key = args[2];
        
      } else {
        std::cout << "pandaRDB: Incorrect parameters for `get`. See 'help'." << std::endl;
      }
    }
  } else if (method.compare("del") == 0) {
    if (args.size() != 3) {
      std::cout << "pandaRDB: Incorrect parameters for `del`. See 'help'." << std::endl;
    } else {
      if (args[1].compare("-k") == 0) {
        key = args[2];

      } else {
        std::cout << "pandaRDB: Incorrect parameters for `del`. See 'help'." << std::endl;
      }
    }
  } else if (method.compare("put") == 0) {
    if (args.size() != 5) {
      std::cout << "pandaRDB: Incorrect parameters for `put`. See 'help'." << std::endl;
    } else {
      if (args[1].compare("-k") == 0 && args[3].compare("-v") == 0) {
        key = args[2];
        value = args[4];

      } else {
        std::cout << "pandaRDB: Incorrect parameters for `put`. See 'help'." << std::endl;
      }
    }
  } else {
    std::cout << "pandaRDB: " << method << " is not a command. See 'help'." << std::endl;
  }
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint specified by
  // the argument "--target=" which is the only expected argument.
  std::string target_str = absl::GetFlag(FLAGS_target);
  // We indicate that the channel isn't authenticated (use of
  // InsecureChannelCredentials()).
  GreeterClient greeter(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  std::string user("world");
  std::string reply = greeter.SayHello(user);
  std::cout << "Greeter received: " << reply << std::endl;

  std::cout << "                                                " << std::endl;
  std::cout << "/\033[1;34m$$$$$$$\033[0m                            /\033[1;34m$$\033[0m          " << std::endl;
  std::cout << "| \033[1;34m$$\033[0m__  \033[1;34m$$\033[0m                          | \033[1;34m$$\033[0m          " << std::endl;
  std::cout << "| \033[1;34m$$\033[0m  \\ \033[1;34m$$\033[0m  /\033[1;34m$$$$$$\033[0m  /\033[1;34m$$$$$$$\033[0m   /\033[1;34m$$$$$$$\033[0m  /\033[1;34m$$$$$$\033[0m " << std::endl;
  std::cout << "| \033[1;34m$$$$$$$\033[0m/ |____  \033[1;34m$$\033[0m| \033[1;34m$$\033[0m__  \033[1;34m$$\033[0m /\033[1;34m$$\033[0m__  \033[1;34m$$\033[0m |____  \033[1;34m$$\033[0m" << std::endl;
  std::cout << "| \033[1;34m$$\033[0m____/   /\033[1;34m$$$$$$$\033[0m| \033[1;34m$$\033[0m  \\ \033[1;34m$$\033[0m| \033[1;34m$$\033[0m  | \033[1;34m$$\033[0m  /\033[1;34m$$$$$$$\033[0m" << std::endl;
  std::cout << "| \033[1;34m$$\033[0m       /\033[1;34m$$\033[0m__  \033[1;34m$$\033[0m| \033[1;34m$$\033[0m  | \033[1;34m$$\033[0m| \033[1;34m$$\033[0m  | \033[1;34m$$\033[0m /\033[1;34m$$\033[0m__  \033[1;34m$$\033[0m" << std::endl;
  std::cout << "| \033[1;34m$$\033[0m      |  \033[1;34m$$$$$$$\033[0m| \033[1;34m$$\033[0m  | \033[1;34m$$\033[0m|  \033[1;34m$$$$$$$\033[0m|  \033[1;34m$$$$$$$\033[0m" << std::endl;
  std::cout << "|__/       \\_______/|__/  |__/ \\_______/ \\_______/" << std::endl;
  std::cout << "                                                " << std::endl;


  std::string input;
  while(1) {
    std::cout << "🍒\033[1;34mpandaRDB> \033[0m";
    std::getline(std::cin, input);

    std::vector<std::string> args;
    std::istringstream iss(input);
    std::string arg;
    while (iss >> arg) {
      args.push_back(arg);
    }

    if (args.empty()) {
      continue;
    }

    processCommand(args);
  }

  return 0;
}
