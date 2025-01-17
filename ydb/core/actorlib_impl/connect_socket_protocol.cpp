#include "connect_socket_protocol.h"

#include <ydb/core/base/appdata.h>

namespace NActors {

void TConnectSocketProtocol::ProtocolFunc(
        TAutoPtr<NActors::IEventHandle>& ev,
        const TActorContext& ctx) noexcept
{
    switch (ev->GetTypeRewrite()) {

    case TEvConnectWakeup::EventType:
        TryAgain(ctx);
        break;

    case TEvSocketConnect::EventType:
        CatchConnectedSocket(ctx, std::move(Socket));
        break;

    case TEvSocketError::EventType:
        {
            auto errorMsg = IEventHandleFat::GetFat(ev)->Get<TEvSocketError>();
            if (Socket.Get() == errorMsg->Socket.Get())
                CheckConnectResult(ctx, errorMsg->Error);
        }
        break;

    default:
        Y_FAIL("Unknown message type dispatched");
    }
}


void TConnectSocketProtocol::CheckRetry(
        const TActorContext& ctx,
        TString explain) noexcept
{
    if (++Retries > 5) {
        CatchConnectError(ctx, std::move(explain));
    } else {
        ctx.Schedule(TDuration::Seconds(1), new TEvConnectWakeup);
    }
}


bool TConnectSocketProtocol::CheckConnectResult(
        const TActorContext& ctx,
        int result) noexcept
{
    switch (result) {
    case EINPROGRESS:
        return false;

    case 0:
        CatchConnectedSocket(ctx, std::move(Socket));
        break;

    case EBADF:
        Y_FAIL("bad descriptor");
    case EFAULT:
        Y_FAIL("socket structure address is outside the user's address space");
    case EAFNOSUPPORT:
        Y_FAIL("EAFNOSUPPORT");
    case EISCONN:
        Y_FAIL("socket is already connected");
    case ENOTSOCK:
        Y_FAIL("descriptor is not a socket");

    case EACCES:
        CatchConnectError(ctx, "permission denied");
        break;

    case EADDRINUSE:
        CheckRetry(ctx, "EADDRINUSE");
        break;

    case EAGAIN:
        CheckRetry(ctx, "No more free local ports");
        break;

    case ECONNREFUSED:
        CheckRetry(ctx, "Connection refused");
        break;

    case ENETUNREACH:
        CheckRetry(ctx, "Network is unreachable");
        break;

    default:
        CheckRetry(ctx, "unexpected error from 'socket'");
        break;
    }
    return true;
}


static TDelegate CheckConnectionRoutine(
        const TIntrusivePtr<TSharedDescriptor>& FDPtr,
        const TActorContext& ctx) noexcept
{
    NInterconnect::TStreamSocket* sock =
        (NInterconnect::TStreamSocket*)FDPtr.Get();

    int errCode;
    if (GetSockOpt(*sock, SOL_SOCKET, SO_ERROR, errCode) == -1) {
        switch (errno) {
        case EBADF:
            Y_FAIL("Bad descriptor");

        case EFAULT:
            Y_FAIL("Invalid optval in getsockopt");

        case EINVAL:
            Y_FAIL("Invalid optlne in getsockopt");

        case ENOPROTOOPT:
            Y_FAIL("Unknown option getsockopt");

        case ENOTSOCK:
            Y_FAIL("Not a socket in getsockopt");

        default:
            Y_FAIL("Unexpected error from getsockopt");
        }
    }

    switch (errCode) {
    case EINPROGRESS:
        return TDelegate();

    case 0:
        return [=]() { ctx.Send(ctx.SelfID, new TEvSocketConnect); };

    default:
        {
            TIntrusivePtr<NInterconnect::TStreamSocket> sockPtr(sock);
            return [=]() {
                ctx.Send(ctx.SelfID, new TEvSocketError(errCode, sockPtr));
            };
        }
    }
}


void TConnectSocketProtocol::TryAgain(const TActorContext& ctx) noexcept {
    Socket = NInterconnect::TStreamSocket::Make(Address->Addr()->sa_family);
    if (*Socket == -1) {
        CheckRetry(ctx, "could not create socket");
        return;
    }

    const auto result = Socket->Connect(Address.Get());
    if (CheckConnectResult(ctx, -result))
        return;

    IPoller* poller = NKikimr::AppData(ctx)->PollerThreads.Get();
    poller->StartWrite(Socket,
        std::bind(CheckConnectionRoutine, std::placeholders::_1, ctx));
}


}
