#pragma once

#include <ydb/library/yql/dq/integration/yql_dq_integration.h>

namespace NYql {

class TDqIntegrationBase: public IDqIntegration {
public:
    ui64 Partition(const TDqSettings& config, size_t maxPartitions, const TExprNode& node,
        TVector<TString>& partitions, TString* clusterName, TExprContext& ctx, bool canFallback) override;
    TMaybe<ui64> CanRead(const TDqSettings& config, const TExprNode& read, TExprContext& ctx, bool skipIssues) override;
    TExprNode::TPtr WrapRead(const TDqSettings& config, const TExprNode::TPtr& read, TExprContext& ctx) override;
    void RegisterMkqlCompiler(NCommon::TMkqlCallableCompilerBase& compiler) override;
    TMaybe<bool> CanWrite(const TDqSettings& config, const TExprNode& write, TExprContext& ctx) override;
    bool CanFallback() override;
    void FillSourceSettings(const TExprNode& node, ::google::protobuf::Any& settings, TString& sourceType) override;
    void FillSinkSettings(const TExprNode& node, ::google::protobuf::Any& settings, TString& sinkType) override;
    void FillTransformSettings(const TExprNode& node, ::google::protobuf::Any& settings) override;
    void Annotate(const TExprNode& node, THashMap<TString, TString>& params) override;
    bool PrepareFullResultTableParams(const TExprNode& root, TExprContext& ctx, THashMap<TString, TString>& params, THashMap<TString, TString>& secureParams) override;
    void WriteFullResultTableRef(NYson::TYsonWriter& writer, const TVector<TString>& columns, const THashMap<TString, TString>& graphParams) override;
};

} // namespace NYql
