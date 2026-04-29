#pragma once

#include <Processors/ISource.h>

namespace DB
{

class GraphNodeScanSource final : public ISource
{
public:
    explicit GraphNodeScanSource(SharedHeader header_);

    String getName() const override { return "GraphNodeScanSource"; }

protected:
    Chunk generate() override;

private:
    bool generated = false;
};

}
