#include <Common/CaresPTRResolver.h>
#include <Common/DNSPTRResolverProvider.h>

namespace DB {
std::shared_ptr<DNSPTRResolver> DNSPTRResolverProvider::get() {
  static auto resolver = std::make_shared<CaresPTRResolver>(CaresPTRResolver::provider_token{});

  return resolver;
}
}  // namespace DB
