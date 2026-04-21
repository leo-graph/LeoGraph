#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLPathPattern final : public DB::IAST
{
public:
    String getID(char) const override
    {
        return "GQLPathPattern";
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLPathPattern>(*this);
        result->children.clear();
        result->variable = variable ? variable->clone() : Ptr{};
        result->prefix = prefix ? prefix->clone() : Ptr{};
        result->expression = expression ? expression->clone() : Ptr{};

        if (result->variable)
            result->children.push_back(result->variable);
        if (result->prefix)
            result->children.push_back(result->prefix);
        if (result->expression)
            result->children.push_back(result->expression);

        return result;
    }

    Ptr variable;
    Ptr prefix;
    /// `expression` is a single child of this node; its operands/factors stay nested under it.
    Ptr expression;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        if (variable)
        {
            variable->format(ostr, settings, state, frame);
            ostr << " = ";
        }

        if (prefix)
        {
            prefix->format(ostr, settings, state, frame);
            ostr << " ";
        }

        if (expression)
            expression->format(ostr, settings, state, frame);
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &variable);
        f(nullptr, &prefix);
        f(nullptr, &expression);
    }
};

}  // namespace DB::OPENGQL::AST
