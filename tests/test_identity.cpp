#include <runtimeregistry/identity.hpp>
#include <runtimeregistry/version.hpp>
#include <runtimeregistry/selection.hpp>
#include <runtimeregistry/strong.hpp>
#include <sstream>
#include "test_util.hpp"

using namespace runtimeregistry;

namespace {

void test_identity_types_noninterchangeable() {
  ServiceId a(5);
  ServiceInstanceId b(4);
  // Different strong types are distinct; no implicit conversion.
  CHECK(a.value() == 5);
  CHECK(b.value() == 4);
  CHECK(a != ServiceId(4));
  CHECK(a == ServiceId(5));
  CHECK(a < ServiceId(6));
  // generation ordering
  ServiceGeneration g1(1), g2(2);
  CHECK(g1 < g2);
  CHECK(g2 > g1);
  CHECK(g2 != g1);
  // Id operator<< deterministic via stream
  std::string s;
  { std::ostringstream os; os << a; s = os.str(); }
  CHECK(s == "Id(5)");
}

void test_semantic_version_ordering() {
  auto v19 = SemanticVersion::parse("1.9.0");
  auto v110 = SemanticVersion::parse("1.10.0");
  CHECK(v19 && v110);
  CHECK(*v19 < *v110);
  auto v1 = SemanticVersion::parse("1.0.0");
  auto v2 = SemanticVersion::parse("1.0.0");
  CHECK(v1 && v2);
  CHECK(*v1 == *v2);
  // prerelease < release
  auto pre = SemanticVersion::parse("1.0.0-alpha");
  CHECK(pre && *pre < *v1);
  // build metadata ignored in precedence
  auto bld = SemanticVersion::parse("1.0.0+build.5");
  CHECK(bld && *bld == *v1);
  // malformed
  CHECK(!SemanticVersion::parse(""));
  CHECK(!SemanticVersion::parse("1.2"));
  CHECK(!SemanticVersion::parse("1.2.3.4"));
  CHECK(!SemanticVersion::parse("a.b.c"));
  CHECK(!SemanticVersion::parse("01.2.3"));
}

void test_structured_version_ordering() {
  auto v19 = ApiVersion::parse("1.9");
  auto v110 = ApiVersion::parse("1.10");
  CHECK(v19 && v110);
  CHECK(*v19 < *v110);
  CHECK(*v19 != *v110);
  CHECK(!ApiVersion::parse(""));
  CHECK(!ApiVersion::parse("1.x"));
  CHECK(!ApiVersion::parse("1.2.3.4"));
  // major compatibility
  CHECK(ApiVersion(2, 0).major_compatible(ApiVersion(2, 5)));
  CHECK(!ApiVersion(2, 0).major_compatible(ApiVersion(1, 5)));
  // range
  CHECK(ApiVersion(2, 3).within_range(ApiVersion(2, 0), ApiVersion(2, 5)));
  CHECK(!ApiVersion(3, 0).within_range(ApiVersion(2, 0), ApiVersion(2, 5)));
  // satisfies min
  CHECK(ApiVersion(2, 5).satisfies_min(ApiVersion(2, 0)));
  CHECK(!ApiVersion(1, 9).satisfies_min(ApiVersion(2, 0)));
}

void test_protocol_negotiation() {
  ProtocolVersion req(2, 0);
  CHECK(negotiate_protocol(req, ProtocolVersion(2, 0), false) == NegotiationOutcome::EXACT);
  CHECK(negotiate_protocol(req, ProtocolVersion(2, 1), false) == NegotiationOutcome::UPGRADE_REQUIRED);
  CHECK(negotiate_protocol(req, ProtocolVersion(2, 0), false) == NegotiationOutcome::EXACT);
  CHECK(negotiate_protocol(ProtocolVersion(2, 1), ProtocolVersion(2, 0), false) == NegotiationOutcome::DOWNGRADE_ALLOWED);
  CHECK(negotiate_protocol(req, ProtocolVersion(3, 0), false) == NegotiationOutcome::INCOMPATIBLE);
  CHECK(negotiate_protocol(req, ProtocolVersion(2, 1), true) == NegotiationOutcome::INCOMPATIBLE);
  CHECK(negotiate_protocol(req, ProtocolVersion(2, 0), true) == NegotiationOutcome::EXACT);
}

void test_capability_satisfaction() {
  CapabilityDescriptor c;
  c.kind = CapabilityKind::CUDA_AVAILABLE;
  c.freshness = Freshness::CURRENT;
  CHECK(capability_satisfaction(c, CapabilityKind::CUDA_AVAILABLE) == -1);
  CHECK(capability_satisfaction(c, CapabilityKind::CUDA_MEMORY) == 1);
  c.freshness = Freshness::STALE;
  CHECK(capability_satisfaction(c, CapabilityKind::CUDA_AVAILABLE) == 0);
  c.freshness = Freshness::UNKNOWN;
  CHECK(capability_satisfaction(c, CapabilityKind::CUDA_AVAILABLE) == 0);
}

}  // namespace

void test_identity_suite();
void test_identity_suite() {
  test_identity_types_noninterchangeable();
  test_semantic_version_ordering();
  test_structured_version_ordering();
  test_protocol_negotiation();
  test_capability_satisfaction();
}
RR_REGISTER(test_identity_suite);

int main() {
  return rr_test::run_all("identity/version");
}
