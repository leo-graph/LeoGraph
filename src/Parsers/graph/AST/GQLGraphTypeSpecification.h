#pragma once

#include <Parsers/graph/AST/Utils.h>

#include <vector>

namespace DB::OPENGQL::AST {

class GQLElementTypeSpecification final : public DB::IAST {
 public:
  enum class Kind : UInt8 {
    Node,
    Edge,
  };

  enum class Syntax : UInt8 {
    Pattern,
    Phrase,
  };

  explicit GQLElementTypeSpecification(Kind kind_) : kind(kind_) {}

  String getID(char delim) const override { return "GQLElementTypeSpecification" + (delim + type_name); }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLElementTypeSpecification>(*this);
    result->children.clear();
    result->properties = properties ? properties->clone() : Ptr{};
    result->source = source ? source->clone() : Ptr{};
    result->destination = destination ? destination->clone() : Ptr{};

    if (result->source) result->children.push_back(result->source);
    if (result->properties) result->children.push_back(result->properties);
    if (result->destination) result->children.push_back(result->destination);

    return result;
  }

  Kind kind;
  Syntax syntax = Syntax::Pattern;
  EdgeDirection direction = EdgeDirection::Right;
  String type_name;
  String alias;
  std::vector<String> labels;
  Ptr properties;
  Ptr source;
  Ptr destination;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    if (kind == Kind::Node) {
      formatNode(ostr, settings, state, frame);
      return;
    }

    formatEdge(ostr, settings, state, frame);
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &source);
    f(nullptr, &properties);
    f(nullptr, &destination);
  }

 private:
  void formatNode(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const {
    if (syntax == Syntax::Phrase) ostr << "NODE ";

    if (syntax == Syntax::Phrase && !type_name.empty()) ostr << type_name << " ";

    ostr << "(";
    if (!alias.empty()) ostr << alias;
    formatLabels(ostr);
    if (properties) {
      if (!alias.empty() || !labels.empty()) ostr << " ";
      properties->format(ostr, settings, state, frame);
    }
    ostr << ")";
  }

  void formatEdge(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const {
    if (source)
      source->format(ostr, settings, state, frame);
    else
      ostr << "()";

    formatEdgePrefix(ostr);
    if (!alias.empty()) ostr << alias;
    formatLabels(ostr);
    if (properties) {
      if (!alias.empty() || !labels.empty()) ostr << " ";
      properties->format(ostr, settings, state, frame);
    }
    formatEdgeSuffix(ostr);

    if (destination)
      destination->format(ostr, settings, state, frame);
    else
      ostr << "()";
  }

  void formatLabels(WriteBuffer &ostr) const {
    if (labels.empty()) return;

    ostr << ":";
    for (size_t i = 0; i < labels.size(); ++i) {
      if (i > 0) ostr << "&";
      ostr << labels[i];
    }
  }

  void formatEdgePrefix(WriteBuffer &ostr) const {
    switch (direction) {
      case EdgeDirection::Left:
        ostr << "<-[";
        break;
      case EdgeDirection::Undirected:
        ostr << "~[";
        break;
      default:
        ostr << "-[";
        break;
    }
  }

  void formatEdgeSuffix(WriteBuffer &ostr) const {
    switch (direction) {
      case EdgeDirection::Left:
        ostr << "]-";
        break;
      case EdgeDirection::Undirected:
        ostr << "]~";
        break;
      default:
        ostr << "]->";
        break;
    }
  }
};

class GQLGraphTypeSpecification final : public DB::IAST {
 public:
  GQLGraphTypeSpecification() = default;

  String getID(char) const override { return "GQLGraphTypeSpecification"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLGraphTypeSpecification>(*this);
    result->children.clear();

    for (const auto &child : children) {
      if (child) result->children.push_back(child->clone());
    }

    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "{";
    detail::formatChildren(ostr, settings, state, frame, children, ", ");
    ostr << "}";
  }
};

}  // namespace DB::OPENGQL::AST
