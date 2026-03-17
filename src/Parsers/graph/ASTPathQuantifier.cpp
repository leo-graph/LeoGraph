#include <Parsers/graph/ASTPathQuantifier.h>

#include <IO/Operators.h>

namespace DB
{

ASTPtr ASTPathQuantifier::clone() const
{
    return make_intrusive<ASTPathQuantifier>(*this);
}

void ASTPathQuantifier::formatImpl(WriteBuffer & ostr, const FormatSettings &, FormatState &, FormatStateStacked) const
{
    if (min_hops == 0 && max_hops == UNLIMITED)
        ostr << "*";
    else if (min_hops == 1 && max_hops == UNLIMITED)
        ostr << "+";
    else if (min_hops == 0 && max_hops == 1)
        ostr << "?";
    else if (min_hops == max_hops)
        ostr << "{" << min_hops << "}";
    else if (max_hops == UNLIMITED)
        ostr << "{" << min_hops << ",}";
    else
        ostr << "{" << min_hops << "," << max_hops << "}";
}

}
