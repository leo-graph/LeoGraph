#include <Processors/Sources/Graph/NodeScanSource.h>

namespace DB::Graph
{

NodeScanSource::NodeScanSource(SharedHeader header_)
    : ISource(std::move(header_))
{
}

Chunk NodeScanSource::generate()
{
    /// Real graph storage is not yet wired up: emit no rows.
    /// A future change will plug the actual storage layer through this source.
    return {};
}

}
