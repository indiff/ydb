#pragma once

#include <library/cpp/actors/core/defs.h>
#include <library/cpp/actors/core/actorid.h>
#include <library/cpp/actors/core/event.h>

#include <util/generic/string.h>

namespace NActors {

    class TDisconnectReason {
        TString Text;

    private:
        explicit TDisconnectReason(TString text)
            : Text(std::move(text))
        {}

    public:
        TDisconnectReason() = default;
        TDisconnectReason(const TDisconnectReason&) = default;
        TDisconnectReason(TDisconnectReason&&) = default;

        static TDisconnectReason FromErrno(int err);

        static TDisconnectReason EndOfStream()            { return TDisconnectReason("EndOfStream"); }
        static TDisconnectReason CloseOnIdle()            { return TDisconnectReason("CloseOnIdle"); }
        static TDisconnectReason LostConnection()         { return TDisconnectReason("LostConnection"); }
        static TDisconnectReason DeadPeer()               { return TDisconnectReason("DeadPeer"); }
        static TDisconnectReason NewSession()             { return TDisconnectReason("NewSession"); }
        static TDisconnectReason HandshakeFailTransient() { return TDisconnectReason("HandshakeFailTransient"); }
        static TDisconnectReason HandshakeFailPermanent() { return TDisconnectReason("HandshakeFailPermanent"); }
        static TDisconnectReason UserRequest()            { return TDisconnectReason("UserRequest"); }
        static TDisconnectReason Debug()                  { return TDisconnectReason("Debug"); }
        static TDisconnectReason ChecksumError()          { return TDisconnectReason("ChecksumError"); }
        static TDisconnectReason FormatError()            { return TDisconnectReason("FormatError"); }
        static TDisconnectReason EventTooLarge()          { return TDisconnectReason("EventTooLarge"); }
        static TDisconnectReason QueueOverload()          { return TDisconnectReason("QueueOverload"); }

        TString ToString() const {
            return Text;
        }

        static TVector<const char*> Reasons;
    };

    struct TProgramInfo {
        ui64 PID = 0;
        ui64 StartTime = 0;
        ui64 Serial = 0;
    };

    struct TSessionParams {
        bool Encryption = {};
        bool UseModernFrame = {};
        bool AuthOnly = {};
        bool UseExtendedTraceFmt = {};
        bool UseExternalDataChannel = {};
        TString AuthCN;
        NActors::TScopeId PeerScopeId;
    };

} // NActors

using NActors::IEventBase;
using NActors::IEventHandle;
using NActors::IEventHandleFat;
using NActors::IEventHandleLight;
using NActors::TActorId;
using NActors::TConstIoVec;
using NActors::TEventSerializedData;
using NActors::TSessionParams;
