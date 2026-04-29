#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLParenthesizedPathPattern final : public DB::IAST
{
public:
    String getID(char) const override
    {
        return "GQLParenthesizedPathPattern";
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLParenthesizedPathPattern>(*this);
        result->children.clear();
        result->subpath_variable = subpath_variable ? subpath_variable->clone() : Ptr{};
        result->prefix = prefix ? prefix->clone() : Ptr{};
        result->expression = expression ? expression->clone() : Ptr{};
        result->where = where ? where->clone() : Ptr{};
        result->quantifier = quantifier ? quantifier->clone() : Ptr{};

        if (result->subpath_variable)
            result->children.push_back(result->subpath_variable);
        if (result->prefix)
            result->children.push_back(result->prefix);
        if (result->expression)
            result->children.push_back(result->expression);
        if (result->where)
            result->children.push_back(result->where);
        if (result->quantifier)
            result->children.push_back(result->quantifier);

        return result;
    }

    Ptr subpath_variable;
    Ptr prefix;
    /// `expression` is a single child of this node; its operands/factors stay nested under it.
    Ptr expression;
    Ptr where;
    Ptr quantifier;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        ostr << "(";

        if (subpath_variable)
        {
            subpath_variable->format(ostr, settings, state, frame);
            ostr << " = ";
        }

        if (prefix)
        {
            prefix->format(ostr, settings, state, frame);
            ostr << " ";
        }

        if (expression)
            expression->format(ostr, settings, state, frame);

        if (where)
        {
            ostr << " ";
            where->format(ostr, settings, state, frame);
        }

        ostr << ")";

        if (quantifier)
            quantifier->format(ostr, settings, state, frame);
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &subpath_variable);
        f(nullptr, &prefix);
        f(nullptr, &expression);
        f(nullptr, &where);
        f(nullptr, &quantifier);
    }
};

}  // namespace DB::OPENGQL::AST
