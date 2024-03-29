# interface.capnp
@0xb3c4e1c09d8b1c75;

struct Request {
  msg @0 :Text;
}

struct Response {
  reply @0 :Text;
}

interface MyInterface {
  foo @0 (request :Request) -> (response :Response);
  subscribe @1 () -> ();
  connect @2 (vat :VatId) -> ();
}

struct VatId {
  id @0 :UInt64;
}