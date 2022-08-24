#include "pg_connection.h"
#include "pg_proxy_config.h"
#include "pg_proxy_types.h"
#include "pg_proxy_events.h"
#include "pg_stream.h"
#include "pg_log_impl.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>

namespace NPG {

using namespace NActors;

class TPGConnection : public TActorBootstrapped<TPGConnection>, public TNetworkConfig {
public:
    enum class EMessageCode : char {
        Initial = 'i',
        Query = 'Q',
        Terminate = 'X',
        PasswordMessage = 'p',
    };

    using TBase = TActorBootstrapped<TPGConnection>;
    TIntrusivePtr<TSocketDescriptor> Socket;
    TSocketAddressType Address;
    THPTimer InactivityTimer;
    static constexpr TDuration InactivityTimeout = TDuration::Minutes(10);
    TEvPollerReady* InactivityEvent = nullptr;
    bool IsAuthRequired = true;
    bool IsSslSupported = true;
    bool ConnectionEstablished = false;
    bool CloseConnection = false;
    bool PasswordWasSupplied = false;
    TPollerToken::TPtr PollerToken;
    TSocketBuffer BufferInput;
    std::unordered_map<TString, TString> ServerParams = {
        {"client_encoding", "UTF8"},
        {"server_encoding", "UTF8"},
        {"DateStyle", "ISO"},
        {"IntervalStyle", "postgres"},
        {"integer_datetimes", "on"},
    };
    std::unordered_map<TString, TString> ClientParams;
    TSocketBuffer BufferOutput;
    TActorId DatabaseProxy;
    std::shared_ptr<TPGInitial> InitialMessage;

    TPGConnection(TIntrusivePtr<TSocketDescriptor> socket, TNetworkConfig::TSocketAddressType address, const TActorId& databaseProxy)
        : Socket(std::move(socket))
        , Address(address)
        , DatabaseProxy(databaseProxy)
    {
        SetNonBlock();
        IsSslSupported = IsSslSupported && Socket->IsSslSupported();
    }

    void Bootstrap() {
        Become(&TPGConnection::StateAccepting);
        Schedule(InactivityTimeout, InactivityEvent = new TEvPollerReady(nullptr, false, false));
        BLOG_D("incoming connection opened");
        OnAccept();
    }

    void PassAway() override {
        //ctx.Send(Endpoint->Owner, new TEvHttpProxy::TEvHttpConnectionClosed(ctx.SelfID, std::move(RecycledRequests)));
        if (ConnectionEstablished) {
            Send(DatabaseProxy, new TEvPGEvents::TEvConnectionClosed());
            ConnectionEstablished = false;
        }
        Shutdown();
        TBase::PassAway();
    }

protected:
    void SetNonBlock() noexcept {
        Socket->SetNonBlock();
    }

    void Shutdown() {
        if (Socket) {
            Socket->Shutdown();
        }
    }

    ssize_t SocketSend(const void* data, size_t size) {
        return Socket->Send(data, size);
    }

    ssize_t SocketReceive(void* data, size_t size) {
        return Socket->Receive(data, size);
    }

    void RequestPoller() {
        Socket->RequestPoller(PollerToken);
    }

    SOCKET GetRawSocket() const {
        return Socket->GetRawSocket();
    }

    TString LogPrefix() const {
        return TStringBuilder() << "(#" << GetRawSocket() << "," << Address << ") ";
    }

    void OnAccept() {
        InactivityTimer.Reset();
        TBase::Become(&TPGConnection::StateConnected);
        BufferInput.Append('i'); // initial packet pseudo-message
        Send(SelfId(), new TEvPollerReady(nullptr, true, true));
    }

    void HandleAccepting(TEvPollerRegisterResult::TPtr ev) {
        PollerToken = std::move(ev->Get()->PollerToken);
        OnAccept();
    }

    void HandleAccepting(NActors::TEvPollerReady::TPtr) {
        OnAccept();
    }

    TStringBuf GetMessageName(const TPGMessage& message) const {
        static const std::unordered_map<char, TStringBuf> messageName = {
            {'i', "Initial"},
            {'R', "Auth"},
            {'Z', "ReadyForQuery"},
            {'Q', "Query"},
            {'C', "CommandComplete"},
            {'X', "Terminate"},
            {'T', "RowDescription"},
            {'D', "DataRow"},
            {'S', "ParameterStatus"},
            {'E', "ErrorResponse"},
            {'I', "EmptyQueryResponse"},
            {'p', "PasswordMessage"},
        };
        auto itMessageName = messageName.find(message.Message);
        if (itMessageName != messageName.end()) {
            return itMessageName->second;
        }
        return {};
    }

    TString GetMessageDump(const TPGMessage& message) const {
        switch (message.Message) {
            case 'i':
                return ((const TPGInitial&)message).Dump();
            case 'Q':
                return ((const TPGQuery&)message).Dump();
            case 'S':
                return ((const TPGParameterStatus&)message).Dump();
            case 'Z':
                return ((const TPGReadyForQuery&)message).Dump();
            case 'C':
                return ((const TPGCommandComplete&)message).Dump();
            case 'R':
                return ((const TPGAuth&)message).Dump();
        }
        return {};
    }

    void PrintMessage(const TStringBuf& prefix, const TPGMessage& message) {
        BLOG_D(prefix << "'" << message.Message << "' \"" << GetMessageName(message) << "\" Size(" << ntohl(message.Length) << ") " << GetMessageDump(message));
    }

    template<typename TMessage>
    void SendMessage(const TMessage& message) {
        PrintMessage("<- ", message);
        BufferOutput.Append(reinterpret_cast<const char*>(&message), sizeof(message));
    }

    template<typename TMessage>
    void SendStream(TPGStream<TMessage>& message) {
        message.UpdateLength();
        const TPGMessage& header = *reinterpret_cast<const TPGMessage*>(message.Data());
        PrintMessage("<- ", header);
        BufferOutput.Append(message.Data(), message.Size());
    }

    void SendAuthOk() {
        TPGStream<TPGAuth> authOk;
        authOk << uint32_t(TPGAuth::EAuthCode::OK);
        SendStream(authOk);
    }

    void SendAuthClearText() {
        TPGStream<TPGAuth> authClearText;
        authClearText << uint32_t(TPGAuth::EAuthCode::ClearText);
        SendStream(authClearText);
    }
    /*
        0x0040:  70ac 68dd 70ac 68dc 5200 0000 0800 0000  p.h.p.h.R.......
        0x0050:  0053 0000 001a 6170 706c 6963 6174 696f  .S....applicatio
        0x0060:  6e5f 6e61 6d65 0070 7371 6c00 5300 0000  n_name.psql.S...
        0x0070:  1963 6c69 656e 745f 656e 636f 6469 6e67  .client_encoding
        0x0080:  0055 5446 3800 5300 0000 1744 6174 6553  .UTF8.S....DateS
        0x0090:  7479 6c65 0049 534f 2c20 4d44 5900 5300  tyle.ISO,.MDY.S.
        0x00a0:  0000 1969 6e74 6567 6572 5f64 6174 6574  ...integer_datet
        0x00b0:  696d 6573 006f 6e00 5300 0000 1b49 6e74  imes.on.S....Int
        0x00c0:  6572 7661 6c53 7479 6c65 0070 6f73 7467  ervalStyle.postg
        0x00d0:  7265 7300 5300 0000 1469 735f 7375 7065  res.S....is_supe
        0x00e0:  7275 7365 7200 6f6e 0053 0000 0019 7365  ruser.on.S....se
        0x00f0:  7276 6572 5f65 6e63 6f64 696e 6700 5554  rver_encoding.UT
        0x0100:  4638 0053 0000 0039 7365 7276 6572 5f76  F8.S...9server_v
        0x0110:  6572 7369 6f6e 0031 322e 3131 2028 5562  ersion.12.11.(Ub
        0x0120:  756e 7475 2031 322e 3131 2d30 7562 756e  untu.12.11-0ubun
        0x0130:  7475 302e 3230 2e30 342e 3129 0053 0000  tu0.20.04.1).S..
        0x0140:  0021 7365 7373 696f 6e5f 6175 7468 6f72  .!session_author
        0x0150:  697a 6174 696f 6e00 616c 6578 6579 0053  ization.alexey.S
        0x0160:  0000 0023 7374 616e 6461 7264 5f63 6f6e  ...#standard_con
        0x0170:  666f 726d 696e 675f 7374 7269 6e67 7300  forming_strings.
        0x0180:  6f6e 0053 0000 0015 5469 6d65 5a6f 6e65  on.S....TimeZone
        0x0190:  0045 7463 2f55 5443 004b 0000 000c 0004  .Etc/UTC.K......
    */
    void SendParameterStatus(TStringBuf name, TStringBuf value) {
        TPGStream<TPGParameterStatus> param;
        param << name << '\0' << value << '\0';
        SendStream(param);
    }

    void SendReadyForQuery() {
        TPGStream<TPGReadyForQuery> readyForQuery;
        readyForQuery << 'I';
        SendStream(readyForQuery);
    }

    void SendAuthError(const TString& error) {
        TPGStream<TPGErrorResponse> errorResponse;
        errorResponse
            << 'S' << "FATAL" << '\0'
            << 'V' << "FATAL" << '\0'
//            << 'C' << "28P01" << '\0'
            << 'M' << error << '\0'
            << 'R' << "auth_failed" << '\0'
            << '\0';
        SendStream(errorResponse);
    }

    void FinishHandshake() {
        for (const auto& [name, value] : ServerParams) {
            SendParameterStatus(name, value);
        }
        SendReadyForQuery();
        ConnectionEstablished = true;
        Send(DatabaseProxy, new TEvPGEvents::TEvConnectionOpened(std::move(InitialMessage)));
    }

    void HandleMessage(const TPGInitial* message) {
        if (message->Protocol == 0x2f16d204) { // 790024708 SSL handshake
            if (IsSslSupported) {
                BufferOutput.Append('S');
                if (!FlushOutput()) {
                    return;
                }
                // TODO(xenoxeno): wait for reply to be sent
                if (!UpgradeToSecure()) {
                    return;
                }
                RequestPoller();
            } else {
                BLOG_D("<- 'N' \"Decline SSL\"");
                BufferOutput.Append('N');
                if (!FlushOutput()) {
                    return;
                }
                RequestPoller();
            }
            BufferInput.Append('i'); // initial packet pseudo-message
            return;
        }
        if (message->Protocol == 0x2e16d204) { // 80877102 cancellation message
            BLOG_D("cancellation message");
            CloseConnection = true;
            return;
        }
        if (message->Protocol != 0x300) {
            BLOG_W("invalid protocol version (" << Hex(message->Protocol) << ")");
            CloseConnection = true;
            return;
        }
        InitialMessage = MakePGMessageCopy(message);
        if (IsAuthRequired) {
            Send(DatabaseProxy, new TEvPGEvents::TEvAuthRequest(InitialMessage));
        } else {
            SendAuthOk();
            FinishHandshake();
        }
    }

    void HandleMessage(const TPGPasswordMessage* message) {
        PasswordWasSupplied = true;
        Send(DatabaseProxy, new TEvPGEvents::TEvAuthRequest(InitialMessage, MakePGMessageCopy(message)));
        return;
    }

    void HandleMessage(const TPGQuery* message) {
        if (message->GetQuery().empty()) {
            SendMessage(TPGEmptyQueryResponse());
        } else {
            Send(DatabaseProxy, new TEvPGEvents::TEvQuery(MakePGMessageCopy(message)));
        }
    }

    void HandleMessage(const TPGTerminate*) {
        CloseConnection = true;
    }

    void HandleConnected(TEvPGEvents::TEvAuthResponse::TPtr& ev) {
        if (ev->Get()->Error) {
            if (PasswordWasSupplied) {
                SendAuthError(ev->Get()->Error);
                CloseConnection = true;
            } else {
                SendAuthClearText();
            }
        } else {
            SendAuthOk();
            FinishHandshake();
        }

        if (!FlushOutput()) {
            return;
        }
        RequestPoller();
    }

    void HandleConnected(TEvPGEvents::TEvRowDescription::TPtr& ev) {
        TPGStream<TPGRowDescription> rowDescription;
        rowDescription << uint16_t(ev->Get()->Fields.size()); // number of fields
        for (const auto& field : ev->Get()->Fields) {
            rowDescription
                << TStringBuf(field.Name) << '\0'
                << uint32_t(field.TableId)
                << uint16_t(field.ColumnId)
                << uint32_t(field.DataType)
                << uint16_t(field.DataTypeSize)
                << uint32_t(0xffffffff) // type modifier
                << uint16_t(0)          // format text
                ;
        }
        SendStream(rowDescription);

        if (!FlushOutput()) {
            return;
        }
        RequestPoller();
    }

    void HandleConnected(TEvPGEvents::TEvDataRows::TPtr& ev) {
        for (const auto& row : ev->Get()->Rows) {
            TPGStream<TPGDataRow> dataRow;
            dataRow << uint16_t(row.size()); // number of fields
            for (const auto& item : row) {
                dataRow << uint32_t(item.size()) << item;
            }
            SendStream(dataRow);
        }

        if (!FlushOutput()) {
            return;
        }
        RequestPoller();
    }

    void HandleConnected(TEvPGEvents::TEvCommandComplete::TPtr& ev) {
        TPGStream<TPGCommandComplete> commandComplete;
        commandComplete << ev->Get()->Tag << '\0';
        SendStream(commandComplete);

        SendReadyForQuery();

        if (!FlushOutput()) {
            return;
        }
        RequestPoller();
    }

    void HandleConnected(TEvPGEvents::TEvErrorResponse::TPtr& ev) {
        TPGStream<TPGErrorResponse> errorResponse;
        for (const auto& field : ev->Get()->ErrorFields) {
            errorResponse << field.first << field.second << '\0';
        }
        errorResponse << '\0';
        SendStream(errorResponse);

        SendReadyForQuery();

        if (!FlushOutput()) {
            return;
        }
        RequestPoller();
    }

    bool HasInputMessage() const {
        if (BufferInput.Size() >= sizeof(TPGMessage)) {
            const TPGMessage* message = reinterpret_cast<const TPGMessage*>(BufferInput.data());
            if (BufferInput.Size() >= message->GetMessageSize()) {
                return true;
            }
        }
        return false;
    }

    const TPGMessage* GetInputMessage() const {
        Y_VERIFY_DEBUG(HasInputMessage());
        return reinterpret_cast<const TPGMessage*>(BufferInput.data());
    }

    size_t GetInputMessageSize() const {
        return GetInputMessage()->GetMessageSize();
    }

    size_t GetInputMessageDataSize() const {
        return GetInputMessage()->GetDataSize();
    }

    void HandleConnected(TEvPollerReady::TPtr event) {
        if (event->Get()->Read) {
            for (;;) {
                ssize_t need = BufferInput.Avail();
                ssize_t res = SocketReceive(BufferInput.Pos(), need);
                if (res > 0) {
                    InactivityTimer.Reset();
                    BufferInput.Advance(res);
                    while (HasInputMessage()) {
                        const TPGMessage* message = GetInputMessage();
                        PrintMessage("-> ", *message);
                        switch (static_cast<EMessageCode>(message->Message)) {
                            case EMessageCode::Initial:
                                HandleMessage(static_cast<const TPGInitial*>(message));
                                break;
                            case EMessageCode::Query:
                                HandleMessage(static_cast<const TPGQuery*>(message));
                                break;
                            case EMessageCode::Terminate:
                                HandleMessage(static_cast<const TPGTerminate*>(message));
                                break;
                            case EMessageCode::PasswordMessage:
                                HandleMessage(static_cast<const TPGPasswordMessage*>(message));
                                break;
                            default:
                                BLOG_W("invalid message (" << message->Message << ")");
                                CloseConnection = true;
                                break;
                        }
                        BufferInput.ChopHead(GetInputMessageSize());
                    }
                    if (!FlushOutput()) {
                        return;
                    }
                } else if (-res == EAGAIN || -res == EWOULDBLOCK) {
                    break;
                } else if (-res == EINTR) {
                    continue;
                } else if (!res) {
                    // connection closed
                    BLOG_D("connection closed");
                    return PassAway();
                } else {
                    BLOG_D("connection closed - error in recv: " << strerror(-res));
                    return PassAway();
                }
            }
            if (event->Get() == InactivityEvent) {
                const TDuration passed = TDuration::Seconds(std::abs(InactivityTimer.Passed()));
                if (passed >= InactivityTimeout) {
                    BLOG_D("connection closed by inactivity timeout");
                    return PassAway(); // timeout
                } else {
                    Schedule(InactivityTimeout - passed, InactivityEvent = new TEvPollerReady(nullptr, false, false));
                }
            }
        }
        if (event->Get()->Write) {
            if (!FlushOutput()) {
                return;
            }
        }
        RequestPoller();
    }

    void HandleConnected(TEvPollerRegisterResult::TPtr ev) {
        PollerToken = std::move(ev->Get()->PollerToken);
        PollerToken->Request(true, true);
    }

    bool FlushOutput() {
        while (BufferOutput.Size() > 0) {
            ssize_t res = SocketSend(BufferOutput.Data(), BufferOutput.Size());
            if (res > 0) {
                BufferOutput.ChopHead(res);
            } else if (-res == EINTR) {
                continue;
            } else if (-res == EAGAIN || -res == EWOULDBLOCK) {
                break;
            } else {
                BLOG_ERROR("connection closed - error in FlushOutput: " << strerror(-res));
                PassAway();
                return false;
            }
        }
        if (CloseConnection && BufferOutput.Empty()) {
            BLOG_D("connection closed");
            PassAway();
            return false;
        }
        return true;
    }

    bool UpgradeToSecure() {
        for (;;) {
            int res = Socket->UpgradeToSecure();
            if (res >= 0) {
                break;
            } else if (-res == EINTR) {
                continue;
            } else if (-res == EAGAIN || -res == EWOULDBLOCK) {
                break;
            } else {
                BLOG_ERROR("connection closed - error in UpgradeToSecure: " << strerror(-res));
                PassAway();
                return false;
            }
        }
        return true;
    }

    STATEFN(StateAccepting) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvPollerReady, HandleAccepting);
            hFunc(TEvPollerRegisterResult, HandleAccepting);
        }
    }

    STATEFN(StateConnected) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvPollerReady, HandleConnected);
            hFunc(TEvPollerRegisterResult, HandleConnected);

            hFunc(TEvPGEvents::TEvAuthResponse, HandleConnected);
            hFunc(TEvPGEvents::TEvRowDescription, HandleConnected);
            hFunc(TEvPGEvents::TEvDataRows, HandleConnected);
            hFunc(TEvPGEvents::TEvCommandComplete, HandleConnected);
            hFunc(TEvPGEvents::TEvErrorResponse, HandleConnected);
        }
    }
};

NActors::IActor* CreatePGConnection(TIntrusivePtr<TSocketDescriptor> socket, TNetworkConfig::TSocketAddressType address, const TActorId& databaseProxy) {
    return new TPGConnection(std::move(socket), std::move(address), databaseProxy);
}

}