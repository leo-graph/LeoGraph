#pragma once

#include <Parsers/IAST.h>

namespace DB
{

class ASTPathQuantifier : public IAST
{
public:
    static constexpr uint64_t UNLIMITED = UINT64_MAX;

    uint64_t min_hops = 0;
    uint64_t max_hops = UNLIMITED;

    String getID(char) const override { return "PathQuantifier"; }
    ASTPtr clone() const override;

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override;
};

}
