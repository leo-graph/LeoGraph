#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLPageClause final : public DB::IAST
{
public:
    Ptr order_by;
    Ptr offset;
    Ptr limit;

    String getID(char) const override { return "GQLPageClause"; }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLPageClause>(*this);
        result->children.clear();
        result->order_by = order_by ? order_by->clone() : Ptr{};
        result->offset = offset ? offset->clone() : Ptr{};
        result->limit = limit ? limit->clone() : Ptr{};

        if (result->order_by)
            result->children.push_back(result->order_by);

        if (result->offset)
            result->children.push_back(result->offset);

        if (result->limit)
            result->children.push_back(result->limit);

        return result;
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        bool needs_space = false;

        if (order_by)
        {
            order_by->format(ostr, settings, state, frame);
            needs_space = true;
        }

        if (offset)
        {
            if (needs_space)
                ostr << " ";

            ostr << "OFFSET ";
            offset->format(ostr, settings, state, frame);
            needs_space = true;
        }

        if (limit)
        {
            if (needs_space)
                ostr << " ";

            ostr << "LIMIT ";
            limit->format(ostr, settings, state, frame);
        }
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &order_by);
        f(nullptr, &offset);
        f(nullptr, &limit);
    }
};

}
