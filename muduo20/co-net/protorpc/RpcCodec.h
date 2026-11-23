#pragma once

#include "muduo20/co-net/protobuf/ProtobufCodecLite.h"

namespace muduo20::co_net {

class RpcMessage;
using RpcMessagePtr = std::shared_ptr<RpcMessage>;

extern const char rpctag[];

using RpcCodec = ProtobufCodecLiteT<RpcMessage, rpctag>;

} // namespace muduo20::co_net
