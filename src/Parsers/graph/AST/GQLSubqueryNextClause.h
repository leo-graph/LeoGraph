#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLSubqueryNextClause final : public DB::IAST
{
public:
    Ptr yield;
    Ptr statement;

    String getID(char) const override { return "GQLSubqueryNextClause"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLSubqueryNextClause>(*this);
        result->children.clear();
        result->yield = yield ? yield->clone() : Ptr{};
        result->statement = statement ? statement->clone() : Ptr{};

        if (result->yield)
            result->children.push_back(result->yield);

        if (result->statement)
            result->children.push_back(result->statement);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        ostr << "NEXT";

        if (yield)
        {
            ostr << " ";
            yield->format(ostr, settings, state, frame);
        }

        if (statement)
        {
            ostr << " ";
            statement->format(ostr, settings, state, frame);
        }
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &yield);
        f(nullptr, &statement);
    }
};

}
