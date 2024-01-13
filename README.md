[C++ Quick Start]: https://grpc.io/docs/languages/cpp/quickstart


# Build and Run this project

1. Install gRPC in `$MY_INSTALL_DIR`. (See [C++ Quick Start][])

2. Build:

```bash
mkdir -p cmake/build
pushd cmake/build
cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../..
make -j 4
```

3. Run:

```bash
# in cmake/build:
./greeter_server
./greeter_client
```

