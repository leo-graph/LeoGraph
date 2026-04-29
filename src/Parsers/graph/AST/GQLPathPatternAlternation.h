#pragma once

#include <Parsers/graph/AST/GQLPathTerm.h>

namespace DB::OPENGQL::AST
{

class GQLPathPatternAlternation final : public DB::IAST
{
public:
    enum class Kind : UInt8
    {
        Union,
        MultisetAlternation,
    };

    explicit GQLPathPatternAlternation(Kind kind_, PtrList operands_ = {})
        : kind(kind_), operands(std::move(operands_))
    {
        validateOperands(operands);

        for (const auto & operand : operands)
        {
            if (operand)
                children.push_back(operand);
        }
    }

    String getID(char) const override
    {
        return "GQLPathPatternAlternation";
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLPathPatternAlternation>(*this);
        result->children.clear();
        detail::cloneChildrenList(operands, result->operands, result->children);
        validateOperands(result->operands);
        return result;
    }

    Kind kind;
    PtrList operands;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        switch (kind)
        {
            case Kind::Union:
                detail::formatChildren(ostr, settings, state, frame, operands, " | ");
                return;
            case Kind::MultisetAlternation:
                detail::formatChildren(ostr, settings, state, frame, operands, " |+| ");
                return;
        }
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        for (auto & operand : operands)
            f(nullptr, &operand);
    }

private:
    static void validateOperands(const PtrList & operands_)
    {
        chassert(operands_.size() >= 2);

        for (const auto & operand : operands_)
        {
            chassert(operand);
            chassert(operand->as<GQLPathTerm>());
        }
    }
};

}
