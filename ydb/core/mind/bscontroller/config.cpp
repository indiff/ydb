#include "impl.h"
#include "config.h"
#include "diff.h"
#include "table_merger.h"

namespace NKikimr::NBsController {

        class TBlobStorageController::TNodeWardenUpdateNotifier {
            TBlobStorageController *Self;
            TConfigState &State;
            THashMap<TNodeId, NKikimrBlobStorage::TEvControllerNodeServiceSetUpdate> Services;
            THashSet<TPDiskId> DeletedPDiskIds;

        public:
            TNodeWardenUpdateNotifier(TBlobStorageController *self, TConfigState &state)
                : Self(self)
                , State(state)
            {}

            void Execute(std::deque<std::unique_ptr<IEventHandle>>& outbox) {
                ApplyUpdates();

                for (auto &pair : Services) {
                    const TNodeId &nodeId = pair.first;

                    if (TNodeInfo *node = Self->FindNode(nodeId); node && node->ConnectedCount) {
                        auto event = MakeHolder<TEvBlobStorage::TEvControllerNodeServiceSetUpdate>();
                        auto& record = event->Record;
                        pair.second.Swap(&record);
                        record.SetStatus(NKikimrProto::OK);
                        record.SetNodeID(nodeId);
                        record.SetInstanceId(Self->InstanceId);
                        record.SetAvailDomain(AppData()->DomainsInfo->GetDomainUidByTabletId(Self->TabletID()));
                        outbox.push_back(std::make_unique<IEventHandleFat>(MakeBlobStorageNodeWardenID(nodeId),
                            Self->SelfId(), event.Release()));
                    }
                }
            }

        private:
            void ApplyUpdates() {
                for (auto&& [base, overlay] : State.PDisks.Diff()) {
                    if (!overlay->second) {
                        ApplyPDiskDeleted(overlay->first, *base->second);
                    } else if (!base) {
                        ApplyPDiskCreated(overlay->first, *overlay->second);
                    }
                }
                for (auto&& [base, overlay] : State.VSlots.Diff()) {
                    if (!overlay->second) {
                        ApplyVSlotDeleted(overlay->first, *base->second);
                    } else if (!base) {
                        ApplyVSlotCreated(overlay->first, *overlay->second);
                    } else {
                        ApplyVSlotDiff(overlay->first, *base->second, *overlay->second);
                    }
                }
                for (auto&& [base, overlay] : State.Groups.Diff()) {
                    if (!overlay->second) {
                        ApplyGroupDeleted(overlay->first, *base->second);
                    } else if (!base) {
                        ApplyGroupCreated(overlay->first, *overlay->second);
                    } else {
                        ApplyGroupDiff(overlay->first, *base->second, *overlay->second);
                    }
                }
            }

            void ApplyPDiskCreated(const TPDiskId &pdiskId, const TPDiskInfo &pdiskInfo) {
                if (!State.StaticPDisks.count(pdiskId)) {
                    // don't create static PDisks as they are already created
                    NKikimrBlobStorage::TNodeWardenServiceSet::TPDisk *pdisk = CreatePDiskEntry(pdiskId, pdiskInfo);
                    pdisk->SetEntityStatus(NKikimrBlobStorage::CREATE);
                }
            }

            void ApplyPDiskDeleted(const TPDiskId &pdiskId, const TPDiskInfo &pdiskInfo) {
                DeletedPDiskIds.insert(pdiskId);
                if (!State.StaticPDisks.count(pdiskId)) {
                    NKikimrBlobStorage::TNodeWardenServiceSet::TPDisk *pdisk = CreatePDiskEntry(pdiskId, pdiskInfo);
                    pdisk->SetEntityStatus(NKikimrBlobStorage::DESTROY);
                }
            }

            NKikimrBlobStorage::TNodeWardenServiceSet::TPDisk *CreatePDiskEntry(const TPDiskId &fullPDiskId,
                    const TPDiskInfo &pdiskInfo) {
                const ui32 nodeId = fullPDiskId.NodeId;
                const ui32 pdiskId = fullPDiskId.PDiskId;

                NKikimrBlobStorage::TNodeWardenServiceSet &service = *Services[nodeId].MutableServiceSet();
                NKikimrBlobStorage::TNodeWardenServiceSet::TPDisk *pdisk = service.AddPDisks();
                pdisk->SetNodeID(nodeId);
                pdisk->SetPDiskID(pdiskId);
                if (pdiskInfo.Path) {
                    pdisk->SetPath(pdiskInfo.Path);
                } else if (pdiskInfo.LastSeenPath) {
                    pdisk->SetPath(pdiskInfo.LastSeenPath);
                }
                pdisk->SetPDiskGuid(pdiskInfo.Guid);
                pdisk->SetPDiskCategory(pdiskInfo.Kind.GetRaw());
                pdisk->SetExpectedSerial(pdiskInfo.ExpectedSerial);
                pdisk->SetManagementStage(Self->SerialManagementStage);
                if (pdiskInfo.PDiskConfig && !pdisk->MutablePDiskConfig()->ParseFromString(pdiskInfo.PDiskConfig)) {
                    // TODO(alexvru): report this somehow
                }
                pdisk->SetSpaceColorBorder(Self->PDiskSpaceColorBorder);

                return pdisk;
            }

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // VSLOT OPERATIONS
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////

            void AddVSlotToProtobuf(const TVSlotId &vslotId, const TVSlotInfo &vslotInfo, TMood::EValue mood,
                    TMaybe<NKikimrBlobStorage::EEntityStatus> status = Nothing()) {
                NKikimrBlobStorage::TNodeWardenServiceSet &service = *Services[vslotId.NodeId].MutableServiceSet();

                NKikimrBlobStorage::TNodeWardenServiceSet::TVDisk &item = *service.AddVDisks();

                // fill in VDiskID for this new VSlot
                VDiskIDFromVDiskID(vslotInfo.GetVDiskId(), item.MutableVDiskID());

                // fill in VDiskLocation
                Serialize(item.MutableVDiskLocation(), vslotInfo);

                // set up kind
                item.SetVDiskKind(vslotInfo.Kind);

                // set destroy/donor flag if needed
                switch (mood) {
                    case TMood::Delete:
                        item.SetDoDestroy(true);
                        item.SetEntityStatus(NKikimrBlobStorage::DESTROY); // set explicitly
                        Y_VERIFY(!status);
                        break;

                    case TMood::Donor:
                        Y_VERIFY(!status);
                        break;

                    case TMood::Normal:
                        if (status) {
                            item.SetEntityStatus(*status);
                        }
                        break;

                    case TMood::Wipe:
                        item.SetDoWipe(true);
                        break;

                    default:
                        Y_FAIL();
                }

                if (const TGroupInfo *group = State.Groups.Find(vslotInfo.GroupId); group && mood != TMood::Delete) {
                    item.SetStoragePoolName(State.StoragePools.Get().at(group->StoragePoolId).Name);

                    const TVSlotFinder vslotFinder{[this](TVSlotId vslotId, auto&& callback) {
                        if (const TVSlotInfo *vslot = State.VSlots.Find(vslotId)) {
                            callback(*vslot);
                        }
                    }};

                    SerializeDonors(&item, vslotInfo, *group, vslotFinder);
                } else {
                    Y_VERIFY(mood != TMood::Donor);
                }
            }

            void ApplyVSlotCreated(const TVSlotId &vslotId, const TVSlotInfo &vslotInfo) {
                AddVSlotToProtobuf(vslotId, vslotInfo, TMood::Normal, NKikimrBlobStorage::CREATE);
            }

            void ApplyVSlotDeleted(const TVSlotId& vslotId, const TVSlotInfo& vslotInfo) {
                if (DeletedPDiskIds.count(vslotId.ComprisingPDiskId()) && vslotInfo.IsBeingDeleted()) {
                    // the slot has been deleted along with its PDisk; although it is useless to slay slots over PDisk
                    // that is being stopped, we issue this command to terminate VDisk actors correctly
                    AddVSlotToProtobuf(vslotId, vslotInfo, TMood::Delete);
                }
            }

            void ApplyVSlotDiff(const TVSlotId &vslotId, const TVSlotInfo &prev, const TVSlotInfo &cur) {
                if (!prev.IsBeingDeleted() && cur.IsBeingDeleted()) {
                    // the slot has started deletion during this update
                    AddVSlotToProtobuf(vslotId, prev, TMood::Delete);
                } else if (prev.Mood != cur.Mood) {
                    // the slot mood has changed
                    AddVSlotToProtobuf(vslotId, cur, static_cast<TMood::EValue>(cur.Mood));
                } else if (prev.GroupGeneration != cur.GroupGeneration) {
                    // the slot generation has changed
                    AddVSlotToProtobuf(vslotId, cur, TMood::Normal);
                }
            }

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // GROUP OPERATIONS
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////

            void ApplyGroupCreated(const TGroupId& /*groupId*/, const TGroupInfo &groupInfo) {
                if (!groupInfo.VDisksInGroup && groupInfo.VirtualGroupState != NKikimrBlobStorage::EVirtualGroupState::WORKING) {
                    return; // do not report virtual groups that are not properly created yet
                }

                // create ordered map of VDisk entries for group
                THashSet<TNodeId> nodes;
                for (const TVSlotInfo *vslot : groupInfo.VDisksInGroup) {
                    Y_VERIFY(vslot->GroupGeneration == groupInfo.Generation);
                    nodes.insert(vslot->VSlotId.NodeId);
                }

                // check tenant id, if necessary
                TMaybe<TKikimrScopeId> scopeId;
                const TStoragePoolInfo& info = State.StoragePools.Get().at(groupInfo.StoragePoolId);
                if (info.SchemeshardId && info.PathItemId) {
                    scopeId = TKikimrScopeId(*info.SchemeshardId, *info.PathItemId);
                } else {
                    Y_VERIFY(!info.SchemeshardId && !info.PathItemId);
                }
                const TString storagePoolName = info.Name;

                // push group information to each node that will receive VDisk status update
                for (TNodeId nodeId : nodes) {
                    NKikimrBlobStorage::TNodeWardenServiceSet *service = Services[nodeId].MutableServiceSet();
                    SerializeGroupInfo(service->AddGroups(), groupInfo, storagePoolName, scopeId);
                }
            }

            void ApplyGroupDeleted(const TGroupId &groupId, const TGroupInfo& /*groupInfo*/) {
                for (const auto &kv : State.Nodes.Get()) {
                    const TNodeId nodeId = kv.first;
                    NKikimrBlobStorage::TNodeWardenServiceSet &service = *Services[nodeId].MutableServiceSet();
                    NKikimrBlobStorage::TGroupInfo &item = *service.AddGroups();
                    item.SetGroupID(groupId);
                    item.SetEntityStatus(NKikimrBlobStorage::DESTROY);
                }
            }

            void ApplyGroupDiff(const TGroupId &groupId, const TGroupInfo &prev, const TGroupInfo &cur) {
                if (prev.Generation != cur.Generation) {
                    ApplyGroupCreated(groupId, cur);
                    for (const auto& [key, info] : *State.HostRecords) {
                        auto *meta = Services[info.NodeId].AddGroupMetadata();
                        meta->SetGroupId(groupId);
                        meta->SetCurrentGeneration(cur.Generation);
                    }
                }
                Y_VERIFY(prev.VDisksInGroup.size() == cur.VDisksInGroup.size() ||
                    (cur.VDisksInGroup.empty() && cur.DecommitStatus == NKikimrBlobStorage::TGroupDecommitStatus::DONE));
                for (size_t i = 0; i < cur.VDisksInGroup.size(); ++i) {
                    const TVSlotInfo& prevSlot = *prev.VDisksInGroup[i];
                    const TVSlotInfo& curSlot = *cur.VDisksInGroup[i];
                    if (prevSlot.VSlotId != curSlot.VSlotId) {
                        STLOG(PRI_INFO, BS_CONTROLLER_AUDIT, BSCA05, "VDisk moved",
                            (UniqueId, State.UniqueId),
                            (PrevSlot, prevSlot.VSlotId),
                            (CurSlot, curSlot.VSlotId),
                            (VDiskId, curSlot.GetVDiskId()));
                    }
                }
            }
        };

        bool TBlobStorageController::CommitConfigUpdates(TConfigState& state, bool suppressFailModelChecking,
                bool suppressDegradedGroupsChecking, bool suppressDisintegratedGroupsChecking,
                TTransactionContext& txc, TString *errorDescription) {
            NIceDb::TNiceDb db(txc.DB);

            for (auto&& [base, overlay] : state.Groups.Diff()) {
                if (base && overlay->second && std::exchange(overlay->second->ContentChanged, false)) {
                    const auto& groupInfo = overlay->second;
                    ++groupInfo->Generation;
                    for (const TVSlotInfo *slot : groupInfo->VDisksInGroup) {
                        if (slot->GroupGeneration != groupInfo->Generation) {
                            TVSlotInfo *mutableSlot = state.VSlots.FindForUpdate(slot->VSlotId);
                            Y_VERIFY(mutableSlot);
                            mutableSlot->GroupGeneration = groupInfo->Generation;
                        }
                    }
                }
            }

            if (!suppressDisintegratedGroupsChecking) {
                for (auto&& [base, overlay] : state.Groups.Diff()) {
                    if (base && overlay->second) {
                        const TGroupInfo::TGroupStatus& prev = base->second->Status;
                        const TGroupInfo::TGroupStatus& status = overlay->second->Status;
                        if (status.ExpectedStatus == NKikimrBlobStorage::TGroupStatus::DISINTEGRATED &&
                                status.ExpectedStatus != prev.ExpectedStatus) { // status did really change
                            *errorDescription = TStringBuilder() << "GroupId# " << overlay->first
                                << " ExpectedStatus# DISINTEGRATED";
                           return false;
                        }
                     }
                }
            }

            // check that group modification would not degrade failure model
            if (!suppressFailModelChecking) {
                for (auto&& [base, overlay] : state.Groups.Diff()) {
                    if (!overlay->second || !base) {
                        continue;
                    }
                    auto& group = overlay->second;
                    if ((base->second->Generation != group->Generation || group->MoodChanged) && group->VDisksInGroup) {
                        // process only groups with changed content; create topology for group
                        auto& topology = *group->Topology;
                        // fill in vector of failed disks (that are not fully operational)
                        TBlobStorageGroupInfo::TGroupVDisks failed(&topology);
                        for (const TVSlotInfo *slot : group->VDisksInGroup) {
                            if (!slot->IsReady) {
                                failed |= {&topology, slot->GetShortVDiskId()};
                            }
                        }
                        // check the failure model
                        auto& checker = *topology.QuorumChecker;
                        if (!checker.CheckFailModelForGroup(failed)) {
                            *errorDescription = TStringBuilder() << "GroupId# " << base->first
                                << " may lose data while modifying group";
                            return false;
                        } else if (!suppressDegradedGroupsChecking && checker.IsDegraded(failed)) {
                            *errorDescription = TStringBuilder() << "GroupId# " << base->first
                                << " may become DEGRADED while modifying group";
                            return false;
                        }
                    }
                }
            }

            // trim PDisks awaiting deletion
            for (const TPDiskId& pdiskId : state.PDisksToRemove) {
                TPDiskInfo *pdiskInfo = state.PDisks.FindForUpdate(pdiskId);
                Y_VERIFY(pdiskInfo);
                if (pdiskInfo->NumActiveSlots) {
                    *errorDescription = TStringBuilder() << "failed to remove PDisk# " << pdiskId << " as it has active VSlots";
                    return false;
                }
                for (const auto& [vslotId, vslot] : std::exchange(pdiskInfo->VSlotsOnPDisk, {})) {
                    Y_VERIFY(vslot->IsBeingDeleted());
                    state.VSlots.DeleteExistingEntry(TVSlotId(pdiskId, vslotId));
                }
                state.PDisks.DeleteExistingEntry(pdiskId);
            }

            MakeTableMerger<Schema::HostConfig>(&HostConfigs, &state.HostConfigs.Get(), this)(txc);
            MakeTableMerger<Schema::Box>(&Boxes, &state.Boxes.Get(), this)(txc);
            MakeTableMerger<Schema::BoxStoragePool>(&StoragePools, &state.StoragePools.Get(), this)(txc);
            MakeTableMerger<Schema::Node>(&Nodes, &state.Nodes.Get(), this)(txc);

            // apply overlay maps to their respective tables
            state.PDisks.ApplyToTable(this, txc);
            state.VSlots.ApplyToTable(this, txc);
            state.Groups.ApplyToTable(this, txc);
            state.DrivesSerials.ApplyToTable(this, txc);

            // apply group to storage pool mapping
            for (auto&& [base, overlay] : state.Groups.Diff()) {
                using Table = Schema::GroupStoragePool;
                if (!overlay->second) {
                    db.Table<Table>().Key(overlay->first).Delete();
                } else if (!base || base->second->StoragePoolId != overlay->second->StoragePoolId) {
                    db.Table<Table>().Key(overlay->first).Update<Table::BoxId, Table::StoragePoolId>(
                        std::get<0>(overlay->second->StoragePoolId),
                        std::get<1>(overlay->second->StoragePoolId));
                }
            }

            // trim the PDiskMetrics table
            for (auto&& [base, overlay] : state.PDisks.Diff()) {
                if (!overlay->second) {
                    db.Table<Schema::PDiskMetrics>().Key(overlay->first.GetKey()).Delete();
                }
            }

            // remove unused group latency and vdisk metrics records
            for (auto&& [base, overlay] : state.Groups.Diff()) {
                if (!overlay->second) {
                    const TGroupId groupId = overlay->first;
                    db.Table<Schema::GroupLatencies>().Key(groupId).Delete();
                }
            }

            // remove unused vdisk metrics for either deleted vslots or with changed generation
            for (auto&& [base, overlay] : state.VSlots.Diff()) {
                if (!overlay->second || (base && overlay->second->GroupGeneration != base->second->GroupGeneration)) {
                    const TVDiskID& vdiskId = base->second->GetVDiskId();
                    db.Table<Schema::VDiskMetrics>().Key(vdiskId.GroupID, vdiskId.GroupGeneration, vdiskId.FailRealm,
                        vdiskId.FailDomain, vdiskId.VDisk).Delete();
                }
            }

            // write down NextGroupId if it has changed
            if (state.NextGroupId.Changed()) {
                db.Table<Schema::State>().Key(true).Update<Schema::State::NextGroupID>(state.NextGroupId.Get());
            }
            if (state.NextStoragePoolId.Changed()) {
                db.Table<Schema::State>().Key(true).Update<Schema::State::NextStoragePoolId>(state.NextStoragePoolId.Get());
            }
            if (state.SerialManagementStage.Changed()) {
                db.Table<Schema::State>().Key(true).Update<Schema::State::SerialManagementStage>(state.SerialManagementStage.Get());
            }

            CommitSelfHealUpdates(state);
            CommitScrubUpdates(state, txc);
            CommitStoragePoolStatUpdates(state);
            CommitSysViewUpdates(state);
            CommitVirtualGroupUpdates(state);

            // add updated and remove deleted vslots from VSlotReadyTimestampQ
            const TMonotonic now = TActivationContext::Monotonic();
            for (auto&& [base, overlay] : state.VSlots.Diff()) {
                if (!overlay->second || !overlay->second->Group) { // deleted one
                    (overlay->second ? overlay->second : base->second)->DropFromVSlotReadyTimestampQ();
                    NotReadyVSlotIds.erase(overlay->first);
                } else if (overlay->second->Status != NKikimrBlobStorage::EVDiskStatus::READY) {
                    overlay->second->DropFromVSlotReadyTimestampQ();
                } else if (!base || base->second->Status != NKikimrBlobStorage::EVDiskStatus::READY) {
                    overlay->second->PutInVSlotReadyTimestampQ(now);
                } else {
                    Y_VERIFY_DEBUG(overlay->second->IsReady || overlay->second->IsInVSlotReadyTimestampQ());
                }
            }

            TNodeWardenUpdateNotifier(this, state).Execute(state.Outbox);

            state.CheckConsistency();
            state.Commit();
            ValidateInternalState();

            ScheduleVSlotReadyUpdate();

            return true;
        }

        void TBlobStorageController::CommitSelfHealUpdates(TConfigState& state) {
            auto ev = MakeHolder<TEvControllerNotifyGroupChange>();
            auto sh = MakeHolder<TEvControllerUpdateSelfHealInfo>();

            for (auto&& [base, overlay] : state.Groups.Diff()) {
                const TGroupId groupId = overlay->first;
                if (!overlay->second) { // item was deleted, drop it from the cache
                    const ui32 erased = GroupLookup.erase(groupId);
                    Y_VERIFY(erased);
                    sh->GroupsToUpdate[groupId].reset();
                    ev->Deleted.push_back(groupId);
                } else if (base) { // item was overwritten, just update pointer in the lookup cache
                    const auto it = GroupLookup.find(groupId);
                    Y_VERIFY(it != GroupLookup.end());
                    TGroupInfo *prev = std::exchange(it->second, overlay->second.Get());
                    Y_VERIFY(prev == base->second.Get());
                    if (base->second->Generation != overlay->second->Generation) {
                        sh->GroupsToUpdate[groupId].emplace();
                    }
                } else { // a new item was inserted
                    auto&& [it, inserted] = GroupLookup.emplace(groupId, overlay->second.Get());
                    Y_VERIFY(inserted);
                    sh->GroupsToUpdate[groupId].emplace();
                    ev->Created.push_back(groupId);
                }
            }
            for (auto&& [base, overlay] : state.PDisks.Diff()) {
                if (!overlay->second) {
                    continue; // ignore cases with PDisk was deleted -- groups over this disk must be deleted too
                } else if (base && base->second->GetSelfHealStatusTuple() == overlay->second->GetSelfHealStatusTuple()) {
                    continue; // nothing changed for this PDisk
                } else {
                    for (const auto& [id, slot] : overlay->second->VSlotsOnPDisk) {
                        if (slot->Group) {
                            sh->GroupsToUpdate[slot->GroupId].emplace();
                        }
                    }
                }
            }

            if (ev->Created || ev->Deleted) {
                state.Outbox.push_back(std::make_unique<IEventHandleFat>(StatProcessorActorId, SelfId(), ev.Release()));
            }
            if (sh->GroupsToUpdate) {
                FillInSelfHealGroups(*sh, &state);
                state.Outbox.push_back(std::make_unique<IEventHandleFat>(SelfHealId, SelfId(), sh.Release()));
            }
        }

        void TBlobStorageController::CommitScrubUpdates(TConfigState& state, TTransactionContext& txc) {
            // remove scrubbing entries
            for (auto&& [base, overlay] : state.PDisks.Diff()) {
                if (!overlay->second) { // PDisk deleted
                    ScrubState.OnDeletePDisk(overlay->first);
                }
            }
            for (auto&& [base, overlay] : state.VSlots.Diff()) {
                if (!overlay->second) {
                    ScrubState.OnDeleteVSlot(overlay->first, txc);
                } else if (!base) {
                    Y_VERIFY_DEBUG(!overlay->second->IsBeingDeleted());
                    ScrubState.UpdateVDiskState(&*overlay->second);
                } else if (overlay->second->IsBeingDeleted() && !base->second->IsBeingDeleted()) {
                    ScrubState.OnDeleteVSlot(overlay->first, txc);
                }
            }
            for (auto&& [base, overlay] : state.Groups.Diff()) {
                if (!overlay->second) { // group deleted
                    ScrubState.OnDeleteGroup(overlay->first);
                }
            }
        }

        void TBlobStorageController::CommitStoragePoolStatUpdates(TConfigState& state) {
            // scan for created/renamed storage pools
            for (const auto& [prev, cur] : Diff(&StoragePools, &state.StoragePools.Get())) {
                if (!prev) { // created storage pool
                    StoragePoolStat->AddStoragePool(TStoragePoolStat::ConvertId(cur->first), cur->second.Name, 0);
                } else if (cur && prev->second.Name != cur->second.Name) { // renamed storage pool
                    StoragePoolStat->RenameStoragePool(TStoragePoolStat::ConvertId(cur->first), cur->second.Name);
                }
            }

            // apply created/deleted groups
            for (const auto& [base, overlay] : state.Groups.Diff()) {
                if (!base) { // newly created group
                    overlay->second->StatusFlags = overlay->second->GetStorageStatusFlags();
                    StoragePoolStat->Update(TStoragePoolStat::ConvertId(overlay->second->StoragePoolId),
                        std::nullopt, overlay->second->StatusFlags);
                } else if (!overlay->second) { // deleted group
                    if (state.StoragePools.Get().count(base->second->StoragePoolId)) {
                        StoragePoolStat->Update(TStoragePoolStat::ConvertId(base->second->StoragePoolId),
                            base->second->StatusFlags, std::nullopt);
                    }
                }
            }

            // apply VDisks going for destruction
            for (const auto& [base, overlay] : state.VSlots.Diff()) {
                if (overlay->second && overlay->second->DeletedFromStoragePoolId && overlay->second->Metrics.GetAllocatedSize()) {
                    StoragePoolStat->UpdateAllocatedSize(TStoragePoolStat::ConvertId(*overlay->second->DeletedFromStoragePoolId),
                        -overlay->second->Metrics.GetAllocatedSize());
                }
            }

            // scan for deleted storage pools
            for (const auto& [prev, cur] : Diff(&StoragePools, &state.StoragePools.Get())) {
                if (prev && !cur) {
                    StoragePoolStat->DeleteStoragePool(TStoragePoolStat::ConvertId(prev->first));
                }
            }
        }

        void TBlobStorageController::CommitSysViewUpdates(TConfigState& state) {
            for (const auto& [base, overlay] : state.PDisks.Diff()) {
                SysViewChangedPDisks.insert(overlay->first);
            }
            for (const auto& [base, overlay] : state.VSlots.Diff()) {
                SysViewChangedPDisks.insert(overlay->first.ComprisingPDiskId());
                SysViewChangedVSlots.insert(overlay->first);
                SysViewChangedGroups.insert(overlay->second ? overlay->second->GroupId : base->second->GroupId);
            }
            for (const auto& [base, overlay] : state.Groups.Diff()) {
                SysViewChangedGroups.insert(overlay->first);
            }
            for (const auto& [prev, cur] : Diff(&StoragePools, &state.StoragePools.Get())) {
                SysViewChangedStoragePools.insert(cur ? cur->first : prev->first);
            }
        }

        void TBlobStorageController::TConfigState::ApplyConfigUpdates() {
            for (auto& msg : Outbox) {
                TActivationContext::Send(msg.release());
            }
            for (auto& fn : Callbacks) {
                fn();
            }
        }

        void TBlobStorageController::TConfigState::DestroyVSlot(const TVSlotId vslotId, const TVSlotInfo *ensureAcceptorSlot) {
            // obtain mutable slot pointer
            TVSlotInfo *mutableSlot = VSlots.FindForUpdate(vslotId);
            Y_VERIFY(mutableSlot);

            // ensure it hasn't started deletion yet
            Y_VERIFY(!mutableSlot->IsBeingDeleted());

            if (mutableSlot->Mood == TMood::Donor) {
                // this is the donor disk and it is being deleted; here we have to inform the acceptor disk of changed
                // donor set by simply removing the donor disk
                const TGroupInfo *group = Groups.Find(mutableSlot->GroupId);
                Y_VERIFY(group);
                const ui32 orderNumber = group->Topology->GetOrderNumber(mutableSlot->GetShortVDiskId());
                const TVSlotInfo *acceptor = group->VDisksInGroup[orderNumber];
                Y_VERIFY(acceptor);
                Y_VERIFY(!acceptor->IsBeingDeleted());
                Y_VERIFY(acceptor->Mood != TMood::Donor);
                Y_VERIFY(mutableSlot->GroupId == acceptor->GroupId && mutableSlot->GroupGeneration < acceptor->GroupGeneration &&
                    mutableSlot->GetShortVDiskId() == acceptor->GetShortVDiskId());

                TVSlotInfo *mutableAcceptor = VSlots.FindForUpdate(acceptor->VSlotId);
                Y_VERIFY(mutableAcceptor);
                Y_VERIFY_S(!ensureAcceptorSlot || ensureAcceptorSlot == mutableAcceptor,
                    "EnsureAcceptor# " << ensureAcceptorSlot->VSlotId << ':' << ensureAcceptorSlot->GetVDiskId()
                    << " MutableAcceptor# " << mutableAcceptor->VSlotId << ':' << mutableAcceptor->GetVDiskId()
                    << " Slot# " << mutableSlot->VSlotId << ':' << mutableSlot->GetVDiskId());

                auto& donors = mutableAcceptor->Donors;
                const size_t numErased = donors.erase(vslotId);
                Y_VERIFY(numErased == 1);
            } else {
                Y_VERIFY(!ensureAcceptorSlot);
            }

            // this is the acceptor disk and we have to delete all the donors as they are not needed anymore
            for (auto& donors = mutableSlot->Donors; !donors.empty(); ) {
                DestroyVSlot(*donors.begin(), mutableSlot);
            }

            // remove slot info from the PDisk
            TPDiskInfo *pdisk = PDisks.FindForUpdate(vslotId.ComprisingPDiskId());
            Y_VERIFY(pdisk);
            --pdisk->NumActiveSlots;

            if (UncommittedVSlots.erase(vslotId)) {
                const ui32 erased = pdisk->VSlotsOnPDisk.erase(vslotId.VSlotId);
                Y_VERIFY(erased);
                VSlots.DeleteExistingEntry(vslotId); // this slot hasn't been created yet and can be deleted safely
            } else {
                const TGroupInfo *group = Groups.Find(mutableSlot->GroupId);
                Y_VERIFY(group);
                mutableSlot->ScheduleForDeletion(group->StoragePoolId);
            }
        }

        void TBlobStorageController::TConfigState::CheckConsistency() const {
#ifndef NDEBUG
            PDisks.ForEach([&](const auto& pdiskId, const auto& pdisk) {
                ui32 numActiveSlots = 0;
                for (const auto& [vslotId, vslot] : pdisk.VSlotsOnPDisk) {
                    const TVSlotInfo *vslotInTable = VSlots.Find(TVSlotId(pdiskId, vslotId));
                    Y_VERIFY(vslot == vslotInTable);
                    Y_VERIFY(vslot->PDisk == &pdisk);
                    numActiveSlots += !vslot->IsBeingDeleted();
                }
                Y_VERIFY(pdisk.NumActiveSlots == numActiveSlots);
            });
            VSlots.ForEach([&](const auto& vslotId, const auto& vslot) {
                Y_VERIFY(vslot.VSlotId == vslotId);
                const TPDiskInfo *pdisk = PDisks.Find(vslot.VSlotId.ComprisingPDiskId());
                Y_VERIFY(vslot.PDisk == pdisk);
                const auto it = vslot.PDisk->VSlotsOnPDisk.find(vslotId.VSlotId);
                Y_VERIFY(it != vslot.PDisk->VSlotsOnPDisk.end());
                Y_VERIFY(it->second == &vslot);
                const TGroupInfo *group = Groups.Find(vslot.GroupId);
                if (!vslot.IsBeingDeleted() && vslot.Mood != TMood::Donor) {
                    Y_VERIFY(group);
                    Y_VERIFY(vslot.Group == group);
                } else {
                    Y_VERIFY(!vslot.Group);
                }
                if (vslot.Mood == TMood::Donor) {
                    Y_VERIFY(vslot.Donors.empty());
                    Y_VERIFY(group);
                    const ui32 orderNumber = group->Topology->GetOrderNumber(vslot.GetShortVDiskId());
                    const TVSlotInfo *acceptor = group->VDisksInGroup[orderNumber];
                    Y_VERIFY(acceptor);
                    Y_VERIFY(!acceptor->IsBeingDeleted());
                    Y_VERIFY(acceptor->Mood != TMood::Donor);
                    Y_VERIFY(acceptor->Donors.contains(vslotId));
                }
                for (const TVSlotId& donorVSlotId : vslot.Donors) {
                    const TVSlotInfo *donor = VSlots.Find(donorVSlotId);
                    Y_VERIFY(donor);
                    Y_VERIFY(donor->Mood == TMood::Donor);
                    Y_VERIFY(donor->GroupId == vslot.GroupId);
                    Y_VERIFY(donor->GroupGeneration < vslot.GroupGeneration + group->ContentChanged);
                    Y_VERIFY(donor->GetShortVDiskId() == vslot.GetShortVDiskId());
                }
            });
            Groups.ForEach([&](const auto& groupId, const auto& group) {
                Y_VERIFY(groupId == group.ID);
                for (const TVSlotInfo *vslot : group.VDisksInGroup) {
                    Y_VERIFY(VSlots.Find(vslot->VSlotId) == vslot);
                    Y_VERIFY(vslot->Group == &group);
                    Y_VERIFY(vslot->GroupId == groupId);
                    Y_VERIFY(vslot->GroupGeneration == group.Generation);
                }
            });
#endif
        }

        void TBlobStorageController::TPDiskInfo::OnCommit() {
            for (const auto& [id, slot] : VSlotsOnPDisk) {
                slot.Mutable().PDisk = this;
            }
        }

        void TBlobStorageController::TVSlotInfo::OnCommit() {
            PDisk.Mutable().VSlotsOnPDisk[VSlotId.VSlotId] = this;
            if (Group) {
                const ui32 index = Group->Topology->GetOrderNumber(GetShortVDiskId());
                Group.Mutable().VDisksInGroup[index] = this;
            }
            if (VSlotReadyTimestampIter != TVSlotReadyTimestampQ::iterator()) {
                VSlotReadyTimestampIter->second = this;
            }
            DeletedFromStoragePoolId.reset();
        }

        void TBlobStorageController::TGroupInfo::OnCommit() {
            for (const auto& slot : VDisksInGroup) {
                slot.Mutable().Group = this;
            }
            MoodChanged = false;
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TDefineHostConfig *pb, const THostConfigId &id,
                const THostConfigInfo &hostConfig) {
            pb->SetHostConfigId(id);
            pb->SetName(hostConfig.Name);
            pb->SetItemConfigGeneration(hostConfig.Generation.GetOrElse(1));
            for (const auto& [key, value] : hostConfig.Drives) {
                auto &drive = *pb->AddDrive();
                drive.SetPath(key.Path);
                drive.SetType(value.Type);
                drive.SetSharedWithOs(value.SharedWithOs);
                drive.SetReadCentric(value.ReadCentric);
                drive.SetKind(value.Kind);

                if (const auto& config = value.PDiskConfig) {
                    NKikimrBlobStorage::TPDiskConfig& pb = *drive.MutablePDiskConfig();
                    if (!pb.ParseFromString(*config)) {
                        throw TExError() << "HostConfigId# " << id << " undeserializable PDiskConfig string"
                            << " Path# " << key.Path;
                    }
                }
            }
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TDefineBox *pb, const TBoxId &id, const TBoxInfo &box) {
            pb->SetBoxId(id);
            pb->SetName(box.Name);
            pb->SetItemConfigGeneration(box.Generation.GetOrElse(1));
            for (const auto &userId : box.UserIds) {
                pb->AddUserId(std::get<1>(userId));
            }
            for (const auto &kv : box.Hosts) {
                auto *host = pb->AddHost();
                host->SetHostConfigId(kv.second.HostConfigId);
                host->SetEnforcedNodeId(kv.second.EnforcedNodeId.GetOrElse(0));
                auto *key = host->MutableKey();
                key->SetFqdn(kv.first.Fqdn);
                key->SetIcPort(kv.first.IcPort);
            }
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TDefineStoragePool *pb, const TBoxStoragePoolId &id, const TStoragePoolInfo &pool) {
            pb->SetBoxId(std::get<0>(id));
            pb->SetStoragePoolId(std::get<1>(id));
            pb->SetName(pool.Name);
            pb->SetItemConfigGeneration(pool.Generation.GetOrElse(1));
            pb->SetErasureSpecies(TBlobStorageGroupType::ErasureSpeciesName(pool.ErasureSpecies));
            pb->SetEncryptionMode(pool.EncryptionMode.GetOrElse(0));
            auto* geometry = pb->MutableGeometry();
            if (pool.RealmLevelBegin) {
                geometry->SetRealmLevelBegin(*pool.RealmLevelBegin);
            }
            if (pool.RealmLevelEnd) {
                geometry->SetRealmLevelEnd(*pool.RealmLevelEnd);
            }
            if (pool.DomainLevelBegin) {
                geometry->SetDomainLevelBegin(*pool.DomainLevelBegin);
            }
            if (pool.DomainLevelEnd) {
                geometry->SetDomainLevelEnd(*pool.DomainLevelEnd);
            }
            if (pool.NumFailRealms) {
                geometry->SetNumFailRealms(*pool.NumFailRealms);
            }
            if (pool.NumFailDomainsPerFailRealm) {
                geometry->SetNumFailDomainsPerFailRealm(*pool.NumFailDomainsPerFailRealm);
            }
            if (pool.NumVDisksPerFailDomain) {
                geometry->SetNumVDisksPerFailDomain(*pool.NumVDisksPerFailDomain);
            }
            if (pool.SchemeshardId && pool.PathItemId) {
                auto *x = pb->MutableScopeId();
                x->SetX1(*pool.SchemeshardId);
                x->SetX2(*pool.PathItemId);
            }

            // group geometry
            if (pool.HasGroupGeometry()) {
                pb->MutableGeometry()->CopyFrom(pool.GetGroupGeometry());
            }

            // serialize VDiskKind as a string
            {
                const google::protobuf::EnumDescriptor *descr = NKikimrBlobStorage::TVDiskKind::EVDiskKind_descriptor();
                const google::protobuf::EnumValueDescriptor *value = descr->FindValueByNumber(pool.VDiskKind);
                if (!value) {
                    throw TExError() << "invalid VDiskKind# " << pool.VDiskKind;
                }
                pb->SetVDiskKind(value->name());
            }

            // usage pattern
            if (pool.HasUsagePattern()) {
                pb->MutableUsagePattern()->CopyFrom(pool.GetUsagePattern());
            }

            pb->SetKind(pool.Kind);
            pb->SetNumGroups(pool.NumGroups);
            pb->SetRandomizeGroupMapping(pool.RandomizeGroupMapping);

            for (const auto &userId : pool.UserIds) {
                pb->AddUserId(std::get<2>(userId));
            }

            for (const TStoragePoolInfo::TPDiskFilter &filter : pool.PDiskFilters) {
                Serialize(pb->AddPDiskFilter(), filter);
            }
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TPDiskFilter *pb, const TStoragePoolInfo::TPDiskFilter &filter) {
#define SERIALIZE_VARIABLE(NAME) (filter.NAME && (pb->AddProperty()->Set##NAME(*filter.NAME), true))
            SERIALIZE_VARIABLE(Type);
            SERIALIZE_VARIABLE(SharedWithOs);
            SERIALIZE_VARIABLE(ReadCentric);
            SERIALIZE_VARIABLE(Kind);
#undef SERIALIZE_VARIABLE
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TBaseConfig::TPDisk *pb, const TPDiskId &id, const TPDiskInfo &pdisk) {
            const TPDiskCategory category(pdisk.Kind);

            pb->SetNodeId(id.NodeId);
            pb->SetPDiskId(id.PDiskId);
            pb->SetPath(pdisk.Path);
            pb->SetType(PDiskTypeToPDiskType(category.Type()));
            pb->SetSharedWithOs(!pdisk.SharedWithOs ? NKikimrBlobStorage::ETriStateBool::kNotSet
                : *pdisk.SharedWithOs ? NKikimrBlobStorage::ETriStateBool::kTrue
                : NKikimrBlobStorage::ETriStateBool::kFalse);
            pb->SetReadCentric(!pdisk.ReadCentric ? NKikimrBlobStorage::ETriStateBool::kNotSet
                : *pdisk.ReadCentric ? NKikimrBlobStorage::ETriStateBool::kTrue
                : NKikimrBlobStorage::ETriStateBool::kFalse);
            pb->SetKind(category.Kind());
            pb->SetGuid(pdisk.Guid);
            if (pdisk.PDiskConfig && !pb->MutablePDiskConfig()->ParseFromString(pdisk.PDiskConfig)) {
                throw TExError() << "failed to parse PDiskConfig for PDisk# " << id;
            }
            pb->SetBoxId(pdisk.BoxId);
            pb->SetNumStaticSlots(pdisk.StaticSlotUsage);
            pb->SetDriveStatus(pdisk.Status);
            pb->SetExpectedSlotCount(pdisk.ExpectedSlotCount);
            pb->SetDriveStatusChangeTimestamp(pdisk.StatusTimestamp.GetValue());
            pb->SetDecommitStatus(pdisk.DecommitStatus);
            pb->MutablePDiskMetrics()->CopyFrom(pdisk.Metrics);
            pb->MutablePDiskMetrics()->ClearPDiskId();
            pb->SetExpectedSerial(pdisk.ExpectedSerial);
            pb->SetLastSeenSerial(pdisk.LastSeenSerial);
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TVSlotId *pb, TVSlotId id) {
            id.Serialize(pb);
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TVDiskLocation *pb, const TVSlotInfo& vslot) {
            pb->SetNodeID(vslot.VSlotId.NodeId);
            pb->SetPDiskID(vslot.VSlotId.PDiskId);
            pb->SetVDiskSlotID(vslot.VSlotId.VSlotId);
            pb->SetPDiskGuid(vslot.PDisk->Guid);
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TVDiskLocation *pb, const TVSlotId& vslotId) {
            pb->SetNodeID(vslotId.NodeId);
            pb->SetPDiskID(vslotId.PDiskId);
            pb->SetVDiskSlotID(vslotId.VSlotId);
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TBaseConfig::TVSlot *pb, const TVSlotInfo &vslot,
                const TVSlotFinder& finder) {
            Serialize(pb->MutableVSlotId(), vslot.VSlotId);
            pb->SetGroupId(vslot.GroupId);
            pb->SetGroupGeneration(vslot.GroupGeneration);
            pb->SetVDiskKind(NKikimrBlobStorage::TVDiskKind::EVDiskKind_Name(vslot.Kind));
            pb->SetFailRealmIdx(vslot.RingIdx);
            pb->SetFailDomainIdx(vslot.FailDomainIdx);
            pb->SetVDiskIdx(vslot.VDiskIdx);
            pb->SetAllocatedSize(vslot.Metrics.GetAllocatedSize());
            pb->MutableVDiskMetrics()->CopyFrom(vslot.Metrics);
            pb->MutableVDiskMetrics()->ClearVDiskId();
            pb->SetStatus(NKikimrBlobStorage::EVDiskStatus_Name(vslot.Status));
            for (const TVSlotId& vslotId : vslot.Donors) {
                auto *item = pb->AddDonors();
                Serialize(item->MutableVSlotId(), vslotId);
                finder(vslotId, [item](const TVSlotInfo& vslot) {
                    VDiskIDFromVDiskID(vslot.GetVDiskId(), item->MutableVDiskId());
                    item->MutableVDiskMetrics()->CopyFrom(vslot.Metrics);
                    item->MutableVDiskMetrics()->ClearVDiskId();
                });
            }
            pb->SetReady(vslot.IsReady);
        }

        void TBlobStorageController::Serialize(NKikimrBlobStorage::TBaseConfig::TGroup *pb, const TGroupInfo &group) {
            pb->SetGroupId(group.ID);
            pb->SetGroupGeneration(group.Generation);
            pb->SetErasureSpecies(TBlobStorageGroupType::ErasureSpeciesName(group.ErasureSpecies));
            for (const TVSlotInfo *vslot : group.VDisksInGroup) {
                Serialize(pb->AddVSlotId(), vslot->VSlotId);
            }
            pb->SetBoxId(std::get<0>(group.StoragePoolId));
            pb->SetStoragePoolId(std::get<1>(group.StoragePoolId));
            pb->SetSeenOperational(group.SeenOperational);

            const auto& status = group.Status;
            pb->SetOperatingStatus(status.OperatingStatus);
            pb->SetExpectedStatus(status.ExpectedStatus);

            if (group.DecommitStatus != NKikimrBlobStorage::TGroupDecommitStatus::NONE || group.VirtualGroupState) {
                auto *vgi = pb->MutableVirtualGroupInfo();
                if (group.VirtualGroupState) {
                    vgi->SetState(*group.VirtualGroupState);
                }
                if (group.VirtualGroupName) {
                    vgi->SetName(*group.VirtualGroupName);
                }
                if (group.BlobDepotId) {
                    vgi->SetBlobDepotId(*group.BlobDepotId);
                }
                if (group.ErrorReason) {
                    vgi->SetErrorReason(*group.ErrorReason);
                }
                vgi->SetDecommitStatus(group.DecommitStatus);
            }
        }

        void TBlobStorageController::SerializeDonors(NKikimrBlobStorage::TNodeWardenServiceSet::TVDisk *vdisk,
                const TVSlotInfo& vslot, const TGroupInfo& group, const TVSlotFinder& finder) {
            if (vslot.Mood == TMood::Donor) {
                ui32 numFailRealms = 0, numFailDomainsPerFailRealm = 0, numVDisksPerFailDomain = 0;
                for (const TVSlotInfo *slot : group.VDisksInGroup) {
                    numFailRealms = Max(numFailRealms, slot->RingIdx + 1);
                    numFailDomainsPerFailRealm = Max(numFailDomainsPerFailRealm, slot->FailDomainIdx + 1);
                    numVDisksPerFailDomain = Max(numVDisksPerFailDomain, slot->VDiskIdx + 1);
                }
                Y_VERIFY(numFailRealms * numFailDomainsPerFailRealm * numVDisksPerFailDomain == group.VDisksInGroup.size());
                auto *pb = vdisk->MutableDonorMode();
                pb->SetNumFailRealms(numFailRealms);
                pb->SetNumFailDomainsPerFailRealm(numFailDomainsPerFailRealm);
                pb->SetNumVDisksPerFailDomain(numVDisksPerFailDomain);
                pb->SetErasureSpecies(group.ErasureSpecies);
            }

            std::vector<std::pair<TVDiskID, TVSlotId>> donors;
            for (const TVSlotId& donorVSlotId : vslot.Donors) {
                std::optional<TVDiskID> vdiskId;
                finder(donorVSlotId, [&](const TVSlotInfo& donor) {
                    vdiskId.emplace(donor.GetVDiskId());
                });
                Y_VERIFY(vdiskId);
                donors.emplace_back(*vdiskId, donorVSlotId);
            }

            std::sort(donors.begin(), donors.end());
            if (!donors.empty()) {
                for (size_t i = 0; i < donors.size() - 1; ++i) {
                    const auto& x = donors[i].first;
                    const auto& y = donors[i + 1].first;
                    Y_VERIFY(x.GroupID == y.GroupID);
                    Y_VERIFY(x.GroupGeneration < y.GroupGeneration);
                    Y_VERIFY(TVDiskIdShort(x) == TVDiskIdShort(y));
                }
            }

            for (const auto& [donorVDiskId, donorVSlotId] : donors) {
                auto *pb = vdisk->AddDonors();
                VDiskIDFromVDiskID(donorVDiskId, pb->MutableVDiskId());
                Serialize(pb->MutableVDiskLocation(), donorVSlotId);
            }
        }

        void TBlobStorageController::SerializeGroupInfo(NKikimrBlobStorage::TGroupInfo *group, const TGroupInfo& groupInfo,
                const TString& storagePoolName, const TMaybe<TKikimrScopeId>& scopeId) {
            group->SetGroupID(groupInfo.ID);
            group->SetGroupGeneration(groupInfo.Generation);
            group->SetStoragePoolName(storagePoolName);

            group->SetEncryptionMode(groupInfo.EncryptionMode.GetOrElse(0));
            group->SetLifeCyclePhase(groupInfo.LifeCyclePhase.GetOrElse(0));
            group->SetMainKeyId(groupInfo.MainKeyId.GetOrElse(""));
            group->SetEncryptedGroupKey(groupInfo.EncryptedGroupKey.GetOrElse(""));
            group->SetGroupKeyNonce(groupInfo.GroupKeyNonce.GetOrElse(0));
            group->SetMainKeyVersion(groupInfo.MainKeyVersion.GetOrElse(0));

            if (scopeId) {
                auto *pb = group->MutableAcceptedScope();
                const TScopeId& x = scopeId->GetInterconnectScopeId();
                pb->SetX1(x.first);
                pb->SetX2(x.second);
            }

            if (groupInfo.VDisksInGroup) {
                group->SetErasureSpecies(groupInfo.ErasureSpecies);
                group->SetDeviceType(PDiskTypeToPDiskType(groupInfo.GetCommonDeviceType()));

                std::vector<std::pair<TVDiskID, const TVSlotInfo*>> vdisks;
                for (const auto& vslot : groupInfo.VDisksInGroup) {
                    vdisks.emplace_back(vslot->GetVDiskId(), vslot);
                }
                auto comp = [](const auto& x, const auto& y) { return x.first < y.first; };
                std::sort(vdisks.begin(), vdisks.end(), comp);

                TVDiskID prevVDiskId;
                NKikimrBlobStorage::TGroupInfo::TFailRealm *realm = nullptr;
                NKikimrBlobStorage::TGroupInfo::TFailRealm::TFailDomain *domain = nullptr;
                for (const auto& [vdiskId, vslot] : vdisks) {
                    if (!realm || prevVDiskId.FailRealm != vdiskId.FailRealm) {
                        realm = group->AddRings();
                        domain = nullptr;
                    }
                    if (!domain || prevVDiskId.FailDomain != vdiskId.FailDomain) {
                        Y_VERIFY(realm);
                        domain = realm->AddFailDomains();
                    }
                    prevVDiskId = vdiskId;

                    Serialize(domain->AddVDiskLocations(), *vslot);
                }
            }

            if (groupInfo.VirtualGroupState == NKikimrBlobStorage::EVirtualGroupState::WORKING) {
                Y_VERIFY(groupInfo.BlobDepotId);
                group->SetBlobDepotId(*groupInfo.BlobDepotId);
            } else if (groupInfo.VirtualGroupState == NKikimrBlobStorage::EVirtualGroupState::CREATE_FAILED) {
                group->SetBlobDepotId(0);
            }

            if (groupInfo.DecommitStatus != NKikimrBlobStorage::TGroupDecommitStatus::NONE) {
                group->SetDecommitStatus(groupInfo.DecommitStatus);
            }
        }

} // NKikimr::NBsController
