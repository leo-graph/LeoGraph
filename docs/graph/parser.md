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

## Current Lowering-Facing Slice

The current implementation supports the parser-facing slice that is already stable enough for downstream lowering to consume without re-parsing source text:

### Supported Statements

| Statement | Example | Notes |
|-----------|---------|-------|
| `MATCH ... RETURN` | `MATCH (a:Person) RETURN a.name` | Core linear query path |
| `OPTIONAL MATCH` | `OPTIONAL MATCH (a)-[e]->(b) RETURN b` | Includes block-wrapper support |
| graph `SELECT` | `SELECT a FROM { MATCH (a) RETURN a }` | Normalizes to `GQLSelectClause` plus optional `GQLPageClause` |
| focused `USE` query | `USE foo MATCH (a) RETURN a` | Preserves `USE` as a structured clause |
| named / inline `CALL` | `CALL foo(a) YIELD x RETURN x` | Distinct AST nodes for named vs inline calls |
| `INSERT` | `INSERT (n:Person {name: 'Alice'})` | `GQLInsertClause` with `GQLInsertPathPattern` (via `parseStatement` direct entry) |
| `SET` | `MATCH (n) SET n.age = 30` | `GQLSetClause` with property / all-properties / label items (via `parseStatement`) |
| `REMOVE` | `MATCH (n) REMOVE n.age` | `GQLRemoveClause` with property / label items (via `parseStatement`) |
| `DELETE` | `MATCH (n) DELETE n` | `GQLDeleteClause` with `DETACH` / `NODETACH` modes (via `parseStatement`) |
| nested query | `{ MATCH (a) RETURN a } NEXT YIELD a { RETURN a }` | Preserves `GQLSubquery` wrapper |

### Supported Patterns

| Pattern | Syntax | Description |
|---------|--------|-------------|
| Node / edge | `(a:Label)` / `-[e:Label]->` | Core classic pattern nodes |
| Parenthesized path | `((a)-[e]->(b))?` | Keeps its own wrapper plus outer quantifier |
| Classic alternation | `(a)-[e]->(b) | (c)-[f]->(d)` | Preserved via `GQLPathPatternAlternation` |
| Simplified path | `()-/ :A | :B /->()` | Preserved via `GQLSimplifiedPathPattern` / `GQLSimplifiedPathExpr` |
| Counted search prefix | `MATCH ANY 3 PATHS ...` | Count preserved as `GQLCountSpec` |
| Quantified non-edge primary | `(a){2}` | Preserved via `GQLQuantifiedPathPrimary` |

### Deferred Features

- `SESSION` / `START TRANSACTION` (session management)
- broader schema / graph / binding-reference decomposition beyond the current wrapper nodes
- any future graph-selection forms beyond the current heuristic top-level `CALL` / graph-`SELECT` / focused-`USE` routing

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
  +-- GQLInsertClause         -- `INSERT` with insert path patterns
  +-- GQLInsertPathPattern    -- node-edge chain in an insert clause
  +-- GQLSetClause            -- `SET` with property / all-properties / label items
  +-- GQLRemoveClause         -- `REMOVE` with property / label items
  +-- GQLDeleteClause         -- `DELETE` with `DETACH` / `NODETACH` modes
  |
  +-- GQLPathPattern
  +-- GQLPathTerm
  +-- GQLPathPatternAlternation
  +-- GQLPathModePrefix
  +-- GQLPathSearchPrefix
  +-- GQLCountSpec
  +-- GQLParenthesizedPathPattern
  +-- GQLSimplifiedPathPattern
  +-- GQLSimplifiedPathExpr
  +-- GQLQuantifiedPathPrimary
  +-- GQLNodePattern
  +-- GQLEdgePattern
  +-- GQLLabelExpression
  +-- GQLQuantifier
  +-- GQLListConstructor
  +-- GQLRecordConstructor
  +-- GQLExpr                 -- current expression skeleton
```

Pattern nodes stay graph-native, while expressions use a `GQLExpr` layer. Most value-function branches are now structurally represented: numeric functions (all 13 branches), character/string functions (including `TRIM` via `GQLExpr::TrimString` with explicit `TrimSpec`), datetime functions, duration functions (including `DURATION_BETWEEN` via `GQLExpr::DurationBetween` with `TemporalQualifier`), and list functions. The expression-primary layer now also covers `valueQueryExpression` (`VALUE { subquery }`) via `GQLExpr::Kind::ValueQuery`, `letValueExpression` (`LET ... IN ... END`) via `GQLExpr::Kind::LetExpr`, and `pathValueConstructor` (`PATH [ ... ]`) via `GQLExpr::Kind::PathConstructor`. The `normalizedPredicatePart2` (`IS [NOT] [normalForm] NORMALIZED`) is now fully structured as a `GQLExpr::BinaryOp` in both the top-level `normalizedPredicateExprAlt` visitor and the `whenOperand` CASE branch. `GQLExpr::Kind::FunctionCall` carries aggregate `DISTINCT` / `ALL` through a `SetQuantifier` field instead of stringifying the modifier back into raw text. Inside `GQLSubquery`, wrapper-level metadata such as `AT schema`, binding definitions, and `NEXT YIELD` now also have dedicated AST nodes instead of being stored as top-level raw-text leaves. `schemaReference`, `graphExpression`, `bindingTableExpression`, and binding initializers are also wrapped in dedicated nodes so the nested procedure-body contract can grow without reshaping its parents.

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
- counted path-search prefixes should preserve their numeric or dynamic parameter payload via `GQLCountSpec` instead of storing the count as a raw string.
- classic `pathPatternExpression` should preserve `pathTerm` and top-level `|` / `|+|` alternation structurally via `GQLPathTerm` / `GQLPathPatternAlternation`, with `GQLPathPattern` and `GQLParenthesizedPathPattern` owning a single `expression` child instead of flattening operands into their parent `children`.
- quantified classic path primaries that cannot carry a quantifier inline should preserve the wrapper explicitly via `GQLQuantifiedPathPrimary`.
- `visitPpSimplifiedPathPatternExpression` should keep simplified path forms in dedicated `GQLSimplifiedPathPattern` / `GQLSimplifiedPathExpr` nodes instead of forcing them into `GQLEdgePattern`. `GQLSimplifiedPathExpr` round-trip preserves the AST shape but normalizes whitespace around binary operators such as `|`, `|+|`, `&`, and concatenation; tests should assert shape plus the normalized form rather than the raw source string.
- classic-pattern alternation and simplified-path alternation deliberately use parallel node families; they should not be merged into one shared node type.
- `visitPredicateExprAlt` should keep `EXISTS` operands as structured AST children (`GQLGraphPatternBlock`, `GQLMatchStatementBlock`, or `GQLSubquery`) instead of stringifying the body.
- aggregate `DISTINCT` / `ALL` should stay structured on `GQLExpr::FunctionCall` instead of being prefixed back into an argument raw-text payload.
- If a nested procedure body contains a non-query statement that the current query AST cannot represent, the visitor should fail explicitly instead of hiding it inside a top-level raw-text query node.

These two constraints keep the query contract stable while the lower layers continue to expand.

## Parser Pipeline

### Current `antlr4` Entry

The primary `antlr4` entry is `GQLParserUtils::parseCompositeQueryStatement` for query statements. A secondary entry `GQLParserUtils::parseStatement` is available for DML-containing statements; it uses the grammar `statement` rule which is a superset that includes both `compositeQueryStatement` and `linearDataModifyingStatement` as alternatives. This secondary entry is currently used for direct AST construction and testing via `parseDMLOrThrow`, not yet routed through `ParserGraphQuery`.

```cpp
ASTPtr parseQuery(std::string_view query, bool use_statement_rule = false)
{
    OPENGQL::GQLParseTreeVisitor visitor;
    if (use_statement_rule) {
        auto * parse_tree = OPENGQL::GQLParserUtils::parseStatement(query);
        return std::any_cast<ASTPtr>(visitor.visit(parse_tree));
    }
    auto * parse_tree = OPENGQL::GQLParserUtils::parseCompositeQueryStatement(query);
    return std::any_cast<ASTPtr>(visitor.visit(parse_tree));
}
```

### Top-Level ClickHouse Parser Entry

`ParserGraphQuery` is still a thin outer adapter. It currently routes strong graph prefixes such as `MATCH`, `OPTIONAL MATCH`, and `CALL` directly into the `antlr4` path, and uses heuristic gating for weaker prefixes such as top-level graph-shaped `SELECT` and focused `USE`. Because `ParserGraphQuery` runs before the regular SQL parser chain in `ParserQuery`, weak-prefix routing must fall back cleanly when the `antlr4` parse fails so plain SQL is not stolen accidentally.

DML statements are not yet routed through `ParserGraphQuery`. The DML AST layer is fully structured via `parseStatement` direct entry, but top-level DML routing (INSERT/SET/REMOVE/DELETE prefix detection, SQL compatibility fallback) is deferred to a future entry routing phase.

Longer term, top-level parser selection should not depend on `ParserGraphQuery` heuristics. `ParserGraphQuery` exists because the current codebase still sits inside the ClickHouse SQL parser chain, but that makes every widened GQL prefix compete with existing SQL syntax. ClickHouse already has a session-level `dialect` setting used for `kusto`, `prql`, and `promql`; the preferred strategy is to add `gql` to that existing dialect mechanism instead of growing a parallel language setting. If an external API needs the spelling `query_language = gql`, it should be implemented as an alias or wrapper over `dialect = gql` so parser dispatch has a single source of truth.

The intended dispatch shape is:

```cpp
if (settings[Setting::dialect] == Dialect::gql)
    return parseQuery(query, /* use_statement_rule = */ true);

return clickhouse_sql_parser.parse(query);
```

The real server-side insertion point is `executeQueryImpl`, after `Context` has resolved session settings and before it constructs the fallback `ParserQuery`. `ClientBase` and `LocalConnection` already mirror the same `dialect` dispatch for client-side parsing and should get matching `gql` branches. Under `dialect = gql`, the top-level entry should use the GQL `statement` rule through `GQLParserUtils::parseStatement`, so query and DML statements share the same normalized GQL AST pipeline. Under `dialect = clickhouse`, ClickHouse `ParserQuery` remains responsible for SQL compatibility. This keeps language selection explicit and avoids growing prefix sniffing into a permanent mixed-parser architecture. If the product direction becomes GQL-only, the final cleanup is to remove the ClickHouse top-level SQL parser path instead of expanding `ParserGraphQuery` further.

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
3. wrapper tests: verify that top-level graph `SELECT` normalizes to `GQLSelectClause` plus optional `GQLPageClause`, `{ ... }` keeps a `GQLSubquery` wrapper, and weak top-level prefixes still fall back to plain SQL when they are not graph-shaped;
4. path tests: verify that parenthesized and simplified path primaries keep their own AST families, that classic `|` / `|+|` alternation preserves `GQLPathPatternAlternation`, and that outer `?` / `{m,n}` quantifiers attach to either the native primary or `GQLQuantifiedPathPrimary` instead of being folded into an edge node;
5. pattern and expression tests: verify the current structured coverage, including aggregate `DISTINCT` / `ALL`, counted path prefixes, dynamic/special value primaries, `TRIM` shape/spec assertions, and `DURATION_BETWEEN` qualifier assertions, and make raw-text fallbacks explicit where they still exist.
