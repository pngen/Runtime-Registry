#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[03_endpoint_registration]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  EndpointId ep = rr_examples::add_endpoint(reg, env, 1, 31817);
  const EndpointDescriptor* e = reg.find_endpoint(ep);
  std::printf("  endpoint=%llu locator=%s transport=%s reach=%s\n",
    (unsigned long long)ep.value(), e ? e->locator.text.c_str() : "?", e ? to_string(e->transport).data() : "?", e ? to_string(e->reachability).data() : "?");
  return 0;
}
