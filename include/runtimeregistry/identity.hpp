#pragma once
// Runtime Registry identity and generation types.
// Each identity is a distinct, non-interchangeable strong type. Each
// generation is distinct and supports explicit comparison.

#include <runtimeregistry/strong.hpp>

namespace runtimeregistry {

// ---- Identity domains ---------------------------------------------------
#define RR_ID(Tag) struct Tag##IdTag; using Tag = Id<Tag##IdTag>

RR_ID(RegistryId);
RR_ID(RegistryRecordId);
RR_ID(ServiceId);
RR_ID(ServiceInstanceId);
RR_ID(RuntimeId);
RR_ID(RuntimeInstanceId);
RR_ID(NodeId);
RR_ID(WorkerId);
RR_ID(WorkerBootId);
RR_ID(ProcessId);
RR_ID(EndpointId);
RR_ID(ProtocolId);
RR_ID(BackendId);
RR_ID(DeviceId);
RR_ID(CapabilityId);
RR_ID(CompatibilityId);
RR_ID(LeaseId);
RR_ID(ObservationId);
RR_ID(QueryId);
RR_ID(ResultId);
RR_ID(TombstoneId);
RR_ID(AttemptId);
RR_ID(DispatchId);
RR_ID(OwnerId);
RR_ID(PolicyId);

#undef RR_ID

// ---- Generation domains ------------------------------------------------
#define RR_GEN(Tag) struct Tag##GenTag; using Tag = Gen<Tag##GenTag>

RR_GEN(CoordinatorEpoch);
RR_GEN(RegistryGeneration);
RR_GEN(RecordGeneration);
RR_GEN(ServiceGeneration);
RR_GEN(ServiceInstanceGeneration);
RR_GEN(RuntimeGeneration);
RR_GEN(RuntimeInstanceGeneration);
RR_GEN(NodeGeneration);
RR_GEN(WorkerGeneration);
RR_GEN(EndpointGeneration);
RR_GEN(ProtocolGeneration);
RR_GEN(BackendGeneration);
RR_GEN(DeviceGeneration);
RR_GEN(CapabilityGeneration);
RR_GEN(CompatibilityGeneration);
RR_GEN(LeaseGeneration);
RR_GEN(ObservationGeneration);
RR_GEN(QueryGeneration);
RR_GEN(AttemptGeneration);
RR_GEN(DispatchGeneration);
RR_GEN(PolicyGeneration);
RR_GEN(AuthorityGeneration);

#undef RR_GEN

}  // namespace runtimeregistry
