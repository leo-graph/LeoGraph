#pragma once

#include <Processors/ISource.h>

namespace DB::Graph
{

class NodeScanSource final : public ISource
{
public:
    explicit NodeScanSource(SharedHeader header_);

    String getName() const override { return "GraphNodeScanSource"; }

protected:
    Chunk generate() override;
};

}
