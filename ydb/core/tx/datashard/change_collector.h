#pragma once

#include <ydb/core/engine/minikql/change_collector_iface.h>
#include <ydb/core/tablet_flat/tablet_flat_executor.h>

namespace NKikimr {
namespace NDataShard {

class TDataShard;
struct TUserTable;

class IDataShardUserDb;

class IDataShardChangeGroupProvider {
protected:
    ~IDataShardChangeGroupProvider() = default;

public:
    virtual bool HasChangeGroup() const = 0;
    virtual ui64 GetChangeGroup() = 0;
};

class TDataShardChangeGroupProvider final
    : public IDataShardChangeGroupProvider
{
public:
    // Note: for distributed transactions group is expected to be 0
    TDataShardChangeGroupProvider(TDataShard& self, NTable::TDatabase& db, std::optional<ui64> group = std::nullopt)
        : Self(self)
        , Db(db)
        , Group(group)
    { }

    bool HasChangeGroup() const override {
        return bool(Group);
    }

    ui64 GetChangeGroup() override;

private:
    TDataShard& Self;
    NTable::TDatabase& Db;
    std::optional<ui64> Group;
};

class IDataShardChangeCollector : public NMiniKQL::IChangeCollector {
public:
    // basic change record's info
    struct TChange {
        ui64 Order;
        ui64 Group;
        ui64 Step;
        ui64 TxId;
        TPathId PathId;
        ui64 BodySize;
        TPathId TableId;
        ui64 SchemaVersion;
        ui64 LockId = 0;
        ui64 LockOffset = 0;
    };

public:
    virtual void OnRestart() = 0;
    virtual bool NeedToReadKeys() const = 0;

    virtual void CommitLockChanges(ui64 lockId, const TRowVersion& writeVersion) = 0;

    virtual const TVector<TChange>& GetCollected() const = 0;
    virtual TVector<TChange>&& GetCollected() = 0;
};

IDataShardChangeCollector* CreateChangeCollector(
        TDataShard& dataShard,
        IDataShardUserDb& userDb,
        IDataShardChangeGroupProvider& groupProvider,
        NTable::TDatabase& db,
        const TUserTable& table);
IDataShardChangeCollector* CreateChangeCollector(
        TDataShard& dataShard,
        IDataShardUserDb& userDb,
        IDataShardChangeGroupProvider& groupProvider,
        NTable::TDatabase& db,
        ui64 tableId);

} // NDataShard
} // NKikimr
