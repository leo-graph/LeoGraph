#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLBindingTableExpression final : public DB::IAST
{
public:
    enum class Kind : UInt8
    {
        NestedQuery,
        BindingTableReference,
        ObjectExpression,
        ObjectNameOrBindingVariable,
    };

    explicit GQLBindingTableExpression(Kind kind_, String text_ = {}, Ptr value_ = {})
        : kind(kind_)
        , text(std::move(text_))
        , value(std::move(value_))
    {
        if (value)
            children.push_back(value);
    }

    Kind kind;
    String text;
    Ptr value;

    String getID(char delim) const override
    {
        return "GQLBindingTableExpression" + (delim + text);
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLBindingTableExpression>(*this);
        result->children.clear();
        result->value = value ? value->clone() : Ptr{};

        if (result->value)
            result->children.push_back(result->value);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        if (value)
        {
            value->format(ostr, settings, state, frame);
            return;
        }

        ostr << text;
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &value);
    }
};

}
