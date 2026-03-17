#include <Parsers/graph/ASTGraphReturnClause.h>

#include <IO/Operators.h>

namespace DB
{

void ASTGraphReturnClause::setGroupBy(const ASTPtr & child)
{
    if (!child)
        return;

    setOrReplace(group_by, child);
}

void ASTGraphReturnClause::setOrderBy(const ASTPtr & child)
{
    if (!child)
        return;

    setOrReplace(order_by, child);
}

void ASTGraphReturnClause::setLimit(const ASTPtr & child)
{
    if (!child)
        return;

    setOrReplace(limit, child);
}

void ASTGraphReturnClause::setOffset(const ASTPtr & child)
{
    if (!child)
        return;

    setOrReplace(offset, child);
}

ASTPtr ASTGraphReturnClause::clone() const
{
    auto res = make_intrusive<ASTGraphReturnClause>(*this);
    res->children.clear();
    res->group_by = nullptr;
    res->order_by = nullptr;
    res->limit = nullptr;
    res->offset = nullptr;

    for (const auto & child : children)
    {
        auto cloned = child->clone();
        if (group_by && child.get() == group_by)
            res->setGroupBy(cloned);
        else if (order_by && child.get() == order_by)
            res->setOrderBy(cloned);
        else if (limit && child.get() == limit)
            res->setLimit(cloned);
        else if (offset && child.get() == offset)
            res->setOffset(cloned);
        else
            res->addItem(cloned);
    }

    return res;
}

void ASTGraphReturnClause::formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const
{
    ostr << "RETURN ";

    if (distinct)
        ostr << "DISTINCT ";

    bool first = true;
    for (const auto & child : children)
    {
        if (child.get() == group_by || child.get() == order_by || child.get() == limit || child.get() == offset)
            continue;

        if (!first)
            ostr << ", ";
        first = false;
        child->format(ostr, settings, state, frame);
    }

    if (group_by)
    {
        ostr << " GROUP BY ";
        group_by->format(ostr, settings, state, frame);
    }

    if (order_by)
    {
        ostr << " ORDER BY ";
        order_by->format(ostr, settings, state, frame);
    }

    if (offset)
    {
        ostr << " OFFSET ";
        offset->format(ostr, settings, state, frame);
    }

    if (limit)
    {
        ostr << " LIMIT ";
        limit->format(ostr, settings, state, frame);
    }
}

void ASTGraphReturnClause::forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f)
{
    f(&group_by, nullptr);
    f(&order_by, nullptr);
    f(&limit, nullptr);
    f(&offset, nullptr);
}

}
