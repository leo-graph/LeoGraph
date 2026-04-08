#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLAtSchemaClause final : public DB::IAST
{
public:
    explicit GQLAtSchemaClause(Ptr schema_reference_ = {})
        : schema_reference(std::move(schema_reference_))
    {
        if (schema_reference)
            children.push_back(schema_reference);
    }

    Ptr schema_reference;

    String getID(char) const override { return "GQLAtSchemaClause"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLAtSchemaClause>(*this);
        result->children.clear();
        result->schema_reference = schema_reference ? schema_reference->clone() : Ptr{};

        if (result->schema_reference)
            result->children.push_back(result->schema_reference);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        ostr << "AT ";

        if (schema_reference)
            schema_reference->format(ostr, settings, state, frame);
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &schema_reference);
    }
};

}
