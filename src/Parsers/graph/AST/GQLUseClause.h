#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLUseClause final : public DB::IAST
{
public:
    Ptr graph_reference;

    explicit GQLUseClause(Ptr graph_reference_ = {})
        : graph_reference(std::move(graph_reference_))
    {
        if (graph_reference)
            children.push_back(graph_reference);
    }

    String getID(char) const override { return "GQLUseClause"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLUseClause>(*this);
        result->children.clear();
        result->graph_reference = graph_reference ? graph_reference->clone() : Ptr{};

        if (result->graph_reference)
            result->children.push_back(result->graph_reference);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        ostr << "USE ";

        if (graph_reference)
            graph_reference->format(ostr, settings, state, frame);
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &graph_reference);
    }
};

}
