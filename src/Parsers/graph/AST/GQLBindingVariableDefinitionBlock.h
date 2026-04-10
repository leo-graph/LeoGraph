#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLBindingVariableDefinitionBlock final : public DB::IAST
{
public:
    PtrList definitions;

    String getID(char) const override { return "GQLBindingVariableDefinitionBlock"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLBindingVariableDefinitionBlock>(*this);
        result->children.clear();
        detail::cloneChildrenList(definitions, result->definitions, result->children);
        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        detail::formatChildren(ostr, settings, state, frame, definitions, " ");
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        for (auto & definition : definitions)
            f(nullptr, &definition);
    }
};

}
