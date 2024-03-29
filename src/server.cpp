// server.cpp
#include <capnp/rpc.h>
#include <capnp/rpc-twoparty.h>
#include "../capnp/interface.capnp.h"
#include <kj/async-io.h>
#include <kj/async.h>
#include <signal.h>
#include <unordered_map>

#include <vector>
#include <string>

class MyInterfaceImpl final: public MyInterface::Server {
public:
  kj::Promise<void> foo(FooContext context) override {
    auto request = context.getParams().getRequest();
    auto response = context.getResults().initResponse();
    response.setReply("Received: " + request.getMsg());
    return kj::READY_NOW;
  }

  kj::Promise<void> subscribe(SubscribeContext context) override {
    subscribers.push_back(context.getResults());
    return kj::READY_NOW;
  }

  kj::Promise<void> connect(ConnectContext context) override {
    auto vatId = context.getParams().getVat().getId();
    auto stream = network.connect(vatId);
    auto rpcSystem = capnp::makeRpcClient(stream);
    vats[vatId] = rpcSystem.bootstrap().castAs<MyInterface>();
    return kj::READY_NOW;
  }

  void notifySubscribers(const std::string& msg) {
    for (auto& subscriber : subscribers) {
      auto request = subscriber.initRequest();
      request.setMsg(msg);
      auto promise = subscriber.send();
      tasks.add(kj::mv(promise));
    }
  }

private:
  std::vector<MyInterface::Client> subscribers;
  kj::TaskSet tasks;
  capnp::TwoPartyVatNetwork network;
  std::unordered_map<uint64_t, MyInterface::Client> vats;
};

int main() {
  auto ioContext = kj::setupAsyncIo();

  // Bind to a local address.
  auto address = ioContext.provider->getNetwork().parseAddress("localhost", 12345).wait(ioContext.waitScope);
  auto listener = address->listen();

  // Accept connections and start the RPC system for each one.
  kj::TaskSet tasks(ioContext.waitScope);
  while (true) {
    auto connection = listener->accept().wait(ioContext.waitScope);
    auto network = kj::heap<capnp::TwoPartyVatNetwork>(*connection, capnp::rpc::twoparty::Side::SERVER);
    auto server = kj::heap<MyInterfaceImpl>(*network);
    auto rpcSystem = kj::heap<capnp::RpcSystem<capnp::rpc::twoparty::VatId>>(capnp::makeRpcServer(network.release(), server.release()));
    tasks.add(rpcSystem->onDisconnect().then([rpcSystem]() mutable {
      // Clean up when the connection ends.
      delete rpcSystem;
    }));
  }

  // Handle SIGTERM gracefully.
  signal(SIGTERM, [](int signum) {
    kj::throwSystemError(signum, "Received SIGTERM");
  });

  // Run the event loop forever, serving requests.
  kj::NEVER_DONE.wait(ioContext.waitScope);
}