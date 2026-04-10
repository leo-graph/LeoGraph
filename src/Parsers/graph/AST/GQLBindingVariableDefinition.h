#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLBindingVariableDefinition final : public DB::IAST
{
public:
    enum class Kind : UInt8
    {
        Graph,
        BindingTable,
        Value,
    };

    explicit GQLBindingVariableDefinition(Kind kind_)
        : kind(kind_)
    {
    }

    Kind kind;
    String name;
    Ptr initializer;
    String raw_type;
    bool property_keyword = false;
    bool binding_keyword = false;

    String getID(char delim) const override { return "GQLBindingVariableDefinition" + (delim + name); }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLBindingVariableDefinition>(*this);
        result->children.clear();
        result->initializer = initializer ? initializer->clone() : Ptr{};

        if (result->initializer)
            result->children.push_back(result->initializer);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        switch (kind)
        {
            case Kind::Graph:
                if (property_keyword)
                    ostr << "PROPERTY ";
                ostr << "GRAPH ";
                break;

            case Kind::BindingTable:
                if (binding_keyword)
                    ostr << "BINDING ";
                ostr << "TABLE ";
                break;

            case Kind::Value:
                ostr << "VALUE ";
                break;
        }

        ostr << name;

        if (!raw_type.empty())
            ostr << " " << raw_type;

        if (initializer)
        {
            ostr << " ";
            initializer->format(ostr, settings, state, frame);
        }
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &initializer);
    }
};

}
