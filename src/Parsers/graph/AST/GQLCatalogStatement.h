#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLCatalogObjectName final : public DB::IAST
{
public:
    explicit GQLCatalogObjectName(String name_, String parent_text_ = {})
        : name(std::move(name_))
        , parent_text(std::move(parent_text_))
    {
    }

    String name;
    String parent_text;

    String getID(char delim) const override
    {
        return "GQLCatalogObjectName" + (delim + name);
    }

    ASTPtr clone() const override
    {
        return make_intrusive<GQLCatalogObjectName>(*this);
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override
    {
        if (!parent_text.empty())
            ostr << parent_text;
        ostr << name;
    }
};


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

    enum class SourceKind : UInt8
    {
        None,
        Any,
        LikeGraph,
        TypeReference,
        NestedSpec,
        CopyOfType,
    };

    explicit GQLCatalogStatement(Kind kind_) : kind(kind_) {}

    String getID(char) const override { return "GQLCatalogStatement"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLCatalogStatement>(*this);
        result->children.clear();
        result->name_reference = name_reference ? name_reference->clone() : Ptr{};
        result->source_reference = source_reference ? source_reference->clone() : Ptr{};
        if (result->name_reference)
            result->children.push_back(result->name_reference);
        if (result->source_reference)
            result->children.push_back(result->source_reference);
        return result;
    }

    Kind kind;
    bool if_exists = false;
    bool if_not_exists = false;
    bool or_replace = false;
    bool is_property = false;

    Ptr name_reference;

    SourceKind source_kind = SourceKind::None;
    Ptr source_reference;
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

        formatSource(ostr, settings, state, frame);
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &name_reference);
        f(nullptr, &source_reference);
    }

private:
    void formatSource(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const
    {
        switch (source_kind)
        {
            case SourceKind::None:
                break;
            case SourceKind::Any:
                ostr << " ANY";
                break;
            case SourceKind::LikeGraph:
                ostr << " LIKE ";
                if (source_reference)
                    source_reference->format(ostr, settings, state, frame);
                break;
            case SourceKind::TypeReference:
                ostr << " ";
                if (source_reference)
                    source_reference->format(ostr, settings, state, frame);
                break;
            case SourceKind::CopyOfType:
                ostr << " COPY OF ";
                if (source_reference)
                    source_reference->format(ostr, settings, state, frame);
                break;
            case SourceKind::NestedSpec:
                if (!source_text.empty())
                    ostr << " " << source_text;
                break;
        }

        if (source_kind != SourceKind::None && source_kind != SourceKind::NestedSpec && !source_text.empty())
            ostr << " " << source_text;
    }
};

}  // namespace DB::OPENGQL::AST
