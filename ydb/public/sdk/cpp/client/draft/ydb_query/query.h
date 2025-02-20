#pragma once

#include <ydb/public/api/grpc/draft/ydb_query_v1.grpc.pb.h>

#include <ydb/public/sdk/cpp/client/ydb_result/result.h>
#include <ydb/public/sdk/cpp/client/ydb_types/fluent_settings_helpers.h>
#include <ydb/public/sdk/cpp/client/ydb_types/operation/operation.h>
#include <ydb/public/sdk/cpp/client/ydb_types/request_settings.h>
#include <ydb/public/sdk/cpp/client/ydb_types/status/status.h>

#include <library/cpp/threading/future/future.h>

namespace NYdb::NQuery {

class TExecuteQueryPart : public TStreamPartStatus {
public:
    bool HasResultSet() const { return ResultSet_.Defined(); }
    ui64 GetResultSetIndex() const { return ResultSetIndex_; }
    const TResultSet& GetResultSet() const { return *ResultSet_; }
    TResultSet ExtractResultSet() { return std::move(*ResultSet_); }

    TExecuteQueryPart(TStatus&& status)
        : TStreamPartStatus(std::move(status))
    {}

    TExecuteQueryPart(TStatus&& status, TResultSet&& resultSet, i64 resultSetIndex)
        : TStreamPartStatus(std::move(status))
        , ResultSet_(std::move(resultSet))
        , ResultSetIndex_(resultSetIndex)
    {}

private:
    TMaybe<TResultSet> ResultSet_;
    i64 ResultSetIndex_ = 0;
};

using TAsyncExecuteQueryPart = NThreading::TFuture<TExecuteQueryPart>;

class TExecuteQueryIterator : public TStatus {
    friend class TExecQueryImpl;
public:
    class TReaderImpl;

    TAsyncExecuteQueryPart ReadNext();

private:
    TExecuteQueryIterator(
        std::shared_ptr<TReaderImpl> impl,
        TPlainStatus&& status)
    : TStatus(std::move(status))
    , ReaderImpl_(impl) {}

    std::shared_ptr<TReaderImpl> ReaderImpl_;
};

using TAsyncExecuteQueryIterator = NThreading::TFuture<TExecuteQueryIterator>;

struct TExecuteQuerySettings : public TRequestSettings<TExecuteQuerySettings> {
};

class TExecuteQueryResult : public TStatus {
public:
    const TVector<TResultSet>& GetResultSets() const;
    TResultSet GetResultSet(size_t resultIndex) const;

    TResultSetParser GetResultSetParser(size_t resultIndex) const;

    TExecuteQueryResult(TStatus&& status)
        : TStatus(std::move(status))
    {}

    TExecuteQueryResult(TStatus&& status, TVector<TResultSet>&& resultSets)
        : TStatus(std::move(status))
        , ResultSets_(std::move(resultSets))
    {}

private:
    TVector<TResultSet> ResultSets_;
};

using TAsyncExecuteQueryResult = NThreading::TFuture<TExecuteQueryResult>;


struct TExecuteScriptSettings : public TOperationRequestSettings<TExecuteScriptSettings> {
};

class TExecuteScriptResult : public TOperation {
public:
    struct TMetadata {
        TString ExecutionId;
    };

    using TOperation::TOperation;
    TExecuteScriptResult(TStatus&& status, Ydb::Operations::Operation&& operation);

    const TMetadata& Metadata() const {
        return Metadata_;
    }

private:
    TMetadata Metadata_;
};

using TAsyncExecuteScriptResult = NThreading::TFuture<TExecuteScriptResult>;

struct TFetchScriptResultsSettings : public TRequestSettings<TFetchScriptResultsSettings> {
    FLUENT_SETTING(TString, FetchToken);
    FLUENT_SETTING_DEFAULT(ui64, RowsOffset, 0);
    FLUENT_SETTING_DEFAULT(ui64, RowsLimit, 1000);
};

class TFetchScriptResultsResult : public TStatus {
public:
    bool HasResultSet() const { return ResultSet_.Defined(); }
    ui64 GetResultSetIndex() const { return ResultSetIndex_; }
    const TResultSet& GetResultSet() const { return *ResultSet_; }
    TResultSet ExtractResultSet() { return std::move(*ResultSet_); }
    const TString& NextPageToken() const { return NextPageToken_; }

    explicit TFetchScriptResultsResult(TStatus&& status)
        : TStatus(std::move(status))
    {}

    TFetchScriptResultsResult(TStatus&& status, TResultSet&& resultSet, i64 resultSetIndex, const TString& nextPageToken)
        : TStatus(std::move(status))
        , ResultSet_(std::move(resultSet))
        , ResultSetIndex_(resultSetIndex)
        , NextPageToken_(nextPageToken)
    {}

private:
    TMaybe<TResultSet> ResultSet_;
    i64 ResultSetIndex_ = 0;
    TString NextPageToken_;
};

using TAsyncFetchScriptResultsResult = NThreading::TFuture<TFetchScriptResultsResult>;

} // namespace NYdb::NQuery
