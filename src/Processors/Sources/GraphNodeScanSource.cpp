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
    auto column = ColumnUInt64::create();
    auto & data = column->getData();
    data.push_back(1);
    data.push_back(2);
    data.push_back(3);
    columns.emplace_back(std::move(column));
    return Chunk(std::move(columns), 3);
}

}
