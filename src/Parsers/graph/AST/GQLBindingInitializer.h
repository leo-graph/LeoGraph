#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLBindingInitializer final : public DB::IAST
{
public:
    enum class Kind : UInt8
    {
        Graph,
        BindingTable,
        Value,
    };

    explicit GQLBindingInitializer(Kind kind_, Ptr value_ = {})
        : kind(kind_)
        , value(std::move(value_))
    {
        if (value)
            children.push_back(value);
    }

    Kind kind;
    Ptr value;

    String getID(char) const override
    {
        return "GQLBindingInitializer";
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLBindingInitializer>(*this);
        result->children.clear();
        result->value = value ? value->clone() : Ptr{};

        if (result->value)
            result->children.push_back(result->value);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        ostr << "= ";

        if (value)
            value->format(ostr, settings, state, frame);
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &value);
    }
};

}
