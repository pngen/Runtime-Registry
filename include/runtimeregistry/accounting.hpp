#pragma once
// Non-negative accounting counters. Counters only move in ways that cannot go
// negative; duplicate removal/expiration/invalidation never double-account.

#include <cstdint>

namespace runtimeregistry {

struct Accounting {
  std::int64_t services{0};
  std::int64_t service_instances{0};
  std::int64_t runtimes{0};
  std::int64_t runtime_instances{0};
  std::int64_t nodes{0};
  std::int64_t endpoints{0};
  std::int64_t protocols{0};
  std::int64_t backends{0};
  std::int64_t devices{0};
  std::int64_t capabilities{0};
  std::int64_t leases{0};

  std::int64_t available_instances{0};
  std::int64_t degraded_instances{0};
  std::int64_t stale_instances{0};
  std::int64_t unreachable_instances{0};

  std::int64_t active_leases{0};
  std::int64_t expired_leases{0};

  std::int64_t registrations{0};
  std::int64_t deregistrations{0};
  std::int64_t supersessions{0};
  std::int64_t invalidations{0};
  std::int64_t tombstones{0};

  std::int64_t queries{0};
  std::int64_t exact_hits{0};
  std::int64_t compatible_hits{0};
  std::int64_t misses{0};
  std::int64_t stale_only{0};
  std::int64_t unready_only{0};
  std::int64_t incompatible{0};

  std::int64_t stale_mutation_rejections{0};
  std::int64_t duplicate_conflict_rejections{0};
  std::int64_t worker_restarts{0};
};

}  // namespace runtimeregistry
