#pragma once

#include <Parsers/graph/AST/Utils.h>

#include <vector>

namespace DB::OPENGQL::AST
{

class GQLSchemaReference final : public DB::IAST
{
public:
    enum class Kind : UInt8
    {
        AbsoluteRoot,
        AbsolutePath,
        RelativePath,
        Predefined,
        Parameter,
    };

    explicit GQLSchemaReference(Kind kind_, String name_ = {})
        : kind(kind_)
        , name(std::move(name_))
    {
    }

    Kind kind;
    size_t parent_levels = 0;
    std::vector<String> directory_parts;
    String name;

    String getID(char delim) const override
    {
        return "GQLSchemaReference" + (delim + name);
    }

    ASTPtr clone() const override
    {
        return make_intrusive<GQLSchemaReference>(*this);
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override
    {
        switch (kind)
        {
            case Kind::AbsoluteRoot:
                ostr << "/";
                return;

            case Kind::AbsolutePath:
                ostr << "/";

                for (const auto & part : directory_parts)
                    ostr << part << "/";

                ostr << name;
                return;

            case Kind::RelativePath:
                for (size_t i = 0; i < parent_levels; ++i)
                    ostr << "../";

                for (const auto & part : directory_parts)
                    ostr << part << "/";

                ostr << name;
                return;

            case Kind::Predefined:
            case Kind::Parameter:
                ostr << name;
                return;
        }
    }
};

}
