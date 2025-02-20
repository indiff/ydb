#include "interconnect_handshake.h"
#include "handshake_broker.h"
#include "interconnect_tcp_proxy.h"

#include <library/cpp/actors/core/actor_coroutine.h>
#include <library/cpp/actors/core/log.h>
#include <library/cpp/actors/protos/services_common.pb.h>
#include <util/system/getpid.h>

#include <google/protobuf/text_format.h>

#include <variant>

namespace NActors {
    static constexpr size_t StackSize = 64 * 1024; // 64k should be enough

    class THandshakeActor
       : public TActorCoroImpl
       , public TInterconnectLoggingBase
    {
        struct TExHandshakeFailed : yexception {};
        struct TExPoison {};

        static constexpr TDuration ResolveTimeout = TDuration::Seconds(1);

#pragma pack(push, 1)

        struct TInitialPacket {
            struct {
                TActorId SelfVirtualId;
                TActorId PeerVirtualId;
                ui64 NextPacket;
                ui64 Version;
            } Header;
            ui32 Checksum;

            TInitialPacket() = default;

            TInitialPacket(const TActorId& self, const TActorId& peer, ui64 nextPacket, ui64 version) {
                Header.SelfVirtualId = self;
                Header.PeerVirtualId = peer;
                Header.NextPacket = nextPacket;
                Header.Version = version;
                Checksum = Crc32cExtendMSanCompatible(0, &Header, sizeof(Header));
            }

            bool Check() const {
                return Checksum == Crc32cExtendMSanCompatible(0, &Header, sizeof(Header));
            }

            TString ToString() const {
                return TStringBuilder()
                    << "{SelfVirtualId# " << Header.SelfVirtualId.ToString()
                    << " PeerVirtualId# " << Header.PeerVirtualId.ToString()
                    << " NextPacket# " << Header.NextPacket
                    << " Version# " << Header.Version
                    << "}";
            }
        };

        struct TExHeader {
            static constexpr ui32 MaxSize = 1024 * 1024;

            ui32 Checksum;
            ui32 Size;

            ui32 CalculateChecksum(const void* data, size_t len) const {
                return Crc32cExtendMSanCompatible(Crc32cExtendMSanCompatible(0, &Size, sizeof(Size)), data, len);
            }

            void Sign(const void* data, size_t len) {
                Checksum = CalculateChecksum(data, len);
            }

            bool Check(const void* data, size_t len) const {
                return Checksum == CalculateChecksum(data, len);
            }
        };

#pragma pack(pop)

    private:
        TInterconnectProxyCommon::TPtr Common;
        TActorId SelfVirtualId;
        TActorId PeerVirtualId;
        ui32 PeerNodeId = 0;
        ui64 NextPacketToPeer = 0;
        TMaybe<ui64> NextPacketFromPeer; // will be obtained from incoming initial packet
        TString PeerHostName;
        TString PeerAddr;
        TSocketPtr Socket;
        TPollerToken::TPtr PollerToken;
        TString State;
        TString HandshakeKind;
        TMaybe<THolder<TProgramInfo>> ProgramInfo; // filled in in case of successful handshake; even if null
        TSessionParams Params;
        bool ResolveTimedOut = false;
        THashMap<ui32, TInstant> LastLogNotice;
        const TDuration MuteDuration = TDuration::Seconds(15);
        TMonotonic Deadline;
        TActorId HandshakeBroker;
        std::optional<TBrokerLeaseHolder> BrokerLeaseHolder;

    public:
        THandshakeActor(TInterconnectProxyCommon::TPtr common, const TActorId& self, const TActorId& peer,
                        ui32 nodeId, ui64 nextPacket, TString peerHostName, TSessionParams params)
            : TActorCoroImpl(StackSize, true)
            , Common(std::move(common))
            , SelfVirtualId(self)
            , PeerVirtualId(peer)
            , PeerNodeId(nodeId)
            , NextPacketToPeer(nextPacket)
            , PeerHostName(std::move(peerHostName))
            , HandshakeKind("outgoing handshake")
            , Params(std::move(params))
        {
            Y_VERIFY(SelfVirtualId);
            Y_VERIFY(SelfVirtualId.NodeId());
            Y_VERIFY(PeerNodeId);
            HandshakeBroker = MakeHandshakeBrokerOutId();
        }

        THandshakeActor(TInterconnectProxyCommon::TPtr common, TSocketPtr socket)
            : TActorCoroImpl(StackSize, true)
            , Common(std::move(common))
            , Socket(std::move(socket))
            , HandshakeKind("incoming handshake")
        {
            Y_VERIFY(Socket);
            PeerAddr = TString::Uninitialized(1024);
            if (GetRemoteAddr(*Socket, PeerAddr.Detach(), PeerAddr.size())) {
                PeerAddr.resize(strlen(PeerAddr.data()));
            } else {
                PeerAddr.clear();
            }
        }

        void UpdatePrefix() {
            SetPrefix(Sprintf("Handshake %s [node %" PRIu32 "]", SelfActorId.ToString().data(), PeerNodeId));
        }

        void Run() override {
            try {
                RunImpl();
            } catch (const TDtorException&) {
                if (BrokerLeaseHolder) {
                    BrokerLeaseHolder->ForgetLease();
                }
                throw;
            } catch (const TExPoison&) {
                return; // just stop execution
            } catch (...) {
                throw;
            }
        }

        void RunImpl() {
            UpdatePrefix();

            if (!Socket && Common->OutgoingHandshakeInflightLimit) {
                // Create holder, which sends request to broker and automatically frees the place when destroyed
                BrokerLeaseHolder.emplace(SelfActorId, HandshakeBroker);
            }

            if (BrokerLeaseHolder && BrokerLeaseHolder->IsLeaseRequested()) {
                WaitForSpecificEvent<TEvHandshakeBrokerPermit>("HandshakeBrokerPermit");
            }

            // set up overall handshake process timer
            TDuration timeout = Common->Settings.Handshake;
            if (timeout == TDuration::Zero()) {
                timeout = DEFAULT_HANDSHAKE_TIMEOUT;
            }
            timeout += ResolveTimeout * 2;

            if (Socket) {
                // Incoming handshakes have shorter timeout than outgoing
                timeout *= 0.9;
            }

            Deadline = TActivationContext::Monotonic() + timeout;
            Schedule(Deadline, new TEvents::TEvWakeup);

            try {
                if (Socket) {
                    PerformIncomingHandshake();
                } else {
                    PerformOutgoingHandshake();
                }

                // establish encrypted channel, or, in case when encryption is disabled, check if it matches settings
                if (ProgramInfo) {
                    if (Params.Encryption) {
                        EstablishSecureConnection();
                    } else if (Common->Settings.EncryptionMode == EEncryptionMode::REQUIRED && !Params.AuthOnly) {
                        Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "Peer doesn't support encryption, which is required");
                    }
                }
            } catch (const TExHandshakeFailed&) {
                ProgramInfo.Clear();
            }

            if (ProgramInfo) {
                LOG_LOG_IC_X(NActorsServices::INTERCONNECT, "ICH04", NLog::PRI_INFO, "handshake succeeded");
                Y_VERIFY(NextPacketFromPeer);
                if (PollerToken) {
                    Y_VERIFY(PollerToken->RefCount() == 1);
                    PollerToken.Reset(); // ensure we are going to destroy poller token here as we will re-register the socket within other actor
                }
                SendToProxy(MakeHolder<TEvHandshakeDone>(std::move(Socket), PeerVirtualId, SelfVirtualId,
                    *NextPacketFromPeer, ProgramInfo->Release(), std::move(Params)));
            }

            Socket.Reset();
        }

        void EstablishSecureConnection() {
            Y_VERIFY(PollerToken && PollerToken->RefCount() == 1);
            PollerToken.Reset();
            auto ev = AskProxy<TEvSecureSocket>(MakeHolder<TEvGetSecureSocket>(Socket), "AskProxy(TEvSecureContext)");
            Socket = std::move(ev->Get()->Socket);
            RegisterInPoller();
            const ui32 myNodeId = GetActorSystem()->NodeId;
            const bool server = myNodeId < PeerNodeId; // keep server/client role permanent to enable easy TLS session resuming
            for (;;) {
                TString err;
                auto& secure = static_cast<NInterconnect::TSecureSocket&>(*Socket);
                switch (secure.Establish(server, Params.AuthOnly, err)) {
                    case NInterconnect::TSecureSocket::EStatus::SUCCESS:
                        if (Params.AuthOnly) {
                            Params.Encryption = false;
                            Params.AuthCN = secure.GetPeerCommonName();
                            Y_VERIFY(PollerToken && PollerToken->RefCount() == 1);
                            PollerToken.Reset();
                            Socket = secure.Detach();
                        }
                        return;

                    case NInterconnect::TSecureSocket::EStatus::ERROR:
                        Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, err, true);
                        [[fallthrough]];

                    case NInterconnect::TSecureSocket::EStatus::WANT_READ:
                        WaitPoller(true, false, "ReadEstablish");
                        break;

                    case NInterconnect::TSecureSocket::EStatus::WANT_WRITE:
                        WaitPoller(false, true, "WriteEstablish");
                        break;
                }
            }
        }

        void ProcessUnexpectedEvent(TAutoPtr<IEventHandle> ev) {
            switch (const ui32 type = ev->GetTypeRewrite()) {
                case TEvents::TSystem::Wakeup:
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_TRANSIENT, Sprintf("Handshake timed out, State# %s", State.data()), true);
                    [[fallthrough]];

                case ui32(ENetwork::NodeInfo):
                case TEvInterconnect::EvNodeAddress:
                case ui32(ENetwork::ResolveError):
                   break; // most likely a race with resolve timeout

                case TEvPollerReady::EventType:
                   break;

                case TEvents::TSystem::Poison:
                   throw TExPoison();

                default:
                    Y_FAIL("unexpected event 0x%08" PRIx32, type);
            }
        }

        template<typename T>
        void SetupCompatibilityInfo(T& proto) {
            if (Common->CompatibilityInfo) {
                proto.SetCompatibilityInfo(*Common->CompatibilityInfo);
            }
        }

        template<typename T>
        void SetupVersionTag(T& proto) {
            if (Common->VersionInfo) {
                proto.SetVersionTag(Common->VersionInfo->Tag);
                for (const TString& accepted : Common->VersionInfo->AcceptedTags) {
                    proto.AddAcceptedVersionTags(accepted);
                }
            }
        }

        template<typename T>
        void SetupClusterUUID(T& proto) {
            auto *pb = proto.MutableClusterUUIDs();
            pb->SetClusterUUID(Common->ClusterUUID);
            for (const TString& uuid : Common->AcceptUUID) {
                pb->AddAcceptUUID(uuid);
            }
        }

        template<typename T, typename TCallback>
        void ValidateCompatibilityInfo(const T& proto, TCallback&& errorCallback) {
            // if possible, use new CompatibilityInfo field
            if (proto.HasCompatibilityInfo() && Common->ValidateCompatibilityInfo) {
                TString errorReason;
                if (!Common->ValidateCompatibilityInfo(proto.GetCompatibilityInfo(), errorReason)) {
                    TStringStream s("Local and peer CompatibilityInfo are incompatible");
                    s << ", errorReason# " << errorReason;
                    errorCallback(s.Str());
                }
            } else if (proto.HasVersionTag() && Common->ValidateCompatibilityOldFormat) {
                TInterconnectProxyCommon::TVersionInfo oldFormat;
                oldFormat.Tag = proto.GetVersionTag();
                for (ui32 i = 0; i < proto.AcceptedVersionTagsSize(); ++i) {
                    oldFormat.AcceptedTags.insert(proto.GetAcceptedVersionTags(i));
                }
                TString errorReason;
                if (!Common->ValidateCompatibilityOldFormat(oldFormat, errorReason)) {
                    TStringStream s("Local CompatibilityInfo and peer TVersionInfo are incompatible");
                    s << ", errorReason# " << errorReason;
                    errorCallback(s.Str());
                }
            } else {
                ValidateVersionTag(proto, std::forward<TCallback>(errorCallback));
            }
        }

        template<typename T, typename TCallback>
        void ValidateVersionTag(const T& proto, TCallback&& errorCallback) {
            // check if we will accept peer's version tag (if peer provides one and if we have accepted list non-empty)
            if (Common->VersionInfo) {
                if (!proto.HasVersionTag()) {
                    LOG_LOG_IC_X(NActorsServices::INTERCONNECT, "ICH06", NLog::PRI_WARN,
                        "peer did not report VersionTag, accepting by default");
                } else if (!Common->VersionInfo->AcceptedTags.count(proto.GetVersionTag())) {
                    // we will not accept peer's tag, so check if remote peer would accept our version tag
                    size_t i;
                    for (i = 0; i < proto.AcceptedVersionTagsSize() && Common->VersionInfo->Tag != proto.GetAcceptedVersionTags(i); ++i)
                    {}
                    if (i == proto.AcceptedVersionTagsSize()) {
                        // peer will neither accept our version -- this is total failure
                        TStringStream s("local/peer version tags did not match accepted ones");
                        s << " local Tag# " << Common->VersionInfo->Tag << " accepted Tags# [";
                        bool first = true;
                        for (const auto& tag : Common->VersionInfo->AcceptedTags) {
                            s << (std::exchange(first, false) ? "" : " ") << tag;
                        }
                        s << "] peer Tag# " << proto.GetVersionTag() << " accepted Tags# [";
                        first = true;
                        for (const auto& tag : proto.GetAcceptedVersionTags()) {
                            s << (std::exchange(first, false) ? "" : " ") << tag;
                        }
                        s << "]";
                        errorCallback(s.Str());
                    }
                }
            }
        }

        template<typename T, typename TCallback>
        void ValidateClusterUUID(const T& proto, TCallback&& errorCallback, const TMaybe<TString>& uuid = {}) {
            auto formatList = [](const auto& list) {
                TStringStream s;
                s << "[";
                for (auto it = list.begin(); it != list.end(); ++it) {
                    if (it != list.begin()) {
                        s << " ";
                    }
                    s << *it;
                }
                s << "]";
                return s.Str();
            };
            if (!Common->AcceptUUID) {
                return; // promiscuous mode -- we accept every other peer
            }
            if (!proto.HasClusterUUIDs()) {
                if (uuid) {
                    // old-style checking, peer does not support symmetric protoocol
                    bool matching = false;
                    for (const TString& accepted : Common->AcceptUUID) {
                        if (*uuid == accepted) {
                            matching = true;
                            break;
                        }
                    }
                    if (!matching) {
                        errorCallback(Sprintf("Peer ClusterUUID# %s mismatch, AcceptUUID# %s", uuid->data(), formatList(Common->AcceptUUID).data()));
                    }
                }
                return; // remote side did not fill in this field -- old version, symmetric protocol is not supported
            }

            const auto& uuids = proto.GetClusterUUIDs();

            // check if our UUID matches remote accept list
            for (const TString& item : uuids.GetAcceptUUID()) {
                if (item == Common->ClusterUUID) {
                    return; // match
                }
            }

            // check if remote UUID matches our accept list
            const TString& remoteUUID = uuids.GetClusterUUID();
            for (const TString& item : Common->AcceptUUID) {
                if (item == remoteUUID) {
                    return; // match
                }
            }

            // no match
            errorCallback(Sprintf("Peer ClusterUUID# %s mismatch, AcceptUUID# %s", remoteUUID.data(), formatList(Common->AcceptUUID).data()));
        }

        void ParsePeerScopeId(const NActorsInterconnect::TScopeId& proto) {
            Params.PeerScopeId = {proto.GetX1(), proto.GetX2()};
        }

        void FillInScopeId(NActorsInterconnect::TScopeId& proto) {
            const TScopeId& scope = Common->LocalScopeId;
            proto.SetX1(scope.first);
            proto.SetX2(scope.second);
        }

        template<typename T>
        void ReportProto(const T& protobuf, const char *msg) {
            auto formatString = [&] {
                google::protobuf::TextFormat::Printer p;
                p.SetSingleLineMode(true);
                TString s;
                p.PrintToString(protobuf, &s);
                return s;
            };
            LOG_LOG_IC_X(NActorsServices::INTERCONNECT, "ICH07", NLog::PRI_DEBUG, "%s %s", msg,
                formatString().data());
        }

        bool CheckPeerCookie(const TString& cookie, TString *error) {
            // create a temporary socket to connect to the peer
            TSocketPtr tempSocket;
            std::swap(tempSocket, Socket);
            TPollerToken::TPtr tempPollerToken;
            std::swap(tempPollerToken, PollerToken);

            // set up virtual self id to ensure peer will not drop our connection
            char buf[12] = {'c', 'o', 'o', 'k', 'i', 'e', ' ', 'c', 'h', 'e', 'c', 'k'};
            SelfVirtualId = TActorId(SelfActorId.NodeId(), TStringBuf(buf, 12));

            bool success = true;
            try {
                // issue connection and send initial packet
                Connect(false);
                SendInitialPacket();

                // wait for basic response
                TInitialPacket response;
                ReceiveData(&response, sizeof(response), "ReceiveResponse");
                if (!response.Check()) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_TRANSIENT, "Initial packet CRC error");
                } else if (response.Header.Version != INTERCONNECT_PROTOCOL_VERSION) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, Sprintf("Incompatible protocol %" PRIu64, response.Header.Version));
                }

                // issue cookie check request
                NActorsInterconnect::THandshakeRequest request;
                request.SetProtocol(INTERCONNECT_PROTOCOL_VERSION);
                request.SetProgramPID(0);
                request.SetProgramStartTime(0);
                request.SetSerial(0);
                request.SetReceiverNodeId(0);
                request.SetSenderActorId(TString());
                request.SetCookie(cookie);
                request.SetDoCheckCookie(true);
                SendExBlock(request, "SendExBlockDoCheckCookie");

                // process cookie check reply
                NActorsInterconnect::THandshakeReply reply;
                if (!reply.ParseFromString(ReceiveExBlock("ReceiveExBlockDoCheckCookie"))) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "Incorrect packet from peer");
                } else if (reply.HasCookieCheckResult() && !reply.GetCookieCheckResult()) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "Cookie check error -- possible network problem");
                }
            } catch (const TExHandshakeFailed& e) {
                *error = e.what();
                success = false;
            }

            // restore state
            SelfVirtualId = TActorId();
            std::swap(tempSocket, Socket);
            std::swap(tempPollerToken, PollerToken);
            return success;
        }

        void PerformOutgoingHandshake() {
            LOG_LOG_IC_X(NActorsServices::INTERCONNECT, "ICH01", NLog::PRI_DEBUG,
                "starting outgoing handshake");

            // perform connection
            Connect(true);

            // send initial request packet
            SendInitialPacket();

            TInitialPacket response;
            ReceiveData(&response, sizeof(response), "ReceiveResponse");
            if (!response.Check()) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_TRANSIENT, "Initial packet CRC error");
            } else if (response.Header.Version != INTERCONNECT_PROTOCOL_VERSION) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, Sprintf("Incompatible protocol %" PRIu64, response.Header.Version));
            }

            // extract next packet
            NextPacketFromPeer = response.Header.NextPacket;

            if (!PeerVirtualId) {
                // creating new session -- we have to generate request
                NActorsInterconnect::THandshakeRequest request;

                request.SetProtocol(INTERCONNECT_PROTOCOL_VERSION);
                request.SetProgramPID(GetPID());
                request.SetProgramStartTime(Common->StartTime);
                request.SetSerial(SelfVirtualId.LocalId());
                request.SetReceiverNodeId(PeerNodeId);
                request.SetSenderActorId(SelfVirtualId.ToString());
                request.SetSenderHostName(Common->TechnicalSelfHostName);
                request.SetReceiverHostName(PeerHostName);

                if (Common->LocalScopeId != TScopeId()) {
                    FillInScopeId(*request.MutableClientScopeId());
                }

                if (Common->Cookie) {
                    request.SetCookie(Common->Cookie);
                }
                if (Common->ClusterUUID) {
                    request.SetUUID(Common->ClusterUUID);
                }
                SetupClusterUUID(request);
                SetupCompatibilityInfo(request);
                SetupVersionTag(request);

                if (const ui32 size = Common->HandshakeBallastSize) {
                    TString ballast(size, 0);
                    char* data = ballast.Detach();
                    for (ui32 i = 0; i < size; ++i) {
                        data[i] = i;
                    }
                    request.SetBallast(ballast);
                }

                switch (Common->Settings.EncryptionMode) {
                    case EEncryptionMode::DISABLED:
                        break;

                    case EEncryptionMode::OPTIONAL:
                        request.SetRequireEncryption(false);
                        break;

                    case EEncryptionMode::REQUIRED:
                        request.SetRequireEncryption(true);
                        break;
                }

                request.SetRequestModernFrame(true);
                request.SetRequestAuthOnly(Common->Settings.TlsAuthOnly);
                request.SetRequestExtendedTraceFmt(true);
                request.SetRequestExternalDataChannel(Common->Settings.EnableExternalDataChannel);

                SendExBlock(request, "ExRequest");

                NActorsInterconnect::THandshakeReply reply;
                if (!reply.ParseFromString(ReceiveExBlock("ExReply"))) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "Incorrect THandshakeReply");
                }
                ReportProto(reply, "ReceiveExBlock ExReply");

                if (reply.HasErrorExplaination()) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "error from peer: " + reply.GetErrorExplaination());
                } else if (!reply.HasSuccess()) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "empty reply");
                }

                auto generateError = [this](TString msg) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, msg);
                };

                const auto& success = reply.GetSuccess();
                ValidateClusterUUID(success, generateError);
                ValidateCompatibilityInfo(success, generateError);

                const auto& s = success.GetSenderActorId();
                PeerVirtualId.Parse(s.data(), s.size());

                // recover flags
                Params.Encryption = success.GetStartEncryption();
                Params.UseModernFrame = success.GetUseModernFrame();
                Params.AuthOnly = Params.Encryption && success.GetAuthOnly();
                Params.UseExtendedTraceFmt = success.GetUseExtendedTraceFmt();
                Params.UseExternalDataChannel = success.GetUseExternalDataChannel();
                if (success.HasServerScopeId()) {
                    ParsePeerScopeId(success.GetServerScopeId());
                }

                // recover peer process info from peer's reply
                ProgramInfo = GetProgramInfo(success);
            } else if (!response.Header.SelfVirtualId) {
                // peer reported error -- empty ack was generated by proxy for this request
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_SESSION_MISMATCH, "Peer rejected session continuation handshake");
            } else if (response.Header.SelfVirtualId != PeerVirtualId || response.Header.PeerVirtualId != SelfVirtualId) {
                // resuming existing session; check that virtual ids of peers match each other
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_SESSION_MISMATCH, "Session virtual ID mismatch");
            } else {
                ProgramInfo.ConstructInPlace(); // successful handshake
            }
        }

        void PerformIncomingHandshake() {
            LOG_LOG_IC_X(NActorsServices::INTERCONNECT, "ICH02", NLog::PRI_DEBUG,
                "starting incoming handshake");

            // set up incoming socket
            SetupSocket();
            RegisterInPoller();

            // wait for initial request packet
            TInitialPacket request;
            ReceiveData(&request, sizeof(request), "ReceiveRequest");
            if (!request.Check()) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_TRANSIENT, "Initial packet CRC error");
            } else if (request.Header.Version != INTERCONNECT_PROTOCOL_VERSION) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, Sprintf("Incompatible protocol %" PRIu64, request.Header.Version));
            }

            // extract peer node id from the peer
            PeerNodeId = request.Header.SelfVirtualId.NodeId();
            if (!PeerNodeId) {
                Y_VERIFY_DEBUG(false, "PeerNodeId is zero request# %s", request.ToString().data());
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "SelfVirtualId.NodeId is empty in initial packet");
            }
            UpdatePrefix();

            // extract next packet
            NextPacketFromPeer = request.Header.NextPacket;

            if (request.Header.PeerVirtualId) {
                // issue request to the proxy and wait for the response
                auto reply = AskProxy<TEvHandshakeAck, TEvHandshakeNak>(MakeHolder<TEvHandshakeAsk>(
                    request.Header.SelfVirtualId, request.Header.PeerVirtualId, request.Header.NextPacket),
                    "TEvHandshakeAsk");
                if (auto *ack = reply->CastAsLocal<TEvHandshakeAck>()) {
                    // extract self/peer virtual ids
                    SelfVirtualId = ack->Self;
                    PeerVirtualId = request.Header.SelfVirtualId;
                    NextPacketToPeer = ack->NextPacket;
                    Params = ack->Params;

                    // only succeed in case when proxy returned valid SelfVirtualId; otherwise it wants us to terminate
                    // the handshake process and it does not expect the handshake reply
                    ProgramInfo.ConstructInPlace();
                } else {
                    LOG_LOG_IC_X(NActorsServices::INTERCONNECT, "ICH08", NLog::PRI_NOTICE,
                        "Continuation request rejected by proxy");

                    // report continuation reject to peer
                    SelfVirtualId = TActorId();
                    PeerVirtualId = TActorId();
                    NextPacketToPeer = 0;
                }

                // issue response to the peer
                SendInitialPacket();
            } else {
                // peer wants a new session, clear fields and send initial packet
                SelfVirtualId = TActorId();
                PeerVirtualId = TActorId();
                NextPacketToPeer = 0;
                SendInitialPacket();

                // wait for extended request
                auto ev = MakeHolder<TEvHandshakeRequest>();
                auto& request = ev->Record;
                if (!request.ParseFromString(ReceiveExBlock("ExRequest"))) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "Incorrect THandshakeRequest");
                }
                ReportProto(request, "ReceiveExBlock ExRequest");

                auto generateError = [this](TString msg) {
                    // issue reply to the peer to prevent repeating connection retries
                    NActorsInterconnect::THandshakeReply reply;
                    reply.SetErrorExplaination(msg);
                    SendExBlock(reply, "ExReply");

                    // terminate ths handshake
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, msg);
                };

                // check request cookie
                TString error;
                if (request.HasDoCheckCookie()) {
                    NActorsInterconnect::THandshakeReply reply;
                    reply.SetCookieCheckResult(request.GetCookie() == Common->Cookie);
                    SendExBlock(reply, "ExReplyDoCheckCookie");
                    throw TExHandshakeFailed();
                } else if (request.HasCookie() && !CheckPeerCookie(request.GetCookie(), &error)) {
                    generateError(TStringBuilder() << "Peer connectivity-checking failed, error# " << error);
                }

                // update log prefix with the reported peer host name
                PeerHostName = request.GetSenderHostName();

                // parse peer virtual id
                const auto& str = request.GetSenderActorId();
                PeerVirtualId.Parse(str.data(), str.size());

                // validate request
                ValidateClusterUUID(request, generateError, request.GetUUID());
                if (request.GetReceiverNodeId() != SelfActorId.NodeId()) {
                    generateError(Sprintf("Incorrect ReceiverNodeId# %" PRIu32 " from the peer, expected# %" PRIu32,
                        request.GetReceiverNodeId(), SelfActorId.NodeId()));
                } else if (request.GetReceiverHostName() != Common->TechnicalSelfHostName) {
                    generateError(Sprintf("ReceiverHostName# %s mismatch, expected# %s", request.GetReceiverHostName().data(),
                        Common->TechnicalSelfHostName.data()));
                }
                ValidateCompatibilityInfo(request, generateError);

                // check peer node
                auto peerNodeInfo = GetPeerNodeInfo();
                if (!peerNodeInfo) {
                    generateError("Peer node not registered in nameservice");
                } else if (peerNodeInfo->Host != request.GetSenderHostName()) {
                    generateError("SenderHostName mismatch");
                }

                // check request against encryption
                switch (Common->Settings.EncryptionMode) {
                    case EEncryptionMode::DISABLED:
                        if (request.GetRequireEncryption()) {
                            generateError("Peer requested encryption, but it is disabled locally");
                        }
                        break;

                    case EEncryptionMode::OPTIONAL:
                        Params.Encryption = request.HasRequireEncryption();
                        break;

                    case EEncryptionMode::REQUIRED:
                        if (!request.HasRequireEncryption()) {
                            generateError("Peer did not request encryption, but it is required locally");
                        }
                        Params.Encryption = true;
                        break;
                }

                Params.UseModernFrame = request.GetRequestModernFrame();
                Params.AuthOnly = Params.Encryption && request.GetRequestAuthOnly() && Common->Settings.TlsAuthOnly;
                Params.UseExtendedTraceFmt = request.GetRequestExtendedTraceFmt();
                Params.UseExternalDataChannel = request.GetRequestExternalDataChannel() && Common->Settings.EnableExternalDataChannel;

                if (request.HasClientScopeId()) {
                    ParsePeerScopeId(request.GetClientScopeId());
                }

                // remember program info (assuming successful handshake)
                ProgramInfo = GetProgramInfo(request);

                // send to proxy
                auto reply = AskProxy<TEvHandshakeReplyOK, TEvHandshakeReplyError>(std::move(ev), "TEvHandshakeRequest");

                // parse it
                if (auto ev = reply->CastAsLocal<TEvHandshakeReplyOK>()) {
                    // issue successful reply to the peer
                    auto& record = ev->Record;
                    Y_VERIFY(record.HasSuccess());
                    auto& success = *record.MutableSuccess();
                    SetupClusterUUID(success);
                    SetupCompatibilityInfo(success);
                    SetupVersionTag(success);
                    success.SetStartEncryption(Params.Encryption);
                    if (Common->LocalScopeId != TScopeId()) {
                        FillInScopeId(*success.MutableServerScopeId());
                    }
                    success.SetUseModernFrame(Params.UseModernFrame);
                    success.SetAuthOnly(Params.AuthOnly);
                    success.SetUseExtendedTraceFmt(Params.UseExtendedTraceFmt);
                    success.SetUseExternalDataChannel(Params.UseExternalDataChannel);
                    SendExBlock(record, "ExReply");

                    // extract sender actor id (self virtual id)
                    const auto& str = success.GetSenderActorId();
                    SelfVirtualId.Parse(str.data(), str.size());
                } else if (auto ev = reply->CastAsLocal<TEvHandshakeReplyError>()) {
                    // in case of error just send reply to the peer and terminate handshake
                    SendExBlock(ev->Record, "ExReply");
                    ProgramInfo.Clear(); // do not issue reply to the proxy
                } else {
                    Y_FAIL("unexpected event Type# 0x%08" PRIx32, reply->GetTypeRewrite());
                }
            }
        }

        template <typename T>
        void SendExBlock(const T& proto, const char* what) {
            TString data;
            Y_PROTOBUF_SUPPRESS_NODISCARD proto.SerializeToString(&data);
            Y_VERIFY(data.size() <= TExHeader::MaxSize);

            ReportProto(proto, Sprintf("SendExBlock %s", what).data());

            TExHeader header;
            header.Size = data.size();
            header.Sign(data.data(), data.size());
            SendData(&header, sizeof(header), Sprintf("Send%sHeader", what));
            SendData(data.data(), data.size(), Sprintf("Send%sData", what));
        }

        TString ReceiveExBlock(const char* what) {
            TExHeader header;
            ReceiveData(&header, sizeof(header), Sprintf("Receive%sHeader", what));
            if (header.Size > TExHeader::MaxSize) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "Incorrect extended header size");
            }

            TString data;
            data.resize(header.Size);
            ReceiveData(data.Detach(), data.size(), Sprintf("Receive%sData", what));

            if (!header.Check(data.data(), data.size())) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_TRANSIENT, "Extended header CRC error");
            }

            return data;
        }

    private:
        void SendToProxy(THolder<IEventBase> ev) {
            Y_VERIFY(PeerNodeId);
            Send(GetActorSystem()->InterconnectProxy(PeerNodeId), ev.Release());
        }

        template <typename TEvent>
        THolder<typename TEvent::THandle> WaitForSpecificEvent(TString state, TMonotonic deadline = TMonotonic::Max()) {
            State = std::move(state);
            return TActorCoroImpl::WaitForSpecificEvent<TEvent>(&THandshakeActor::ProcessUnexpectedEvent, deadline);
        }

        template <typename T1, typename T2, typename... TEvents>
        THolder<IEventHandle> WaitForSpecificEvent(TString state, TMonotonic deadline = TMonotonic::Max()) {
            State = std::move(state);
            return TActorCoroImpl::WaitForSpecificEvent<T1, T2, TEvents...>(&THandshakeActor::ProcessUnexpectedEvent, deadline);
        }

        template <typename TEvent>
        THolder<typename TEvent::THandle> AskProxy(THolder<IEventBase> ev, TString state) {
            SendToProxy(std::move(ev));
            return WaitForSpecificEvent<TEvent>(std::move(state));
        }

        template <typename T1, typename T2, typename... TOther>
        THolder<IEventHandle> AskProxy(THolder<IEventBase> ev, TString state) {
            SendToProxy(std::move(ev));
            return WaitForSpecificEvent<T1, T2, TOther...>(std::move(state));
        }

        void Fail(TEvHandshakeFail::EnumHandshakeFail reason, TString explanation, bool network = false) {
            TString msg = Sprintf("%s Peer# %s(%s) %s%s", HandshakeKind.data(), PeerHostName ? PeerHostName.data() : "<unknown>",
                PeerAddr.size() ? PeerAddr.data() : "<unknown>", ResolveTimedOut ? "[resolve timeout] " : "",
                explanation.data());

            if (network) {
                TInstant now = Now();
                TInstant prevLog = LastLogNotice[PeerNodeId];
                NActors::NLog::EPriority logPriority = NActors::NLog::PRI_DEBUG;
                if (now - prevLog > MuteDuration) {
                    logPriority = NActors::NLog::PRI_NOTICE;
                    LastLogNotice[PeerNodeId] = now;
                }
                LOG_LOG_NET_X(logPriority, PeerNodeId, "network-related error occured on handshake: %s", msg.data());
            } else {
                // calculate log severity based on failure type; permanent failures lead to error log messages
                auto severity = reason == TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT
                                    ? NActors::NLog::PRI_NOTICE
                                    : NActors::NLog::PRI_INFO;

                LOG_LOG_IC_X(NActorsServices::INTERCONNECT, "ICH03", severity, "handshake failed, explanation# %s", msg.data());
            }

            if (PeerNodeId) {
                SendToProxy(MakeHolder<TEvHandshakeFail>(reason, std::move(msg)));
            }

            throw TExHandshakeFailed() << explanation;
        }

    private:
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // COMMUNICATION BLOCK
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        void Connect(bool updatePeerAddr) {
            // issue request to a nameservice to resolve peer node address
            const auto mono = TActivationContext::Monotonic();
            Send(Common->NameserviceId, new TEvInterconnect::TEvResolveNode(PeerNodeId, TActivationContext::Now() + (Deadline - mono)));

            // wait for the result
            auto ev = WaitForSpecificEvent<TEvResolveError, TEvLocalNodeInfo, TEvInterconnect::TEvNodeAddress>("ResolveNode",
                mono + ResolveTimeout);

            // extract address from the result
            std::vector<NInterconnect::TAddress> addresses;
            if (!ev) {
                ResolveTimedOut = true;
                if (auto peerNodeInfo = GetPeerNodeInfo(); peerNodeInfo && peerNodeInfo->Address) {
                    addresses.emplace_back(peerNodeInfo->Address, peerNodeInfo->Port);
                } else {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "DNS resolve timed out and no static address defined", true);
                }
            } else if (auto *p = ev->CastAsLocal<TEvLocalNodeInfo>()) {
                addresses = std::move(p->Addresses);
                if (addresses.empty()) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "DNS resolve error: no address returned", true);
                }
            } else if (auto *p = ev->CastAsLocal<TEvInterconnect::TEvNodeAddress>()) {
                const auto& r = p->Record;
                if (!r.HasAddress() || !r.HasPort()) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "DNS resolve error: no address returned", true);
                }
                addresses.emplace_back(r.GetAddress(), static_cast<ui16>(r.GetPort()));
            } else {
                Y_VERIFY(ev->GetTypeRewrite() == ui32(ENetwork::ResolveError));
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "DNS resolve error: " + ev->Get<TEvResolveError>()->Explain
                    + ", Unresolved host# " + ev->Get<TEvResolveError>()->Host, true);
            }

            for (const NInterconnect::TAddress& address : addresses) {
                // create the socket with matching address family
                int err = 0;
                Socket = NInterconnect::TStreamSocket::Make(address.GetFamily(), &err);
                if (err == EAFNOSUPPORT) {
                    Socket.Reset();
                    continue;
                } else if (*Socket == -1) {
                    Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "System error: failed to create socket");
                }

                // extract peer address
                if (updatePeerAddr) {
                    PeerAddr = address.ToString();
                }

                // set up socket parameters
                SetupSocket();

                // start connecting
                err = -Socket->Connect(address);
                if (err == EINPROGRESS) {
                    RegisterInPoller();
                    WaitPoller(false, true, "WaitConnect");
                    err = Socket->GetConnectStatus();
                } else if (!err) {
                    RegisterInPoller();
                }

                // check if connection succeeded
                if (err) {
                    Socket.Reset();
                    PollerToken.Reset();
                } else {
                    break;
                }
            }

            if (!Socket) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "Couldn't connect to any resolved address", true);
            }

            auto it = LastLogNotice.find(PeerNodeId);
            NActors::NLog::EPriority logPriority = NActors::NLog::PRI_DEBUG;
            if (it != LastLogNotice.end()) {
                LastLogNotice.erase(it);
                logPriority = NActors::NLog::PRI_NOTICE;
            }
            LOG_LOG_IC_X(NActorsServices::INTERCONNECT, "ICH05", logPriority, "connected to peer");
        }

        void SetupSocket() {
            // switch to nonblocking mode
            try {
                SetNonBlock(*Socket);
                SetNoDelay(*Socket, true);
            } catch (...) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_PERMANENT, "System error: can't up nonblocking mode for socket");
            }

            // setup send buffer size
            Socket->SetSendBufferSize(Common->Settings.GetSendBufferSize());
        }

        void RegisterInPoller() {
            const bool success = Send(MakePollerActorId(), new TEvPollerRegister(Socket, SelfActorId, SelfActorId));
            Y_VERIFY(success);
            auto result = WaitForSpecificEvent<TEvPollerRegisterResult>("RegisterPoller");
            PollerToken = std::move(result->Get()->PollerToken);
            Y_VERIFY(PollerToken);
            Y_VERIFY(PollerToken->RefCount() == 1); // ensure exclusive ownership
        }

        void SendInitialPacket() {
            TInitialPacket packet(SelfVirtualId, PeerVirtualId, NextPacketToPeer, INTERCONNECT_PROTOCOL_VERSION);
            SendData(&packet, sizeof(packet), "SendInitialPacket");
        }

        void WaitPoller(bool read, bool write, TString state) {
            PollerToken->Request(read, write);
            WaitForSpecificEvent<TEvPollerReady>(std::move(state));
        }

        template <typename TDataPtr, typename TSendRecvFunc>
        void Process(TDataPtr buffer, size_t len, TSendRecvFunc&& sendRecv, bool read, bool write, TString state) {
            Y_VERIFY(Socket);
            NInterconnect::TStreamSocket* sock = Socket.Get();
            ssize_t (NInterconnect::TStreamSocket::*pfn)(TDataPtr, size_t, TString*) const = sendRecv;
            size_t processed = 0;

            auto error = [&](TString msg) {
                Fail(TEvHandshakeFail::HANDSHAKE_FAIL_TRANSIENT, Sprintf("Socket error# %s state# %s processed# %zu remain# %zu",
                    msg.data(), state.data(), processed, len), true);
            };

            while (len) {
                TString err;
                ssize_t nbytes = (sock->*pfn)(buffer, len, &err);
                if (nbytes > 0) {
                    buffer = (char*)buffer + nbytes;
                    len -= nbytes;
                    processed += nbytes;
                } else if (-nbytes == EAGAIN || -nbytes == EWOULDBLOCK) {
                    WaitPoller(read, write, state);
                } else if (!nbytes) {
                    error("connection unexpectedly closed");
                } else if (-nbytes != EINTR) {
                    error(err ? err : TString(strerror(-nbytes)));
                }
            }
        }

        void SendData(const void* buffer, size_t len, TString state) {
            Process(buffer, len, &NInterconnect::TStreamSocket::Send, false, true, std::move(state));
        }

        void ReceiveData(void* buffer, size_t len, TString state) {
            Process(buffer, len, &NInterconnect::TStreamSocket::Recv, true, false, std::move(state));
        }

        THolder<TEvInterconnect::TNodeInfo> GetPeerNodeInfo() {
            Y_VERIFY(PeerNodeId);
            Send(Common->NameserviceId, new TEvInterconnect::TEvGetNode(PeerNodeId, TActivationContext::Now() +
                (Deadline - TActivationContext::Monotonic())));
            auto response = WaitForSpecificEvent<TEvInterconnect::TEvNodeInfo>("GetPeerNodeInfo");
            return std::move(response->Get()->Node);
        }

        template <typename T>
        static THolder<TProgramInfo> GetProgramInfo(const T& proto) {
            auto programInfo = MakeHolder<TProgramInfo>();
            programInfo->PID = proto.GetProgramPID();
            programInfo->StartTime = proto.GetProgramStartTime();
            programInfo->Serial = proto.GetSerial();
            return programInfo;
        }
    };

    IActor* CreateOutgoingHandshakeActor(TInterconnectProxyCommon::TPtr common, const TActorId& self,
                                         const TActorId& peer, ui32 nodeId, ui64 nextPacket, TString peerHostName,
                                         TSessionParams params) {
        return new TActorCoro(MakeHolder<THandshakeActor>(std::move(common), self, peer, nodeId, nextPacket,
            std::move(peerHostName), std::move(params)), IActor::INTERCONNECT_HANDSHAKE);
    }

    IActor* CreateIncomingHandshakeActor(TInterconnectProxyCommon::TPtr common, TSocketPtr socket) {
        return new TActorCoro(MakeHolder<THandshakeActor>(std::move(common), std::move(socket)),
            IActor::INTERCONNECT_HANDSHAKE);
    }

}
