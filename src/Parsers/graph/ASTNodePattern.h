#pragma once

#include <Parsers/graph/ASTLabelExpression.h>

namespace DB
{

class ASTNodePattern : public IAST
{
public:
    String variable;
    String label;

    ASTLabelExpression * label_expression = nullptr;
    IAST * where_predicate = nullptr;

    void setLabelExpression(const ASTPtr & child);
    void setWherePredicate(const ASTPtr & child);

    String getID(char) const override { return "NodePattern"; }
    ASTPtr clone() const override;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override;
    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override;
};

}
