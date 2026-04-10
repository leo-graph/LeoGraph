#pragma once

#include <IO/Operators.h>
#include <IO/WriteBufferFromString.h>
#include <Parsers/graph/fwd_decl.h>
#include <Parsers/IAST.h>

namespace DB::OPENGQL::AST::detail {

inline void formatChildren(WriteBuffer& ostr, const IAST::FormatSettings& settings, IAST::FormatState& state,
                           IAST::FormatStateStacked frame, const PtrList& children, std::string_view separator) {
  bool first = true;

  for (const auto& child : children) {
    if (!child) continue;

    if (!first) ostr << separator;

    child->format(ostr, settings, state, frame);
    first = false;
  }
}

inline void cloneChildrenList(const PtrList& source, PtrList& destination, PtrList& owned_children) {
  destination.clear();

  for (const auto& child : source) {
    auto clone = child ? child->clone() : Ptr{};
    destination.push_back(clone);

    if (clone) owned_children.push_back(clone);
  }
}

inline String formatNodeToString(const IAST& node) {
  WriteBufferFromOwnString buffer;
  IAST::FormatSettings settings(true);
  IAST::FormatState state;
  node.format(buffer, settings, state, {});
  return buffer.str();
}

inline const char* getSetOperationKeyword(SetOperation operation) {
  switch (operation) {
    case SetOperation::Union:
      return "UNION";
    case SetOperation::Except:
      return "EXCEPT";
    case SetOperation::Intersect:
      return "INTERSECT";
    case SetOperation::Otherwise:
      return "OTHERWISE";
  }

  return "UNION";
}

inline void formatEdgePrefix(WriteBuffer& ostr, EdgeDirection direction) {
  switch (direction) {
    case EdgeDirection::Left:
      ostr << "<-[";
      break;
    case EdgeDirection::Right:
      ostr << "-[";
      break;
    case EdgeDirection::Undirected:
      ostr << "~[";
      break;
    case EdgeDirection::LeftOrRight:
      ostr << "<-[";
      break;
    case EdgeDirection::LeftOrUndirected:
      ostr << "<~[";
      break;
    case EdgeDirection::UndirectedOrRight:
      ostr << "~[";
      break;
    case EdgeDirection::Any:
      ostr << "-[";
      break;
  }
}

inline void formatEdgeSuffix(WriteBuffer& ostr, EdgeDirection direction) {
  switch (direction) {
    case EdgeDirection::Left:
      ostr << "-";
      break;
    case EdgeDirection::Right:
      ostr << "->";
      break;
    case EdgeDirection::Undirected:
      ostr << "~";
      break;
    case EdgeDirection::LeftOrRight:
      ostr << "->";
      break;
    case EdgeDirection::LeftOrUndirected:
      ostr << "~";
      break;
    case EdgeDirection::UndirectedOrRight:
      ostr << "~>";
      break;
    case EdgeDirection::Any:
      ostr << "-";
      break;
  }
}

}  // namespace DB::OPENGQL::AST::detail
