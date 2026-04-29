#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLSimplifiedPathExpr final : public DB::IAST
{
public:
    enum class Kind : UInt8
    {
        Label,
        Group,
        Concatenation,
        Conjunction,
        Union,
        MultisetAlternation,
        Negation,
        DirectionOverride,
        Repetition,
    };

    explicit GQLSimplifiedPathExpr(Kind kind_, String text_ = {})
        : kind(kind_), text(std::move(text_))
    {
    }

    static Ptr label(const String & text)
    {
        return Ptr(make_intrusive<GQLSimplifiedPathExpr>(Kind::Label, text));
    }

    static Ptr group(Ptr operand_)
    {
        auto expression = make_intrusive<GQLSimplifiedPathExpr>(Kind::Group);
        expression->operand = std::move(operand_);
        if (expression->operand)
            expression->children.push_back(expression->operand);
        return Ptr(expression);
    }

    static Ptr negation(Ptr operand_)
    {
        auto expression = make_intrusive<GQLSimplifiedPathExpr>(Kind::Negation);
        expression->operand = std::move(operand_);
        if (expression->operand)
            expression->children.push_back(expression->operand);
        return Ptr(expression);
    }

    static Ptr directionOverride(EdgeDirection direction_, Ptr operand_)
    {
        auto expression = make_intrusive<GQLSimplifiedPathExpr>(Kind::DirectionOverride);
        expression->direction = direction_;
        expression->operand = std::move(operand_);
        if (expression->operand)
            expression->children.push_back(expression->operand);
        return Ptr(expression);
    }

    static Ptr repetition(Ptr operand_, Ptr quantifier_)
    {
        auto expression = make_intrusive<GQLSimplifiedPathExpr>(Kind::Repetition);
        expression->operand = std::move(operand_);
        expression->quantifier = std::move(quantifier_);

        if (expression->operand)
            expression->children.push_back(expression->operand);
        if (expression->quantifier)
            expression->children.push_back(expression->quantifier);

        return Ptr(expression);
    }

    String getID(char) const override
    {
        return "GQLSimplifiedPathExpr";
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLSimplifiedPathExpr>(*this);
        result->children.clear();
        result->operands.clear();
        result->operand = operand ? operand->clone() : Ptr{};
        result->quantifier = quantifier ? quantifier->clone() : Ptr{};

        if (result->operand)
            result->children.push_back(result->operand);
        if (result->quantifier)
            result->children.push_back(result->quantifier);

        detail::cloneChildrenList(operands, result->operands, result->children);
        return result;
    }

    Kind kind;
    String text;
    EdgeDirection direction = EdgeDirection::Any;
    Ptr operand;
    Ptr quantifier;
    PtrList operands;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        switch (kind)
        {
            case Kind::Label:
                ostr << text;
                return;
            case Kind::Group:
                ostr << "(";
                if (operand)
                    operand->format(ostr, settings, state, frame);
                ostr << ")";
                return;
            case Kind::Concatenation:
                detail::formatChildren(ostr, settings, state, frame, operands, " ");
                return;
            case Kind::Conjunction:
                detail::formatChildren(ostr, settings, state, frame, operands, " & ");
                return;
            case Kind::Union:
                detail::formatChildren(ostr, settings, state, frame, operands, " | ");
                return;
            case Kind::MultisetAlternation:
                detail::formatChildren(ostr, settings, state, frame, operands, " |+| ");
                return;
            case Kind::Negation:
                ostr << "!";
                if (operand)
                    operand->format(ostr, settings, state, frame);
                return;
            case Kind::DirectionOverride:
                detail::formatSimplifiedDirectionOverridePrefix(ostr, direction);
                if (operand)
                    operand->format(ostr, settings, state, frame);
                detail::formatSimplifiedDirectionOverrideSuffix(ostr, direction);
                return;
            case Kind::Repetition:
                if (operand)
                    operand->format(ostr, settings, state, frame);
                if (quantifier)
                    quantifier->format(ostr, settings, state, frame);
                return;
        }
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        f(nullptr, &operand);
        f(nullptr, &quantifier);

        for (auto & child : operands)
            f(nullptr, &child);
    }
};

}
