LIBRARY()

SRCS(
    audit_log.cpp
    grpc_endpoint_publish_actor.cpp
    grpc_helper.cpp
    grpc_mon.cpp
    grpc_publisher_service_actor.cpp
    grpc_request_proxy.cpp
    grpc_request_proxy_simple.cpp
    local_rate_limiter.cpp
    operation_helpers.cpp
    resolve_local_db_table.cpp
    rpc_alter_coordination_node.cpp
    rpc_alter_table.cpp
    rpc_begin_transaction.cpp
    rpc_calls.cpp
    rpc_cancel_operation.cpp
    rpc_cms.cpp
    rpc_commit_transaction.cpp
    rpc_copy_table.cpp
    rpc_copy_tables.cpp
    rpc_export.cpp
    rpc_create_coordination_node.cpp
    rpc_create_session.cpp
    rpc_create_table.cpp
    rpc_delete_session.cpp
    rpc_describe_coordination_node.cpp
    rpc_describe_path.cpp
    rpc_describe_table.cpp
    rpc_describe_table_options.cpp
    rpc_drop_coordination_node.cpp
    rpc_drop_table.cpp
    rpc_discovery.cpp
    rpc_execute_data_query.cpp
    rpc_execute_scheme_query.cpp
    rpc_execute_yql_script.cpp
    rpc_explain_yql_script.cpp
    rpc_explain_data_query.cpp
    rpc_forget_operation.cpp
    rpc_fq_internal.cpp
    rpc_fq.cpp
    rpc_get_operation.cpp
    rpc_get_shard_locations.cpp
    rpc_import.cpp
    rpc_import_data.cpp
    rpc_keep_alive.cpp
    rpc_kh_describe.cpp
    rpc_kh_snapshots.cpp
    rpc_kqp_base.cpp
    rpc_list_operations.cpp
    rpc_login.cpp
    rpc_load_rows.cpp
    rpc_log_store.cpp
    rpc_long_tx.cpp
    rpc_make_directory.cpp
    rpc_modify_permissions.cpp
    rpc_monitoring.cpp
    rpc_prepare_data_query.cpp
    rpc_rate_limiter_api.cpp
    rpc_read_columns.cpp
    rpc_read_table.cpp
    rpc_remove_directory.cpp
    rpc_rename_tables.cpp
    rpc_rollback_transaction.cpp
    rpc_scheme_base.cpp
    rpc_stream_execute_scan_query.cpp
    rpc_stream_execute_yql_script.cpp
    rpc_whoami.cpp
    table_settings.cpp

    query/rpc_execute_query.cpp
    query/rpc_execute_script.cpp
    query/rpc_fetch_script_results.cpp
    query/service_query.h
)

PEERDIR(
    contrib/libs/xxhash
    library/cpp/cgiparam
    library/cpp/digest/old_crc
    ydb/core/actorlib_impl
    ydb/core/base
    ydb/core/control
    ydb/core/discovery
    ydb/core/engine
    ydb/core/formats
    ydb/core/fq/libs/actors
    ydb/core/fq/libs/control_plane_proxy
    ydb/core/fq/libs/control_plane_proxy/events
    ydb/core/grpc_services/counters
    ydb/core/grpc_services/local_rpc
    ydb/core/grpc_services/cancelation
    ydb/core/health_check
    ydb/core/io_formats
    ydb/core/kesus/tablet
    ydb/core/protos
    ydb/core/scheme
    ydb/core/sys_view
    ydb/core/tx
    ydb/core/tx/datashard
    ydb/core/tx/sharding
    ydb/core/tx/long_tx_service/public
    ydb/core/ydb_convert
    ydb/library/aclib
    ydb/library/binary_json
    ydb/library/dynumber
    ydb/library/mkql_proto
    ydb/library/persqueue/topic_parser
    ydb/library/yql/public/types
    ydb/public/api/grpc/draft
    ydb/public/api/protos
    ydb/public/lib/operation_id
    ydb/public/sdk/cpp/client/resources
    ydb/services/ext_index/common
)

YQL_LAST_ABI_VERSION()

END()

RECURSE(
    base
    counters
    local_rpc
)

RECURSE_FOR_TESTS(
    ut
)
