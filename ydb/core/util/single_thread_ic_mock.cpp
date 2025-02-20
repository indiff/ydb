#include "single_thread_ic_mock.h"
#include "testactorsys.h"
#include "stlog.h"

using namespace NActors;
using namespace NKikimr;

using TMock = TSingleThreadInterconnectMock;

struct TMock::TNode {
    const ui32 NodeId;
    const ui64 BurstCapacityBytes;
    const ui64 BytesPerSecond;
    TInstant NextActionTimestamp;

    TNode(ui32 nodeId, ui64 burstCapacityBytes, ui64 bytesPerSecond)
        : NodeId(nodeId)
        , BurstCapacityBytes(burstCapacityBytes)
        , BytesPerSecond(bytesPerSecond)
    {}

    TInstant Enqueue(ui64 size, TInstant clock) {
        if (BytesPerSecond == Max<ui64>()) {
            return clock;
        }
        NextActionTimestamp = Max(clock - TDuration::MicroSeconds(BurstCapacityBytes * 1'000'000 / BytesPerSecond),
            NextActionTimestamp) + TDuration::MicroSeconds((size * 1'000'000 + BytesPerSecond - 1) / BytesPerSecond);
        return Max(NextActionTimestamp, clock);
    }
};

class TMock::TProxyActor : public TActor<TProxyActor> {
    friend class TSessionActor;

    const TString Prefix;
    TMock* const Mock;
    std::shared_ptr<TNode> Node;
    TProxyActor *Peer;
    const ui32 PeerNodeId;
    TIntrusivePtr<TInterconnectProxyCommon> Common;
    TSessionActor *SessionActor = nullptr;
    bool Working = false;
    std::deque<std::unique_ptr<IEventHandle>> PendingEvents;
    ui64 DropPendingEventsCookie = 0;

    enum {
        EvDropPendingEvents = EventSpaceBegin(TEvents::ES_PRIVATE),
    };

public:
    TProxyActor(TMock *mock, std::shared_ptr<TNode> node, TProxyActor *peer, ui32 peerNodeId,
            TIntrusivePtr<TInterconnectProxyCommon> common)
        : TActor(&TThis::StateFunc)
        , Prefix(TStringBuilder() << "Proxy[" << node->NodeId << ":" << peerNodeId << "] ")
        , Mock(mock)
        , Node(std::move(node))
        , Peer(peer)
        , PeerNodeId(peerNodeId)
        , Common(std::move(common))
    {
        if (Peer) {
            Peer->Peer = this;
        }
    }

    ~TProxyActor();

    void Registered(TActorSystem *as, const TActorId& parent) override {
        TActor::Registered(as, parent);
        Working = true;
        if (Peer && Peer->Working) {
            ProcessPendingEvents();
            Peer->ProcessPendingEvents();
        }
    }

    TSessionActor *GetSession() {
        if (!SessionActor && Working && Peer && Peer->Working) {
            Y_VERIFY(PendingEvents.empty());
            Y_VERIFY(Peer->PendingEvents.empty());
            CreateSession();
            Peer->CreateSession();
        }
        return SessionActor;
    }

    void CreateSession();
    void ForwardToSession(TAutoPtr<IEventHandle> ev);

    void DropSessionEvent(std::unique_ptr<IEventHandle> ev);
    void HandleDropPendingEvents(TAutoPtr<IEventHandle> ev);
    void ProcessPendingEvents();

    void ShutdownConnection();
    void DetachSessionActor();

    STRICT_STFUNC(StateFunc,
        fFunc(EvDropPendingEvents, HandleDropPendingEvents);
        fFunc(TEvInterconnect::EvForward, ForwardToSession);
        fFunc(TEvInterconnect::EvConnectNode, ForwardToSession);
        fFunc(TEvents::TSystem::Subscribe, ForwardToSession);
        fFunc(TEvents::TSystem::Unsubscribe, ForwardToSession);
        cFunc(TEvInterconnect::EvDisconnect, ShutdownConnection);
    )
};

class TMock::TSessionActor : public TActor<TSessionActor> {
    const TString Prefix;
    TProxyActor *Proxy;
    std::unordered_map<TActorId, ui64, THash<TActorId>> Subscribers;

    std::unordered_map<ui16, std::deque<std::unique_ptr<IEventHandle>>> Outbox;
    bool SendPending = false;
    TInstant NextSendTimestamp;

    std::deque<std::unique_ptr<IEventHandle>> Inbox;
    bool ReceivePending = false;
    TInstant NextReceiveTimestamp;

    enum {
        EvSend = EventSpaceBegin(TEvents::ES_PRIVATE),
        EvReceive,
    };

public:
    // The SessionActor exists when and only when there are two working peers connected together and both of them have
    // their respective sessions. That is, when one of these conditions break, we have to terminate both sessions of a
    // connection. There are also some other things to consider: session can be destroyed via dtor after proxy destruction,
    // so it can't access proxy's fields from its destruction path. It is peer proxy's responsibility to destroy both
    // sessions upon destruction.

    TSessionActor(TProxyActor *proxy)
        : TActor(&TThis::StateFunc)
        , Prefix(TStringBuilder() << "Session[" << proxy->Node->NodeId << ":" << proxy->PeerNodeId << "] ")
        , Proxy(proxy)
    {}

    ~TSessionActor() {
        if (Proxy) {
            Proxy->SessionActor = nullptr;
        }
    }

    // Shut down local part of this session.
    void ShutdownSession() {
        if (!Proxy) {
            return;
        }

        STLOG(PRI_DEBUG, INTERCONNECT_SESSION, STIM01, Prefix << "ShutdownSession", (SelfId, SelfId()));

        // notify all subscribers
        for (const auto& [actorId, cookie] : Subscribers) {
            auto ev = std::make_unique<TEvInterconnect::TEvNodeDisconnected>(Proxy->PeerNodeId);
            Proxy->Mock->TestActorSystem->Send(new IEventHandleFat(actorId, SelfId(), ev.release(), 0, cookie),
                Proxy->Node->NodeId);
        }

        // drop unsent messages
        for (auto& [_, queue] : Outbox) {
            for (auto& ev : queue) {
                Proxy->Mock->TestActorSystem->Send(IEventHandle::ForwardOnNondelivery(ev, TEvents::TEvUndelivered::Disconnected).Release(),
                    Proxy->Node->NodeId);
            }
        }

        // transit to self-destruction state
        Proxy->Mock->TestActorSystem->Send(new IEventHandleFat(TEvents::TSystem::Poison, 0, SelfId(), {}, nullptr, 0),
            Proxy->Node->NodeId);
        Become(&TThis::StateUndelivered); // do not handle further events

        // no more proxy connected to this actor
        Proxy = nullptr;
    }

    void StateUndelivered(STFUNC_SIG) {
        Y_UNUSED(ctx);
        if (ev->GetTypeRewrite() == TEvents::TSystem::Poison) {
            TActor::PassAway();
        } else {
            TActivationContext::Send(IEventHandle::ForwardOnNondelivery(ev, TEvents::TEvUndelivered::ReasonActorUnknown));
        }
    }

    void PassAway() override {
        Proxy->ShutdownConnection();
        TActor::PassAway();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void HandleForward(TAutoPtr<IEventHandle> ev) {
        STLOG(PRI_DEBUG, INTERCONNECT_SESSION, STIM02, Prefix << "HandleForward", (SelfId, SelfId()),
            (Type, ev->Type), (TypeName, Proxy->Mock->TestActorSystem->GetEventName(ev->Type)),
            (Sender, ev->Sender), (Recipient, ev->Recipient), (Flags, ev->Flags), (Cookie, ev->Cookie));

        if (ev->Flags & IEventHandle::FlagSubscribeOnSession) {
            Subscribe(ev->Sender, ev->Cookie);
        }
        if (SendPending) {
            const ui16 ch = ev->GetChannel();
            Outbox[ch].emplace_back(ev.Release());
        } else {
            ScheduleSendEvent(ev);
            HandleSend(ev);
        }
    }

    void HandleSend(TAutoPtr<IEventHandle> ev) {
        while (ev) {
            STLOG(PRI_TRACE, INTERCONNECT_SESSION, STIM03, Prefix << "HandleSend", (SelfId, SelfId()),
                (Type, ev->Type), (Sender, ev->Sender), (Recipient, ev->Recipient), (Flags, ev->Flags),
                (Cookie, ev->Cookie));

            const TInstant now = TActivationContext::Now();
            Y_VERIFY(now == NextSendTimestamp);
            Y_VERIFY(Proxy->Peer && Proxy->Peer->SessionActor);
            if (ev->IsEventFat()) {
                auto* evf = IEventHandleFat::GetFat(ev);
                const_cast<TScopeId&>(evf->OriginScopeId) = Proxy->Common->LocalScopeId;
            }
            Proxy->Peer->SessionActor->PutToInbox(ev);

            if (Outbox.empty()) {
                SendPending = false;
                break;
            } else {
                auto it = Outbox.begin();
                std::advance(it, RandomNumber(Outbox.size()));
                auto& q = it->second;
                ev = q.front().release();
                q.pop_front();
                if (q.empty()) {
                    Outbox.erase(it);
                }

                ScheduleSendEvent(ev);
            }
        }
    }

    void ScheduleSendEvent(TAutoPtr<IEventHandle>& ev) {
        const TInstant now = TActivationContext::Now();
        NextSendTimestamp = Proxy->Node->Enqueue(ev->GetSize(), now);
        if (now < NextSendTimestamp) {
            ev->Rewrite(EvSend, SelfId());
            TActivationContext::Schedule(NextSendTimestamp, ev.Release());
            SendPending = true;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void PutToInbox(TAutoPtr<IEventHandle> ev) {
        if (ReceivePending) {
            Inbox.emplace_back(ev.Release());
        } else {
            ScheduleReceiveEvent(ev);
            HandleReceive(ev);
        }
    }

    void HandleReceive(TAutoPtr<IEventHandle> ev) {
        while (ev) {
            const TInstant now = TActivationContext::Now();
            Y_VERIFY(now == NextReceiveTimestamp);

            std::unique_ptr<IEventHandle> fw;
            if (ev->IsEventFat()) {
                auto* evf = IEventHandleFat::GetFat(ev);
                fw = std::make_unique<IEventHandleFat>(
                    SelfId(),
                    evf->Type,
                    evf->Flags & ~IEventHandle::FlagForwardOnNondelivery,
                    evf->Recipient,
                    evf->Sender,
                    evf->ReleaseChainBuffer(),
                    evf->Cookie,
                    evf->OriginScopeId,
                    std::move(evf->TraceId)
                );
            } else {
                fw.reset(ev.Release());
            }

            STLOG(PRI_TRACE, INTERCONNECT_SESSION, STIM04, Prefix << "HandleReceive", (SelfId, SelfId()),
                (Type, fw->Type), (Sender, fw->Sender), (Recipient, fw->Recipient), (Flags, fw->Flags),
                (Cookie, ev->Cookie));

            if (fw->IsEventFat()) {
                auto& common = Proxy->Common;
                if (!common->EventFilter || common->EventFilter->CheckIncomingEvent(*IEventHandleFat::GetFat(fw), common->LocalScopeId)) {
                    Proxy->Mock->TestActorSystem->Send(fw.release(), Proxy->Node->NodeId);
                }
            }

            if (Inbox.empty()) {
                ReceivePending = false;
                break;
            } else {
                ev = Inbox.front().release();
                Inbox.pop_front();
                ScheduleReceiveEvent(ev);
            }
        }
    }

    void ScheduleReceiveEvent(TAutoPtr<IEventHandle>& ev) {
        const TInstant now = TActivationContext::Now();
        NextReceiveTimestamp = Proxy->Node->Enqueue(ev->GetSize(), now);
        if (now < NextReceiveTimestamp) {
            ev->Rewrite(EvReceive, SelfId());
            Proxy->Mock->TestActorSystem->Schedule(NextReceiveTimestamp, ev.Release(), nullptr, Proxy->Node->NodeId);
            ReceivePending = true;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void Handle(TEvInterconnect::TEvConnectNode::TPtr ev) {
        Subscribe(ev->Sender, ev->Cookie);
    }

    void Handle(TEvents::TEvSubscribe::TPtr ev) {
        Subscribe(ev->Sender, ev->Cookie);
    }

    void Subscribe(const TActorId& actorId, ui64 cookie) {
        Subscribers[actorId] = cookie;
        Send(actorId, new TEvInterconnect::TEvNodeConnected(Proxy->PeerNodeId), 0, cookie);
    }

    void Handle(TEvents::TEvUnsubscribe::TPtr ev) {
        Subscribers.erase(ev->Sender);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    STRICT_STFUNC(StateFunc,
        fFunc(TEvInterconnect::EvForward, HandleForward);
        fFunc(EvSend, HandleSend);
        fFunc(EvReceive, HandleReceive);
        hFunc(TEvInterconnect::TEvConnectNode, Handle);
        hFunc(TEvents::TEvSubscribe, Handle);
        hFunc(TEvents::TEvUnsubscribe, Handle);
        cFunc(TEvents::TSystem::Poison, PassAway);
    )
};

TMock::TProxyActor::~TProxyActor() {
    const auto it = Mock->Proxies.find({Node->NodeId, PeerNodeId});
    Y_VERIFY(it != Mock->Proxies.end());
    Y_VERIFY(it->second == this);
    Mock->Proxies.erase(it);

    if (Peer) {
        Peer->DetachSessionActor();
        Peer->Peer = nullptr;
    }
    DetachSessionActor();
}

void TMock::TProxyActor::CreateSession() {
    Y_VERIFY(SelfId());
    Y_VERIFY(Working);
    Y_VERIFY(!SessionActor);
    SessionActor = new TSessionActor(this);
    const TActorId self = SelfId();
    Mock->TestActorSystem->Register(SessionActor, self, self.PoolID(), self.Hint(), Node->NodeId);
}

void TMock::TProxyActor::ForwardToSession(TAutoPtr<IEventHandle> ev) {
    if (TSessionActor *session = GetSession()) {
        InvokeOtherActor(*session, &TSessionActor::Receive, ev);
    } else {
        const bool first = PendingEvents.empty();
        PendingEvents.emplace_back(ev.Release());
        if (first) {
            TActivationContext::Schedule(TDuration::Seconds(5), new IEventHandleFat(EvDropPendingEvents, 0, SelfId(), {},
                nullptr, ++DropPendingEventsCookie));
        }
    }
}

void TMock::TProxyActor::DropSessionEvent(std::unique_ptr<IEventHandle> ev) {
    switch (ev->GetTypeRewrite()) {
        case TEvInterconnect::EvForward:
            if (ev->Flags & IEventHandle::FlagSubscribeOnSession) {
                Send(ev->Sender, new TEvInterconnect::TEvNodeDisconnected(PeerNodeId), 0, ev->Cookie);
            }
            TActivationContext::Send(IEventHandle::ForwardOnNondelivery(ev, TEvents::TEvUndelivered::Disconnected));
            break;

        case TEvInterconnect::EvConnectNode:
        case TEvents::TSystem::Subscribe:
            Send(ev->Sender, new TEvInterconnect::TEvNodeDisconnected(PeerNodeId), 0, ev->Cookie);
            break;

        case TEvents::TSystem::Unsubscribe:
            break;

        default:
            Y_FAIL();
    }
}

void TMock::TProxyActor::HandleDropPendingEvents(TAutoPtr<IEventHandle> ev) {
    if (ev->Cookie == DropPendingEventsCookie) {
        for (auto& ev : std::exchange(PendingEvents, {})) {
            DropSessionEvent(std::move(ev));
        }
    }
}

void TMock::TProxyActor::ProcessPendingEvents() {
    for (auto& ev : std::exchange(PendingEvents, {})) {
        TSessionActor *session = GetSession();
        Y_VERIFY(session);
        Mock->TestActorSystem->Send(ev.release(), Node->NodeId);
    }
}

void TMock::TProxyActor::ShutdownConnection() {
    Y_VERIFY(Peer && ((SessionActor && Peer->SessionActor) || (!SessionActor && !Peer->SessionActor)));
    DetachSessionActor();
    Peer->DetachSessionActor();
}

void TMock::TProxyActor::DetachSessionActor() {
    if (auto *p = std::exchange(SessionActor, nullptr)) {
        p->ShutdownSession();
    }
}

TMock::TSingleThreadInterconnectMock(ui64 burstCapacityBytes, ui64 bytesPerSecond,
        TTestActorSystem *tas)
    : BurstCapacityBytes(burstCapacityBytes)
    , BytesPerSecond(bytesPerSecond)
    , TestActorSystem(tas)
{}

TMock::~TSingleThreadInterconnectMock()
{}

std::unique_ptr<IActor> TMock::CreateProxyActor(ui32 nodeId, ui32 peerNodeId,
        TIntrusivePtr<TInterconnectProxyCommon> common) {
    Y_VERIFY(nodeId != peerNodeId);

    auto& ptr = Proxies[{nodeId, peerNodeId}];
    Y_VERIFY(!ptr); // no multiple proxies for the same direction are allowed

    auto& node = Nodes[nodeId];
    if (!node) {
        node = std::make_shared<TNode>(nodeId, BurstCapacityBytes, BytesPerSecond);
    }

    const auto it = Proxies.find({peerNodeId, nodeId});
    TProxyActor *peer = it != Proxies.end() ? it->second : nullptr;

    auto proxy = std::make_unique<TProxyActor>(this, node, peer, peerNodeId, std::move(common));
    ptr = proxy.get();
    return proxy;
}
