#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLDeleteClause final : public DB::IAST {
 public:
  enum class DetachMode : UInt8 {
    None,
    Detach,
    NoDetach,
  };

  explicit GQLDeleteClause(DetachMode detach_mode_ = DetachMode::None) : detach_mode(detach_mode_) {}

  String getID(char) const override { return "GQLDeleteClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLDeleteClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    return result;
  }

  DetachMode detach_mode;
  PtrList items;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    switch (detach_mode) {
      case DetachMode::Detach:
        ostr << "DETACH ";
        break;
      case DetachMode::NoDetach:
        ostr << "NODETACH ";
        break;
      case DetachMode::None:
        break;
    }

    ostr << "DELETE ";
    detail::formatChildren(ostr, settings, state, frame, items, ", ");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);
  }
};

}  // namespace DB::OPENGQL::AST
