#pragma once

#include <Core/NamesAndTypes.h>
#include <Parsers/IAST_fwd.h>
#include <Processors/Sources/Graph/MatchSource.h>
#include <base/types.h>

#include <vector>

namespace DB
{
class Block;
}

namespace DB::GQL
{

enum class BindingKind : UInt8
{
    Source,
    Projection,
};

struct PlanBinding
{
    String name;
    DataTypePtr type;
    BindingKind kind = BindingKind::Source;
    ASTPtr expression;
};

class PlanScope final
{
public:
    void replaceWithHeader(const Block & header, BindingKind kind);
    void addOrReplaceBinding(String name, DataTypePtr type, BindingKind kind, ASTPtr expression = {});
    void setActiveGraph(ASTPtr graph_reference_);
    void setMatchSourceFactory(Graph::MatchSourceFactoryPtr match_source_factory_);
    PlanScope makeChildGraphScope() const;

    bool hasBinding(const String & name) const;
    const PlanBinding * tryGetBinding(const String & name) const;
    const std::vector<PlanBinding> & getBindings() const { return bindings; }
    const ASTPtr & getActiveGraph() const { return active_graph; }
    const Graph::MatchSourceFactoryPtr & getMatchSourceFactory() const { return match_source_factory; }

private:
    std::vector<PlanBinding> bindings;
    ASTPtr active_graph;
    Graph::MatchSourceFactoryPtr match_source_factory;
};

}
