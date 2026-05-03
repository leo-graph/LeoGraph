#pragma once

#include <Processors/ISource.h>
#include <Processors/QueryPlan/Graph/MatchSpec.h>

#include <memory>

namespace DB::Graph
{

class IMatchSourceReader
{
public:
    virtual ~IMatchSourceReader() = default;
    virtual Chunk generate() = 0;
};

class IMatchSourceFactory
{
public:
    virtual ~IMatchSourceFactory() = default;
    virtual std::unique_ptr<IMatchSourceReader> createReader(SharedHeader header, const MatchSpec & match_spec) const = 0;
};

using MatchSourceFactoryPtr = std::shared_ptr<const IMatchSourceFactory>;

MatchSourceFactoryPtr getEmptyMatchSourceFactory();

class MatchSource final : public ISource
{
public:
    MatchSource(SharedHeader header_, MatchSpec match_spec_, std::unique_ptr<IMatchSourceReader> reader_);

    String getName() const override { return "GraphMatchSource"; }

protected:
    Chunk generate() override;

private:
    MatchSpec match_spec;
    std::unique_ptr<IMatchSourceReader> reader;
};

}
