#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLQuantifiedPathPrimary final : public DB::IAST
{
public:
    String getID(char) const override
    {
        return "GQLQuantifiedPathPrimary";
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLQuantifiedPathPrimary>(*this);
        result->children.clear();
        result->operand = operand ? operand->clone() : Ptr{};
        result->quantifier = quantifier ? quantifier->clone() : Ptr{};

        if (result->operand)
            result->children.push_back(result->operand);
        if (result->quantifier)
            result->children.push_back(result->quantifier);

        return result;
    }

    Ptr operand;
    Ptr quantifier;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        if (operand)
            operand->format(ostr, settings, state, frame);
        if (quantifier)
            quantifier->format(ostr, settings, state, frame);
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &operand);
        f(nullptr, &quantifier);
    }
};

}
