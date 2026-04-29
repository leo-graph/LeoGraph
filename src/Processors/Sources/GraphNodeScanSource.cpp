#include <Processors/Sources/GraphNodeScanSource.h>

#include <Columns/ColumnsNumber.h>

namespace DB
{

GraphNodeScanSource::GraphNodeScanSource(SharedHeader header_)
    : ISource(std::move(header_))
{
}

Chunk GraphNodeScanSource::generate()
{
    if (generated)
        return {};

    generated = true;
    MutableColumns columns;
    columns.emplace_back(ColumnUInt64::create(1, 1));
    return Chunk(std::move(columns), 1);
}

}
