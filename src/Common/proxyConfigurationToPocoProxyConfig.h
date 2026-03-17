#pragma once

#include <Common/ProxyConfiguration.h>
#include <Poco/Net/HTTPClientSession.h>

namespace DB {

Poco::Net::HTTPClientSession::ProxyConfig proxyConfigurationToPocoProxyConfig(const DB::ProxyConfiguration& proxy_configuration);

std::string buildPocoNonProxyHosts(const std::string& no_proxy_hosts_string);

}  // namespace DB
