#include <ydb/core/base/tablet.h>
#include <ydb/core/base/kikimr_issue.h>
#include <ydb/core/kqp/ut/common/kqp_ut_common.h>
#include <ydb/core/kqp/proxy_service/kqp_proxy_service.h>
#include <ydb/core/kqp/common/kqp.h>
#include <ydb/core/protos/kqp.pb.h>
#include <ydb/core/testlib/test_client.h>
#include <ydb/core/testlib/basics/appdata.h>
#include <ydb/core/tx/schemeshard/schemeshard.h>

#include <library/cpp/actors/interconnect/interconnect_impl.h>
#include <ydb/core/tx/scheme_cache/scheme_cache.h>

#include <util/generic/vector.h>
#include <memory>

namespace NKikimr::NKqp {

using namespace Tests;
using namespace NSchemeShard;

namespace  {

struct TSimpleResource {
    ui32 Cnt;
    ui32 NodeId;
    TString DataCenterId;

    TSimpleResource(ui32 cnt, ui32 nodeId, TString dataCenterId)
        : Cnt(cnt)
        , NodeId(nodeId)
        , DataCenterId(std::move(dataCenterId))
    {}
};


TVector<NKikimrKqp::TKqpProxyNodeResources> Transform(TVector<TSimpleResource> data) {
    TVector<NKikimrKqp::TKqpProxyNodeResources> result;
    result.resize(data.size());
    for(auto& item: data) {
        NKikimrKqp::TKqpProxyNodeResources payload;
        payload.SetNodeId(item.NodeId);
        payload.SetDataCenterId(item.DataCenterId);
        payload.SetActiveWorkersCount(item.Cnt);
        result.emplace_back(payload);
    }

    return result;
}

TString CreateSession(TTestActorRuntime* runtime, const TActorId& kqpProxy, const TActorId& sender) {
    runtime->Send(new IEventHandleFat(kqpProxy, sender, new TEvKqp::TEvCreateSessionRequest()));
    auto reply = runtime->GrabEdgeEventRethrow<TEvKqp::TEvCreateSessionResponse>(sender);
    auto record = reply->Get()->Record;
    UNIT_ASSERT_VALUES_EQUAL(record.GetYdbStatus(), Ydb::StatusIds::SUCCESS);
    TString sessionId = record.GetResponse().GetSessionId();
    return sessionId;
}

}

Y_UNIT_TEST_SUITE(KqpProxy) {
    Y_UNIT_TEST(CalcPeerStats) {
        auto getActiveWorkers = [](const NKikimrKqp::TKqpProxyNodeResources& entry) {
            return entry.GetActiveWorkersCount();
        };

        UNIT_ASSERT_VALUES_EQUAL(
            CalcPeerStats(Transform(TVector<TSimpleResource>{TSimpleResource(100, 1, "1"), TSimpleResource(50, 2, "1")}), "1", true, getActiveWorkers).CV,
            47);

        UNIT_ASSERT_VALUES_EQUAL(
            CalcPeerStats(Transform(TVector<TSimpleResource>{TSimpleResource(100, 1, "1"), TSimpleResource(50, 2, "2")}), "1", true, getActiveWorkers).CV,
            0);
    }


    Y_UNIT_TEST(InvalidSessionID) {
        TPortManager tp;

        ui16 mbusport = tp.GetPort(2134);
        auto settings = Tests::TServerSettings(mbusport);

        Tests::TServer server(settings);
        Tests::TClient client(settings);

        server.GetRuntime()->SetLogPriority(NKikimrServices::KQP_PROXY, NActors::NLog::PRI_DEBUG);
        client.InitRootScheme();
        auto runtime = server.GetRuntime();

        TActorId kqpProxy = MakeKqpProxyID(runtime->GetNodeId(0));
        TActorId sender = runtime->AllocateEdgeActor();

        auto SendBadRequestToSession = [&](const TString& sessionId) {
            auto ev = MakeHolder<NKqp::TEvKqp::TEvQueryRequest>();
            ev->Record.MutableRequest()->SetSessionId(sessionId);
            ev->Record.MutableRequest()->SetAction(NKikimrKqp::QUERY_ACTION_EXECUTE);
            ev->Record.MutableRequest()->SetType(NKikimrKqp::QUERY_TYPE_SQL_SCRIPT);
            ev->Record.MutableRequest()->SetQuery("SELECT 1; COMMIT;");
            ev->Record.MutableRequest()->SetKeepSession(true);
            ev->Record.MutableRequest()->SetTimeoutMs(10);

            runtime->Send(new IEventHandleFat(kqpProxy, sender, ev.Release()));
            TAutoPtr<IEventHandle> handle;
            auto reply = runtime->GrabEdgeEventRethrow<TEvKqp::TEvProcessResponse>(sender);
            UNIT_ASSERT_VALUES_EQUAL(reply->Get()->Record.GetYdbStatus(), Ydb::StatusIds::BAD_REQUEST);
        };

        SendBadRequestToSession("ydb://session/1?id=ZjY5NWRlM2EtYWMyYjA5YWEtNzQ0MTVlYTMtM2Q4ZDgzOWQ=&node_id=1234&node_id=12345");
        SendBadRequestToSession("unknown://session/1?id=ZjY5NWRlM2EtYWMyYjA5YWEtNzQ0MTVlYTMtM2Q4ZDgzOWQ=&node_id=1234&node_id=12345");
        SendBadRequestToSession("ydb://session/1?id=ZjY5NWRlM2EtYWMyYjA5YWEtNzQ0MTVlYTMtM2Q4ZDgzOWQ=&node_id=eqweq");
    }

    Y_UNIT_TEST(PassErrroViaSessionActor) {
        TPortManager tp;

        ui16 mbusport = tp.GetPort(2134);
        auto settings = Tests::TServerSettings(mbusport);

        Tests::TServer server(settings);
        Tests::TClient client(settings);

        server.GetRuntime()->SetLogPriority(NKikimrServices::KQP_PROXY, NActors::NLog::PRI_DEBUG);
        client.InitRootScheme();
        auto runtime = server.GetRuntime();

        TActorId kqpProxy = MakeKqpProxyID(runtime->GetNodeId(0));
        TActorId sender = runtime->AllocateEdgeActor();

        auto ev = MakeHolder<NKqp::TEvKqp::TEvQueryRequest>();
        //ev->Record.MutableRequest()->SetSessionId(sessionId);
        ev->Record.SetYdbStatus(Ydb::StatusIds::BAD_REQUEST);
        auto issue = MakeIssue(NKikimrIssues::TIssuesIds::DEFAULT_ERROR, "SomeUniqTextForUt");

        NYql::TIssues issues;
        issues.AddIssue(issue);
        NYql::IssuesToMessage(issues, ev->Record.MutableQueryIssues());

        ev->Record.MutableRequest()->SetAction(NKikimrKqp::QUERY_ACTION_EXECUTE);
        ev->Record.MutableRequest()->SetType(NKikimrKqp::QUERY_TYPE_SQL_SCRIPT);
        ev->Record.MutableRequest()->SetQuery("SELECT 1; COMMIT;");
        ev->Record.MutableRequest()->SetKeepSession(true);
        ev->Record.MutableRequest()->SetTimeoutMs(10);

        runtime->Send(new IEventHandleFat(kqpProxy, sender, ev.Release()));
        TAutoPtr<IEventHandle> handle;
        auto reply = runtime->GrabEdgeEventRethrow<TEvKqp::TEvProcessResponse>(sender);
        UNIT_ASSERT_VALUES_EQUAL(reply->Get()->Record.GetYdbStatus(), Ydb::StatusIds::BAD_REQUEST);
        UNIT_ASSERT_VALUES_EQUAL(reply->Get()->Record.GetError(), "<main>: Error: SomeUniqTextForUt\n");
    }

    Y_UNIT_TEST(LoadedMetadataAfterCompilationTimeout) {

        TPortManager tp;

        ui16 mbusport = tp.GetPort(2134);
        auto settings = Tests::TServerSettings(mbusport).SetDomainName("Root").SetUseRealThreads(false);
        // set small compilation timeout to avoid long timer creation
        settings.AppConfig.MutableTableServiceConfig()->SetCompileTimeoutMs(400);

        Tests::TServer::TPtr server = new Tests::TServer(settings);

        server->GetRuntime()->SetLogPriority(NKikimrServices::KQP_PROXY, NActors::NLog::PRI_DEBUG);
        server->GetRuntime()->SetLogPriority(NKikimrServices::KQP_WORKER, NActors::NLog::PRI_DEBUG);
        server->GetRuntime()->SetLogPriority(NKikimrServices::TX_PROXY_SCHEME_CACHE,  NActors::NLog::PRI_DEBUG);
        server->GetRuntime()->SetLogPriority(NKikimrServices::KQP_COMPILE_ACTOR, NActors::NLog::PRI_DEBUG);

        auto runtime = server->GetRuntime();

        TActorId kqpProxy = MakeKqpProxyID(runtime->GetNodeId(0));
        TActorId sender = runtime->AllocateEdgeActor();
        InitRoot(server, sender);

        Cerr << "Allocated edge actor" << Endl;
        std::vector<TAutoPtr<IEventHandle>> captured;
        std::vector<TAutoPtr<IEventHandle>> scheduled;

        auto scheduledEvs = [&](TTestActorRuntimeBase& run, TAutoPtr<IEventHandle> &event, TDuration delay, TInstant &deadline) {
            if (event->GetTypeRewrite() == TEvents::TSystem::Wakeup) {
                TActorId actorId = event->GetRecipientRewrite();
                IActor *actor = runtime->FindActor(actorId);
                if (actor && actor->GetActivityType() == NKikimrServices::TActivity::KQP_COMPILE_ACTOR) {
                    Cerr << "Captured scheduled event for compile actor " << event->Recipient << Endl;
                    scheduled.push_back(event.Release());
                    return true;
                }
            }

            return TTestActorRuntime::DefaultScheduledFilterFunc(run, event, delay, deadline);
        };

        auto captureEvents =  [&](TTestActorRuntimeBase&, TAutoPtr<IEventHandle> &ev) {
            if (ev->GetTypeRewrite() == TEvTxProxySchemeCache::TEvNavigateKeySetResult::EventType) {
                Cerr << "Captured Event" << Endl;
                captured.push_back(ev.Release());
                return true;
            }
            return false;
        };

        auto CreateTable = [&](const TString& sessionId, const TString& queryText) {
            auto ev = std::make_unique<NKqp::TEvKqp::TEvQueryRequest>();
            ev->Record.MutableRequest()->SetSessionId(sessionId);
            ev->Record.MutableRequest()->SetAction(NKikimrKqp::QUERY_ACTION_EXECUTE);
            ev->Record.MutableRequest()->SetType(NKikimrKqp::QUERY_TYPE_SQL_DDL);
            ev->Record.MutableRequest()->SetQuery(queryText);
            runtime->Send(new IEventHandleFat(kqpProxy, sender, ev.release()));
            TAutoPtr<IEventHandle> handle;
            auto reply = runtime->GrabEdgeEventRethrow<TEvKqp::TEvQueryResponse>(sender);
            UNIT_ASSERT_VALUES_EQUAL(reply->Get()->Record.GetRef().GetYdbStatus(), Ydb::StatusIds::SUCCESS);
        };

        auto SendQuery = [&](const TString& sessionId, const TString& queryText) {
            auto ev = std::make_unique<NKqp::TEvKqp::TEvQueryRequest>();
            ev->Record.MutableRequest()->SetSessionId(sessionId);
            ev->Record.MutableRequest()->SetAction(NKikimrKqp::QUERY_ACTION_PREPARE);
            ev->Record.MutableRequest()->SetType(NKikimrKqp::QUERY_TYPE_SQL_DML);
            ev->Record.MutableRequest()->SetQuery(queryText);
            ev->Record.MutableRequest()->SetKeepSession(true);
            ev->Record.MutableRequest()->SetTimeoutMs(5000);

            runtime->Send(new IEventHandleFat(kqpProxy, sender, ev.release()));
            TAutoPtr<IEventHandle> handle;
            auto reply = runtime->GrabEdgeEventRethrow<TEvKqp::TEvQueryResponse>(sender);
            UNIT_ASSERT_VALUES_EQUAL(reply->Get()->Record.GetRef().GetYdbStatus(), Ydb::StatusIds::TIMEOUT);
        };

        TString sessionId = CreateSession(runtime, kqpProxy, sender);
        CreateTable(sessionId, "--!syntax_v1\nCREATE TABLE `/Root/Table` (A int32, PRIMARY KEY(A));");
        CreateTable(sessionId, "--!syntax_v1\nCREATE TABLE `/Root/TableWithIndex` (A int32, B int32, PRIMARY KEY(A), INDEX TestIndex GLOBAL ON(B));");

        server->GetRuntime()->SetEventFilter(captureEvents);
        server->GetRuntime()->SetScheduledEventFilter(scheduledEvs);
        std::vector<TString> queries{"SELECT * FROM `/Root/Table`;", "SELECT * FROM `/Root/TableWithIndex`;", "SELECT * FROM `/Root/Table`;", "SELECT * FROM `/Root/Table`;"};
        for (auto query: queries) {
            for(size_t iter = 0; iter < 2; ++iter) {
                SendQuery(CreateSession(runtime, kqpProxy, sender), query);
                for(auto ev: scheduled) {
                    Cerr << "Send scheduled evet back" << Endl;
                    runtime->Send(ev.Release());
                }

                for(auto ev: captured) {
                    Cerr << "Send captured event back" << Endl;
                    runtime->Send(ev.Release());
                }

                scheduled.clear();
                captured.clear();
            }
        }
    }

    Y_UNIT_TEST(NoLocalSessionExecution) {
        TPortManager tp;

        ui16 mbusport = tp.GetPort(2134);
        auto settings = Tests::TServerSettings(mbusport);
        // Setup two to nodes with 2 KQP_RPOXY_ACTOR instances.
        settings.SetNodeCount(2);

        Tests::TServer server(settings);
        Tests::TClient client(settings);

        server.GetRuntime()->SetLogPriority(NKikimrServices::KQP_PROXY, NActors::NLog::PRI_DEBUG);
        auto runtime = server.GetRuntime();

        TActorId kqpProxy1 = MakeKqpProxyID(runtime->GetNodeId(0));
        TActorId kqpProxy2 = MakeKqpProxyID(runtime->GetNodeId(1));
        TActorId sender = runtime->AllocateEdgeActor();

        {
            TString sessionId = CreateSession(runtime, kqpProxy2, sender);
            auto ev = MakeHolder<NKqp::TEvKqp::TEvQueryRequest>();
            ev->Record.MutableRequest()->SetSessionId(sessionId);
            ev->Record.MutableRequest()->SetAction(NKikimrKqp::QUERY_ACTION_EXECUTE);
            ev->Record.MutableRequest()->SetType(NKikimrKqp::QUERY_TYPE_SQL_SCRIPT);
            ev->Record.MutableRequest()->SetQuery("SELECT 1; COMMIT;");
            ev->Record.MutableRequest()->SetKeepSession(true);

            runtime->Send(new IEventHandleFat(kqpProxy1, sender, ev.Release()));

            TAutoPtr<IEventHandle> handle;
            auto reply = runtime->GrabEdgeEventsRethrow<TEvKqp::TEvQueryResponse, TEvKqp::TEvProcessResponse>(handle);

            TEvKqp::TEvQueryResponse* queryResponse = std::get<TEvKqp::TEvQueryResponse*>(reply);
            Y_VERIFY(queryResponse);
            UNIT_ASSERT_VALUES_EQUAL(queryResponse->Record.GetRef().GetYdbStatus(), Ydb::StatusIds::SUCCESS);
        }
    }

    Y_UNIT_TEST(NodeDisconnectedTest) {
        TPortManager tp;

        ui16 mbusport = tp.GetPort(2134);
        auto settings = Tests::TServerSettings(mbusport);
        // Setup two to nodes with 2 KQP_RPOXY_ACTOR instances.
        settings.SetNodeCount(2);
        // Don't use real threads so we can capture all events
        settings.SetUseRealThreads(false);

        Tests::TServer server(settings);
        Tests::TClient client(settings);

        server.GetRuntime()->SetLogPriority(NKikimrServices::KQP_PROXY, NActors::NLog::PRI_DEBUG);
        auto runtime = server.GetRuntime();

        TActorId kqpProxy1 = MakeKqpProxyID(runtime->GetNodeId(0));
        TActorId kqpProxy2 = MakeKqpProxyID(runtime->GetNodeId(1));
        Cerr << "KQP PROXY1 " << kqpProxy1 << Endl;
        Cerr << "KQP PROXY2 " << kqpProxy2 << Endl;
        TActorId sender = runtime->AllocateEdgeActor();

        Cerr << "SENDER " << sender << Endl;

        size_t NegativeStories = 0;
        size_t SuccessStories = 0;

        size_t capturedQueries = 0;
        size_t capturedPings = 0;
        auto captureEvents = [&](TTestActorRuntimeBase&, TAutoPtr<IEventHandle> &ev) {
            // Drop every second event for KQP_PROXY_ACTOR on second node.
            if (ev->Recipient == kqpProxy2 && ev->GetTypeRewrite() == NKqp::TEvKqp::TEvQueryRequest::EventType) {
                ++capturedQueries;
                if (capturedQueries % 2 == 0) {
                    return true;
                } else {
                    return false;
                }
            }

            if (ev->Recipient == kqpProxy2 && ev->GetTypeRewrite() == NKqp::TEvKqp::TEvPingSessionRequest::EventType) {
                ++capturedPings;
                if (capturedPings % 2 == 0) {
                    return true;
                } else {
                    return false;
                }
            }
            return false;
        };

        server.GetRuntime()->SetEventFilter(captureEvents);

        for (ui32 rep = 0; rep < 30; rep++) {

            {
                TString sessionId = CreateSession(runtime, kqpProxy2, sender);
                Cerr << "Created  session " << sessionId << Endl;
                auto ev = MakeHolder<NKqp::TEvKqp::TEvQueryRequest>();
                ev->Record.MutableRequest()->SetSessionId(sessionId);
                ev->Record.MutableRequest()->SetAction(NKikimrKqp::QUERY_ACTION_EXECUTE);
                ev->Record.MutableRequest()->SetType(NKikimrKqp::QUERY_TYPE_SQL_SCRIPT);
                ev->Record.MutableRequest()->SetQuery("SELECT 1; COMMIT;");
                ev->Record.MutableRequest()->SetKeepSession(true);
                ev->Record.MutableRequest()->SetTimeoutMs(1);

                runtime->Send(new IEventHandleFat(kqpProxy1, sender, ev.Release()));

                TAutoPtr<IEventHandle> handle;
                auto reply = runtime->GrabEdgeEventsRethrow<TEvKqp::TEvQueryResponse, TEvKqp::TEvProcessResponse>(handle);

                TEvKqp::TEvQueryResponse* queryResponse = std::get<TEvKqp::TEvQueryResponse*>(reply);
                if (queryResponse) {
                    ++SuccessStories;
                    UNIT_ASSERT_VALUES_EQUAL(queryResponse->Record.GetRef().GetYdbStatus(), Ydb::StatusIds::SUCCESS);
                }

                TEvKqp::TEvProcessResponse* processResponse = std::get<TEvKqp::TEvProcessResponse*>(reply);
                if (processResponse) {
                    ++NegativeStories;
                    UNIT_ASSERT_VALUES_EQUAL(processResponse->Record.GetYdbStatus(), Ydb::StatusIds::TIMEOUT);
                }
            }

            {
                TString sessionId = CreateSession(runtime, kqpProxy2, sender);
                auto ev = MakeHolder<NKqp::TEvKqp::TEvPingSessionRequest>();
                ev->Record.MutableRequest()->SetSessionId(sessionId);
                ev->Record.MutableRequest()->SetTimeoutMs(1);
                runtime->Send(new IEventHandleFat(kqpProxy1, sender, ev.Release()));

                TAutoPtr<IEventHandle> handle;
                auto reply = runtime->GrabEdgeEventsRethrow<TEvKqp::TEvPingSessionResponse, TEvKqp::TEvProcessResponse>(handle);

                TEvKqp::TEvPingSessionResponse* pingResponse = std::get<TEvKqp::TEvPingSessionResponse*>(reply);
                if (pingResponse) {
                    ++SuccessStories;
                    UNIT_ASSERT_VALUES_EQUAL(pingResponse->Record.GetStatus(), Ydb::StatusIds::SUCCESS);
                }

                TEvKqp::TEvProcessResponse* processResponse = std::get<TEvKqp::TEvProcessResponse*>(reply);
                if (processResponse) {
                    ++NegativeStories;
                    UNIT_ASSERT_VALUES_EQUAL(processResponse->Record.GetYdbStatus(), Ydb::StatusIds::TIMEOUT);
                }
            }
        }

        UNIT_ASSERT_C(SuccessStories > 0, "Proxy has success responses");
        UNIT_ASSERT_C(NegativeStories > 0, "Proxy has no negative responses");
    }

} // namspace NKqp
} // namespace NKikimr
