#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLYieldClause final : public DB::IAST
{
public:
    PtrList items;

    String getID(char) const override { return "GQLYieldClause"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLYieldClause>(*this);
        result->children.clear();
        detail::cloneChildrenList(items, result->items, result->children);
        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        ostr << "YIELD ";
        detail::formatChildren(ostr, settings, state, frame, items, ", ");
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        for (auto & item : items)
            f(nullptr, &item);
    }
};

}
