// client.cpp
#include <capnp/rpc.h>
#include <capnp/rpc-twoparty.h>
#include "../capnp/interface.capnp.h"
#include <kj/async-io.h>
#include <kj/async.h>

int main() {
  auto ioContext = kj::setupAsyncIo();

  // Connect to the server.
  auto address = ioContext.provider->getNetwork().parseAddress("localhost", 12345).wait(ioContext.waitScope);
  auto connection = address->connect().wait(ioContext.waitScope);
  auto network = kj::heap<capnp::TwoPartyVatNetwork>(*connection, capnp::rpc::twoparty::Side::CLIENT);

  // Start the RPC system.
  auto rpcSystem = kj::heap<capnp::RpcSystem<capnp::rpc::twoparty::VatId>>(capnp::makeRpcClient(network.release()));

  // Get the server's main interface.
  auto server = rpcSystem->bootstrap().castAs<MyInterface>();

  // Subscribe to updates.
  auto request = server.getSubscribeRequest();
  auto promise = server.send();
  promise.wait(ioContext.waitScope);

  // Run the event loop forever, receiving updates.
  kj::NEVER_DONE.wait(ioContext.waitScope);
}