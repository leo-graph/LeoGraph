#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLSelectSourceItem final : public DB::IAST
{
public:
    Ptr graph_reference;
    Ptr source;

    String getID(char) const override { return "GQLSelectSourceItem"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLSelectSourceItem>(*this);
        result->children.clear();
        result->graph_reference = graph_reference ? graph_reference->clone() : Ptr{};
        result->source = source ? source->clone() : Ptr{};

        if (result->graph_reference)
            result->children.push_back(result->graph_reference);

        if (result->source)
            result->children.push_back(result->source);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        if (graph_reference)
            graph_reference->format(ostr, settings, state, frame);

        if (source)
        {
            if (graph_reference)
                ostr << " ";

            source->format(ostr, settings, state, frame);
        }
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &graph_reference);
        f(nullptr, &source);
    }
};

}
