#pragma once

#ifndef NKR_NO_GRPC

#include "go/grpc_server/gen/libcore.pb.h"
#include <QString>

namespace QtGrpc {
    class Http2GrpcChannelPrivate;
}

namespace ProxorGui_rpc {
    class Client {
    public:
        explicit Client(std::function<void(const QString &)> onError, const QString &target, const QString &token);

        void Exit();

        bool KeepAlive();

        // QString returns is error string

        QString Validate(bool *rpcOK, const libcore::LoadConfigReq &request);

        QString Start(bool *rpcOK, const libcore::LoadConfigReq &request);

        QString Stop(bool *rpcOK);

    inline Client *defaultClient;
} // namespace ProxorGui_rpc
#endif
