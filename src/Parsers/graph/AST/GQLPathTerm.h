#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST
{

class GQLPathTerm final : public DB::IAST
{
public:
    GQLPathTerm() = default;

    explicit GQLPathTerm(PtrList factors_)
        : factors(std::move(factors_))
    {
        chassert(!factors.empty());

        for (const auto & factor : factors)
        {
            if (factor)
                children.push_back(factor);
        }
    }

    String getID(char) const override
    {
        return "GQLPathTerm";
    }

    ASTPtr clone() const override
    {
        auto result = make_intrusive<GQLPathTerm>(*this);
        result->children.clear();
        detail::cloneChildrenList(factors, result->factors, result->children);
        chassert(!result->factors.empty());
        return result;
    }

    PtrList factors;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override
    {
        /// Path factors already carry their own boundary tokens, so no extra separator is needed here.
        detail::formatChildren(ostr, settings, state, frame, factors, "");
    }

    void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override
    {
        for (auto & factor : factors)
            f(nullptr, &factor);
    }
};

}
