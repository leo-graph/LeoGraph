#pragma once

#include <cstdint>

namespace DB {

enum class GraphEdgeDirection : uint8_t {
  RIGHT,
  LEFT,
  UNDIRECTED,
  LEFT_OR_UNDIRECTED,
  UNDIRECTED_OR_RIGHT,
  LEFT_OR_RIGHT,
  ANY,
};

enum class GraphPathMode : uint8_t {
  DEFAULT,
  WALK,
  TRAIL,
  SIMPLE,
  ACYCLIC,
};

enum class GraphPathSearch : uint8_t {
  NONE,
  ALL,
  ANY,
  SHORTEST,
  ALL_SHORTEST,
  ANY_SHORTEST,
  COUNTED_SHORTEST,
  COUNTED_SHORTEST_GROUP,
};

enum class GraphLabelOp : uint8_t {
  NAME,
  CONJUNCTION,
  DISJUNCTION,
  NEGATION,
  WILDCARD,
};

}  // namespace DB
