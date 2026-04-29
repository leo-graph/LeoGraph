#pragma once

#include <Parsers/graph/AST/Utils.h>

#include <vector>

namespace DB::OPENGQL::AST
{

class GQLCatalogObjectName final : public DB::IAST
{
public:
    explicit GQLCatalogObjectName(String name_)
        : name(std::move(name_))
    {
    }

    String name;
    Ptr schema_ref;
    bool has_slash_after_schema = false;
    std::vector<String> parent_parts;

    String getID(char delim) const override
    {
        return "GQLCatalogObjectName" + (delim + name);
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLCatalogObjectName>(*this);
        result->children.clear();
        result->schema_ref = schema_ref ? schema_ref->clone() : Ptr{};
        if (result->schema_ref)
            result->children.push_back(result->schema_ref);
        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        if (schema_ref)
        {
            schema_ref->format(ostr, settings, state, frame);
            if (has_slash_after_schema)
                ostr << "/";
        }
        for (const auto & part : parent_parts)
            ostr << part << ".";
        ostr << name;
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &schema_ref);
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
        result->copy_source = copy_source ? copy_source->clone() : Ptr{};
        if (result->name_reference)
            result->children.push_back(result->name_reference);
        if (result->source_reference)
            result->children.push_back(result->source_reference);
        if (result->copy_source)
            result->children.push_back(result->copy_source);
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
    Ptr copy_source;

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
        f(nullptr, &copy_source);
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
                ostr << " ";
                if (source_reference)
                    source_reference->format(ostr, settings, state, frame);
                break;
        }

        if (copy_source)
        {
            ostr << " AS COPY OF ";
            copy_source->format(ostr, settings, state, frame);
        }
    }
};

}  // namespace DB::OPENGQL::AST
