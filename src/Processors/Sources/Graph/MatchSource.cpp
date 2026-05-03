#include <Processors/Sources/Graph/MatchSource.h>

#include <utility>

namespace DB::Graph
{
namespace
{

class EmptyMatchSourceReader final : public IMatchSourceReader
{
public:
    Chunk generate() override
    {
        /// Real graph storage is not yet wired up: emit no rows.
        /// A future change will plug the actual storage layer through this reader contract.
        return {};
    }
};

class EmptyMatchSourceFactory final : public IMatchSourceFactory
{
public:
    std::unique_ptr<IMatchSourceReader> createReader(SharedHeader, const MatchSpec &) const override
    {
        return std::make_unique<EmptyMatchSourceReader>();
    }
};

}

MatchSourceFactoryPtr getEmptyMatchSourceFactory()
{
    static auto factory = std::make_shared<EmptyMatchSourceFactory>();
    return factory;
}

MatchSource::MatchSource(SharedHeader header_, MatchSpec match_spec_, std::unique_ptr<IMatchSourceReader> reader_)
    : ISource(std::move(header_))
    , match_spec(std::move(match_spec_))
    , reader(std::move(reader_))
{
    if (!reader)
        reader = std::make_unique<EmptyMatchSourceReader>();
}

Chunk MatchSource::generate()
{
    return reader->generate();
}

}
