#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLCatalogStatement final : public DB::IAST
{
public:
    enum class Kind : UInt8
    {
        CreateSchema,
        DropSchema,
        CreateGraph,
        DropGraph,
        CreateGraphType,
        DropGraphType,
    };

    explicit GQLCatalogStatement(Kind kind_) : kind(kind_) {}

    String getID(char) const override { return "GQLCatalogStatement"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLCatalogStatement>(*this);
        result->children.clear();
        result->name_reference = name_reference ? name_reference->clone() : Ptr{};
        if (result->name_reference)
            result->children.push_back(result->name_reference);
        return result;
    }

    Kind kind;
    bool if_exists = false;
    bool if_not_exists = false;
    bool or_replace = false;
    bool is_property = false;

    Ptr name_reference;

    String source_text;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        bool is_create = (kind == Kind::CreateSchema || kind == Kind::CreateGraph || kind == Kind::CreateGraphType);

        if (is_create)
        {
            ostr << "CREATE ";
            if (or_replace)
                ostr << "OR REPLACE ";
        }
        else
        {
            ostr << "DROP ";
        }

        if (is_property)
            ostr << "PROPERTY ";

        switch (kind)
        {
            case Kind::CreateSchema:
            case Kind::DropSchema:
                ostr << "SCHEMA ";
                break;
            case Kind::CreateGraph:
            case Kind::DropGraph:
                ostr << "GRAPH ";
                break;
            case Kind::CreateGraphType:
            case Kind::DropGraphType:
                ostr << "GRAPH TYPE ";
                break;
        }

        if (is_create && if_not_exists)
            ostr << "IF NOT EXISTS ";
        if (!is_create && if_exists)
            ostr << "IF EXISTS ";

        if (name_reference)
            name_reference->format(ostr, settings, state, frame);

        if (!source_text.empty())
            ostr << " " << source_text;
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &name_reference);
    }
};

}  // namespace DB::OPENGQL::AST
