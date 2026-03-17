#pragma once

#include <Parsers/IAST.h>

namespace DB
{

class ASTGraphReturnClause : public IAST
{
public:
    bool distinct = false;

    IAST * group_by = nullptr;
    IAST * order_by = nullptr;
    IAST * limit = nullptr;
    IAST * offset = nullptr;

    void addItem(const ASTPtr & child)
    {
        if (child)
            children.push_back(child);
    }

    void setGroupBy(const ASTPtr & child);
    void setOrderBy(const ASTPtr & child);
    void setLimit(const ASTPtr & child);
    void setOffset(const ASTPtr & child);

    String getID(char) const override { return "GraphReturnClause"; }
    ASTPtr clone() const override;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override;
    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override;
};

}
