#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLSimplifiedPathPattern final : public DB::IAST
{
public:
    explicit GQLSimplifiedPathPattern(EdgeDirection default_direction_)
        : default_direction(default_direction_)
    {
    }

    String getID(char) const override
    {
        return "GQLSimplifiedPathPattern";
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLSimplifiedPathPattern>(*this);
        result->children.clear();
        result->expression = expression ? expression->clone() : Ptr{};
        result->quantifier = quantifier ? quantifier->clone() : Ptr{};

        if (result->expression)
            result->children.push_back(result->expression);
        if (result->quantifier)
            result->children.push_back(result->quantifier);

        return result;
    }

    EdgeDirection default_direction;
    Ptr expression;
    Ptr quantifier;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        detail::formatSimplifiedPathPrefix(ostr, default_direction);

        if (expression)
            expression->format(ostr, settings, state, frame);

        detail::formatSimplifiedPathSuffix(ostr, default_direction);

        if (quantifier)
            quantifier->format(ostr, settings, state, frame);
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &expression);
        f(nullptr, &quantifier);
    }
};

}
