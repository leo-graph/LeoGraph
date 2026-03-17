#pragma once
#include <Common/ObjectStorageKeyGenerator.h>
#include <Core/Types.h>
#include "config.h"

#if USE_AWS_S3

namespace Poco::Util {
class AbstractConfiguration;
}

namespace DB {
namespace S3 {
struct URI;
}

ObjectStorageKeyGeneratorPtr getKeyGenerator(const S3::URI& uri, const Poco::Util::AbstractConfiguration& config,
                                             const String& config_prefix);

}  // namespace DB

#endif
