#include "example_util.hpp"
#include <runtimeregistry/identity.hpp>
using namespace runtimeregistry;
int main() {
  std::printf("[01_service_identity]\n");
  ServiceId a(42); ServiceInstanceId b(7); RuntimeId c(9);
  // Identity types are distinct and never implicitly interconvert.
  std::printf("  ServiceId=%llu ServiceInstanceId=%llu RuntimeId=%llu\n",
    (unsigned long long)a.value(), (unsigned long long)b.value(), (unsigned long long)c.value());
  ServiceGeneration g1(1), g2(2);
  std::printf("  generation compare: g1<g2=%d g2>g1=%d\n", (int)(g1<g2), (int)(g2>g1));
  std::printf("  done\n");
  return 0;
}
