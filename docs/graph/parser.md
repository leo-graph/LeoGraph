---
description: 'GQL Parser design: ANTLR4 integration and Graph AST'
sidebar_label: 'GQL Parser'
sidebar_position: 82
slug: /development/graph/parser
title: 'GQL Parser Design'
doc_type: 'reference'
---

# GQL Parser Design

This document describes how the GQL (Graph Query Language, ISO/IEC 39075) parser is integrated into ClickHouse using ANTLR4.

## ANTLR4 Integration

### Existing Precedent: PromQL

ClickHouse already uses ANTLR4 for parsing PromQL (Prometheus Query Language):

```
contrib/antlr4-cpp-runtime/         -- ANTLR4 C++ runtime library
contrib/antlr4-cpp-runtime-cmake/   -- CMake build for the runtime
contrib/antlr4-grammars/            -- PromQL grammar (PromQL.g4)
contrib/antlr4-grammars-cmake/      -- CMake that generates C++ from .g4

src/Parsers/Prometheus/
  PrometheusQueryParsingUtil-antlr.cpp  -- ANTLR ParseTree -> ClickHouse AST
```

The GQL parser follows the exact same pattern.

### Grammar Source

The GQL grammar is sourced from the [opengql/grammar](https://github.com/opengql/grammar) repository, which provides a language-independent ANTLR grammar conforming to ISO GQL.

Key characteristics of the grammar:
- **Case-insensitive** keywords (`options { caseInsensitive = true; }`)
- **Combined** lexer and parser rules in a single `GQL.g4` file
- Covers the full GQL standard: `MATCH`, `RETURN`, `WHERE`, `INSERT`, `DELETE`, DDL, session management, transactions

### Build Pipeline

```
GQL.g4  ──(ANTLR4 tool)──>  GQLLexer.cpp / GQLParser.cpp / GQLVisitor.cpp
                                    |
                              (compiled into)
                                    |
                            ch_contrib::gql_grammar
                                    |
                              (linked by)
                                    |
                          src/Parsers/Graph/  (GQLParsingUtil.cpp)
```

CMake configuration:

```cmake
# contrib/gql-grammar-cmake/CMakeLists.txt
set(GQL_GRAMMAR_DIR "${ClickHouse_SOURCE_DIR}/contrib/gql-grammar")
set(GQL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_custom_command(
    OUTPUT
        ${GQL_GENERATED_DIR}/GQLLexer.cpp
        ${GQL_GENERATED_DIR}/GQLParser.cpp
        ${GQL_GENERATED_DIR}/GQLBaseVisitor.cpp
    COMMAND ${ANTLR4_EXECUTABLE} -Dlanguage=Cpp -visitor -no-listener
            -o ${GQL_GENERATED_DIR}
            ${GQL_GRAMMAR_DIR}/GQL.g4
    DEPENDS ${GQL_GRAMMAR_DIR}/GQL.g4
)

add_library(ch_contrib::gql_grammar STATIC
    ${GQL_GENERATED_DIR}/GQLLexer.cpp
    ${GQL_GENERATED_DIR}/GQLParser.cpp
    ${GQL_GENERATED_DIR}/GQLBaseVisitor.cpp
)
target_link_libraries(ch_contrib::gql_grammar PUBLIC ch_contrib::antlr4_runtime)
```

## GQL Subset (Phase 1)

The initial implementation supports a core subset of GQL:

### Supported Statements

| Statement | Example | Priority |
|-----------|---------|----------|
| `MATCH ... RETURN` | `MATCH (a:Person) RETURN a.name` | P0 |
| `MATCH ... WHERE ... RETURN` | `MATCH (a)-[e]->(b) WHERE a.age > 25 RETURN b` | P0 |
| `CREATE PROPERTY GRAPH` | `CREATE PROPERTY GRAPH g VERTEX TABLES (...) EDGE TABLES (...)` | P1 |
| `DROP PROPERTY GRAPH` | `DROP PROPERTY GRAPH g` | P1 |
| `MATCH` with multi-hop | `MATCH (a)-[:E*1..5]->(b) RETURN b` | P3 |

### Supported Patterns

| Pattern | Syntax | Description |
|---------|--------|-------------|
| Node | `(a:Label)` | Vertex with optional variable and label |
| Directed edge | `-[e:Label]->` | Outgoing edge |
| Reverse edge | `<-[e:Label]-` | Incoming edge |
| Undirected edge | `~[e:Label]~` | Undirected edge |
| Multi-hop | `-[:Label*1..N]->` | Variable-length path |
| Property filter | `(a {name: 'Alice'})` | Inline property predicate |

### Deferred Features

- `INSERT` / `DELETE` / `SET` / `REMOVE` (graph DML)
- `USE graphExpression` (multi-graph)
- `SESSION` / `START TRANSACTION` (session management)
- `CALL` procedure
- Path modes (`WALK`, `TRAIL`, `SIMPLE`, `ACYCLIC`)
- `SHORTEST` path queries
- `OPTIONAL MATCH`

## Graph AST Design

The current refactor keeps the `antlr4` side and the `IAST` side intentionally separate:

- The `visit*` boundaries in `GQLParseTreeVisitor` follow `GQL.g4`.
- The `IAST` layer is normalized in a `kgraph`-style shape, instead of mirroring every parse-tree wrapper rule.
- Pure grammar pass-through nodes such as `compositeQueryStatement` are not preserved as dedicated AST wrappers.

This keeps the visitor easy to debug against the grammar while still producing a stable AST contract for later lowering.

### Query Root Contract

The current query-level contract is:

| Grammar rule | Returned AST node | Notes |
|--------------|-------------------|-------|
| `compositeQueryStatement` | forwards child root | no dedicated wrapper |
| `compositeQueryExpression` with a set operator | `GQLCombinedQuery` | stores `queries + operators`, preserving `ALL` / `DISTINCT` explicitly |
| `compositeQueryExpression` without a set operator | forwards child root | keeps the root stable |
| linear clause query | `GQLSingleQuery` | ordered list of clause nodes |
| top-level `selectStatement` | `GQLSingleQuery` | starts with `GQLSelectClause`, followed by `GQLPageClause` when paging is present |
| `nestedQuerySpecification` | `GQLSubquery` | preserves the wrapper; the inner `query` child is itself a normalized query root |

This is the main design rule that aligns the ClickHouse visitor with `kgraph`:

- grammar decides which `visit*` methods exist;
- AST normalization decides which nodes are allowed to become public query roots.

### Main AST Families

At the current phase, the most important `GQL*` nodes are:

```text
IAST
  |
  +-- GQLSingleQuery          -- ordered clause list for linear queries
  +-- GQLCombinedQuery        -- `UNION` / `EXCEPT` / `INTERSECT` / `OTHERWISE`, with explicit operator variants
  +-- GQLSubquery             -- explicit `{ ... }` wrapper with `NEXT` chain support
  |
  +-- GQLAtSchemaClause       -- `AT schemaReference`
  +-- GQLSchemaReference      -- typed `schemaReference` wrapper
  +-- GQLGraphExpression      -- typed wrapper for `graphExpression`
  +-- GQLBindingTableExpression -- typed wrapper for `bindingTableExpression`
  +-- GQLBindingInitializer   -- typed wrapper for `= ...` in binding definitions
  +-- GQLBindingVariableDefinitionBlock
  +-- GQLBindingVariableDefinition
  +-- GQLYieldClause
  |
  +-- GQLMatchClause          -- `MATCH` / `OPTIONAL MATCH`
  +-- GQLGraphPatternBlock    -- delimited graph-pattern body for predicates
  +-- GQLMatchStatementBlock  -- delimited `MATCH`-statement block wrapper
  +-- GQLReturnClause         -- `RETURN`
  +-- GQLSelectClause         -- `SELECT`
  +-- GQLWhereClause          -- `WHERE`, `FILTER`, `HAVING`
  +-- GQLUseClause            -- `USE`
  +-- GQLCallNamedClause      -- named `CALL foo(...) YIELD ...`
  +-- GQLCallVariableScopeClause -- optional `(x, y)` scope for inline `CALL`
  +-- GQLCallInlineClause     -- inline `CALL { ... }` or `CALL (x, y) { ... }`
  +-- GQLGroupByClause        -- structured `GROUP BY`
  +-- GQLLetClause            -- `LET`
  +-- GQLForClause            -- `FOR`
  +-- GQLPageClause           -- standalone `ORDER BY` / `OFFSET` / `LIMIT`
  |
  +-- GQLPathPattern
  +-- GQLPathPatternPrefix
  +-- GQLParenthesizedPathPattern
  +-- GQLSimplifiedPathPattern
  +-- GQLSimplifiedPathExpr
  +-- GQLNodePattern
  +-- GQLEdgePattern
  +-- GQLLabelExpression
  +-- GQLQuantifier
  +-- GQLListConstructor
  +-- GQLRecordConstructor
  +-- GQLExpr                 -- current expression skeleton
```

Pattern nodes stay graph-native, while expressions still use a thin `GQLExpr` layer or local raw-text fallbacks where the refactor is not complete yet. Inside `GQLSubquery`, wrapper-level metadata such as `AT schema`, binding definitions, and `NEXT YIELD` now also have dedicated AST nodes instead of being stored as top-level raw-text leaves. `schemaReference`, `graphExpression`, `bindingTableExpression`, and binding initializers are also wrapped in dedicated nodes so the nested procedure-body contract can grow without reshaping its parents.

### Visitor Organization

The visitor is organized in the same high-level layers as the grammar:

- query-level visitors such as `visitCompositeQueryExpression`, `visitLinearQueryStatement`, and `visitSelectStatement` decide the normalized query root;
- clause-level visitors such as `visitMatchStatement`, `visitCallQueryStatement`, `visitFilterStatement`, and `visitReturnStatement` build individual clause nodes;
- pattern-level visitors such as `visitGraphPattern`, `visitPathPattern`, `visitNodePattern`, and `visitEdgePattern` build graph-specific subtrees;
- expression-level visitors build `GQLExpr` and related leaf nodes.

Two rules are especially important:

- `visitSelectStatement` must finish reading the needed parse-tree branches before it constructs `GQLSelectClause`, then emit a `GQLSingleQuery` and append `GQLPageClause` only when paging exists.
- `visitNestedQuerySpecification` must preserve a `GQLSubquery` wrapper instead of unwrapping its inner query.
- `visitCallQueryStatement` should preserve named and inline procedure calls as different AST node types instead of folding them back into one boolean-driven clause.
- inline `CALL` should preserve `variableScopeClause` structurally when present instead of rejecting it.
- `visitGroupByClause` should build a dedicated `GQLGroupByClause` instead of storing `GROUP BY ...` as raw text.
- `visitUseGraphClause` and graph-qualified `SELECT FROM` source builders should preserve `graphExpression` as `GQLGraphExpression` instead of a top-level raw-text expression.
- binding-variable initializers should preserve `bindingTableExpression` as `GQLBindingTableExpression` unless the grammar branch is still explicitly unsupported.
- `visitGraphPattern` and `visitPathPattern` should preserve graph/path prefixes structurally instead of degrading them to raw text or `Unsupported`.
- `visitPpSimplifiedPathPatternExpression` should keep simplified path forms in dedicated `GQLSimplifiedPathPattern` / `GQLSimplifiedPathExpr` nodes instead of forcing them into `GQLEdgePattern`. `GQLSimplifiedPathExpr` round-trip preserves the AST shape but normalizes whitespace around binary operators such as `|`, `|+|`, `&`, and concatenation; tests should assert shape plus the normalized form rather than the raw source string.
- `visitPredicateExprAlt` should keep `EXISTS` operands as structured AST children (`GQLGraphPatternBlock`, `GQLMatchStatementBlock`, or `GQLSubquery`) instead of stringifying the body.
- If a nested procedure body contains a non-query statement that the current query AST cannot represent, the visitor should fail explicitly instead of hiding it inside a top-level raw-text query node.

These two constraints keep the query contract stable while the lower layers continue to expand.

## Parser Pipeline

### Current `antlr4` Entry

The current `antlr4` entry is `GQLParserUtils::parseCompositeQueryStatement`.

```cpp
ASTPtr parseQuery(std::string_view query)
{
    auto * parse_tree = OPENGQL::GQLParserUtils::parseCompositeQueryStatement(query);
    OPENGQL::GQLParseTreeVisitor visitor;
    return std::any_cast<ASTPtr>(visitor.visit(parse_tree));
}
```

This means the internal refactor currently centers on complete query statements, not on isolated clause fragments.

### Top-Level ClickHouse Parser Entry

`ParserGraphQuery` is still a thin outer adapter. Today it only routes a subset of top-level prefixes into the `antlr4` path, and that gating is intentionally treated as a later integration task.

The current parser work therefore focuses on making:

`GQL text -> antlr4 parse tree -> normalized GQL IAST`

as complete and stable as possible before widening the top-level routing rules in `ParserQuery`.

## Error Handling

`antlr4` syntax and lexer errors are translated into ClickHouse exceptions with existing error codes such as `SYNTAX_ERROR`.

The current rule of thumb is:

- use `GQLParserUtils` for `SLL -> LL` fallback and listener wiring;
- use existing ClickHouse error codes;
- keep feature gaps explicit with clear `Unsupported GQL ...` exceptions while the AST layer is still expanding.

## Testing Strategy

The useful tests at this phase are shape tests for the normalized AST contract:

1. query-root tests: verify whether a query returns `GQLSingleQuery`, `GQLCombinedQuery`, or `GQLSubquery`;
2. clause-order tests: verify that linear queries preserve clause order inside `GQLSingleQuery`;
3. wrapper tests: verify that top-level `SELECT` normalizes to `GQLSelectClause` plus optional `GQLPageClause`, and `{ ... }` keeps a `GQLSubquery` wrapper;
4. path tests: verify that parenthesized and simplified path primaries keep their own AST families, and that outer `?` / `{m,n}` quantifiers attach to the path-primary wrapper instead of being folded into an edge node;
4. pattern and expression tests: verify the current structured coverage and make raw-text fallbacks explicit where they still exist.
