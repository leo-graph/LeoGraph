#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLSubqueryClause final : public DB::IAST
{
public:
    Ptr at_schema;
    PtrList bindings;
    Ptr statement;
    PtrList next_statements;

    String getID(char) const override { return "GQLSubqueryClause"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLSubqueryClause>(*this);
        result->children.clear();
        result->at_schema = at_schema ? at_schema->clone() : Ptr{};
        result->statement = statement ? statement->clone() : Ptr{};

        if (result->at_schema)
            result->children.push_back(result->at_schema);

        detail::cloneChildrenList(bindings, result->bindings, result->children);

        if (result->statement)
            result->children.push_back(result->statement);

        detail::cloneChildrenList(next_statements, result->next_statements, result->children);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        ostr << "{";

        bool needs_space = false;
        auto format_child = [&](const Ptr & child)
        {
            if (!child)
                return;

            ostr << (needs_space ? " " : " ");
            child->format(ostr, settings, state, frame);
            needs_space = true;
        };

        format_child(at_schema);

        for (const auto & binding : bindings)
            format_child(binding);

        format_child(statement);

        for (const auto & next_statement : next_statements)
            format_child(next_statement);

        if (needs_space)
            ostr << " ";

        ostr << "}";
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &at_schema);

        for (auto & binding : bindings)
            f(nullptr, &binding);

        f(nullptr, &statement);

        for (auto & next_statement : next_statements)
            f(nullptr, &next_statement);
    }
};

}
