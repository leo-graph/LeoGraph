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
| `INSERT` | `INSERT (n:Person {name: 'Alice'})` | `GQLInsertClause` with `GQLInsertPathPattern` through `ParserGQLQuery` / `parseStatement` |
| `SET` | `MATCH (n) SET n.age = 30` | `GQLSetClause` with property / all-properties / label items through `ParserGQLQuery` / `parseStatement` |
| `REMOVE` | `MATCH (n) REMOVE n.age` | `GQLRemoveClause` with property / label items through `ParserGQLQuery` / `parseStatement` |
| `DELETE` | `MATCH (n) DELETE n` | `GQLDeleteClause` with `DETACH` / `NODETACH` modes through `ParserGQLQuery` / `parseStatement` |
| catalog DDL | `CREATE GRAPH g ANY AS COPY OF h` | `GQLCatalogStatement` with structured object names and graph sources through `ParserGQLQuery` / `parseStatement` |
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
- binder, interpreter, storage, catalog execution, and query-plan lowering
- full type AST for graph / binding-variable type declarations; parser-only v1 keeps raw type strings there
- GQL text in ordinary ClickHouse sessions; production GQL parsing requires explicit `dialect = gql` / `query_language = gql`

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

Pattern nodes stay graph-native, while expressions use a `GQLExpr` layer. Most value-function branches are now structurally represented: numeric functions (all 13 branches), character/string functions (including `TRIM` via `GQLExpr::TrimString` with explicit `TrimSpec`), datetime functions, duration functions (including `DURATION_BETWEEN` via `GQLExpr::DurationBetween` with `TemporalQualifier`), and list functions. Bare keyword datetime forms (`CURRENT_DATE`, `CURRENT_TIME`, `CURRENT_TIMESTAMP`, `LOCAL_TIME`, `LOCAL_TIMESTAMP`) are structured as `GQLExpr::Kind::FunctionCall` with `bare_keyword = true`, so round-trip output omits parentheses. Datetime and duration value functions with string parameters (e.g. `DATE('2024-01-15')`, `ZONED_TIME('10:30:00+02:00')`, `DURATION('P1Y')`) store their argument as a `GQLExpr::Kind::Literal` child node instead of raw text. The expression-primary layer now also covers `valueQueryExpression` (`VALUE { subquery }`) via `GQLExpr::Kind::ValueQuery`, `letValueExpression` (`LET ... IN ... END`) via `GQLExpr::Kind::LetExpr`, and `pathValueConstructor` (`PATH [ ... ]`) via `GQLExpr::Kind::PathConstructor`. The `normalizedPredicatePart2` (`IS [NOT] [normalForm] NORMALIZED`) is now fully structured as a `GQLExpr::BinaryOp` in both the top-level `normalizedPredicateExprAlt` visitor and the `whenOperand` CASE branch. `GQLExpr::Kind::FunctionCall` carries aggregate `DISTINCT` / `ALL` through a `SetQuantifier` field instead of stringifying the modifier back into raw text. Inside `GQLSubquery`, wrapper-level metadata such as `AT schema`, binding definitions, and `NEXT YIELD` now also have dedicated AST nodes instead of being stored as top-level raw-text leaves. `schemaReference`, `graphExpression`, `bindingTableExpression`, and binding initializers are also wrapped in dedicated nodes so the nested procedure-body contract can grow without reshaping its parents. Dynamic parameters (`$param`) are now `GQLExpr::Kind::DynamicParameter` instead of `Literal`, and keyword-based special values (`NULL`, `TRUE`, `FALSE`, `UNKNOWN`, `SESSION_USER`) are `GQLExpr::Kind::SpecialValue`, keeping the `Literal` kind reserved for actual data literals and string constants. Temporal literals (`DATE '...'`, `TIME '...'`, `DATETIME '...'`, `TIMESTAMP '...'`) are `GQLExpr::Kind::TemporalLiteral`, and duration literals (`DURATION '...'`) are `GQLExpr::Kind::DurationLiteral`; both keep the keyword in `text` and the string value as a `Literal` child node, distinguishing them from both plain string literals and parenthesized datetime/duration value functions (`FunctionCall`).

### Visitor Organization

The visitor is organized in the same high-level layers as the grammar:

- query-level visitors such as `visitCompositeQueryExpression`, `visitLinearQueryStatement`, and `visitSelectStatement` decide the normalized query root;
- clause-level visitors such as `visitMatchStatement`, `visitCallQueryStatement`, `visitFilterStatement`, and `visitReturnStatement` build individual clause nodes;
- pattern-level visitors such as `visitGraphPattern`, `visitPathPattern`, `visitNodePattern`, and `visitEdgePattern` build graph-specific subtrees;
- expression-level visitors build `GQLExpr` and related leaf nodes.

Key design constraints for the visitor:

- `visitSelectStatement` must finish reading the needed parse-tree branches before it constructs `GQLSelectClause`, then emit a `GQLSingleQuery` and append `GQLPageClause` only when paging exists.
- `visitNestedQuerySpecification` must preserve a `GQLSubquery` wrapper instead of unwrapping its inner query.
- Classic-pattern alternation and simplified-path alternation deliberately use parallel node families (`GQLPathPatternAlternation` vs `GQLSimplifiedPathExpr`); they should not be merged into one shared node type.
- If a nested procedure body contains a non-query statement that the current query AST cannot represent, the visitor should fail explicitly instead of hiding it inside a top-level raw-text query node.

These constraints keep the query contract stable while the lower layers continue to expand.

## Parser Pipeline

### Current `antlr4` Entry

The primary dialect-mode entry is `GQLParserUtils::parseStatement`, used by `ParserGQLQuery`. The grammar `statement` rule is a superset that includes query, DML, and catalog DDL alternatives, so `Dialect::gql` validates the main parser-only path with one normalized pipeline.

```cpp
ASTPtr parseDialectGQL(std::string_view query)
{
    OPENGQL::GQLParseTreeVisitor visitor;
    auto * parse_tree = OPENGQL::GQLParserUtils::parseStatement(query);
    return std::any_cast<ASTPtr>(visitor.visit(parse_tree));
}
```

### Top-Level ClickHouse Parser Entry

Ordinary ClickHouse `ParserQuery` does not auto-detect GQL. GQL text is not routed through a prefix-sniffing fallback in ClickHouse mode, so inputs such as `MATCH ... RETURN ...`, graph-shaped `SELECT`, focused `USE`, and GQL `CALL` must fail as ClickHouse queries unless the session has selected `Dialect::gql`.

Query, DML, and catalog DDL are covered through `ParserGQLQuery` in explicit `Dialect::gql` mode, where the whole statement is routed to `parseStatement` without competing with normal ClickHouse SQL prefixes.

### `Dialect::gql` Integration (Implemented)

`Dialect::gql` is now wired into the existing dialect mechanism alongside `kusto`, `prql`, and `promql`. The integration consists of:

- **Setting**: `allow_experimental_gql_dialect` (default `false`, `EXPERIMENTAL` tier). When the dialect is `gql` but this flag is off, all dispatch points throw `SUPPORT_IS_DISABLED`.
- **Alias**: `query_language` is a settings-level alias for `dialect` (declared via `DECLARE_WITH_ALIAS` in `Settings.cpp`). Both `SET dialect = 'gql'` and `SET query_language = 'gql'` route to the same `Dialect` enum and the same parser dispatch. `query_language` is not an independent setting; it resolves to `dialect` through the standard `BaseSettings` alias mechanism.
- **Parser wrapper**: `ParserGQLQuery` (`src/Parsers/graph/ParserGQLQuery.h/cpp`) inherits from `IParserBase`. The entire query text up to the next semicolon is routed through `GQLParserUtils::parseStatement` (the `statement` rule). Before that unconditional GQL parse, it gives `ParserSetQuery` one chance to parse real ClickHouse built-in settings. If every changed/defaulted setting name is a known ClickHouse setting, the statement is passed through as `ASTSetQuery`; otherwise the text stays on the GQL path, so graph assignments such as `SET n.x = 1` are not swallowed by ClickHouse `SET` syntax.
- **Server dispatch** (`executeQueryImpl`): a `Dialect::gql` branch before the fallback `ParserQuery`.
- **Client dispatch** (`ClientBase::parseQuery`, `LocalConnection`): matching `Dialect::gql` branches using the same `ParserGQLQuery`.

Under `dialect = gql` (equivalently `query_language = gql`), the top-level entry uses the GQL `statement` rule through `GQLParserUtils::parseStatement`, so query and DML statements share the same normalized GQL AST pipeline. Under `dialect = clickhouse`, ClickHouse `ParserQuery` remains responsible for SQL compatibility and does not produce `GQL*` AST nodes. This keeps language selection explicit and avoids growing prefix sniffing into a permanent mixed-parser architecture. The spelling `query_language` is now a settings-level alias for `dialect`, so parser dispatch has a single source of truth.

Known limitations of the current skeleton:

- Distributed / remote-node queries (`HedgedConnections`, `MultiplexedConnections`) reset foreign dialects to `clickhouse` before forwarding, but do not propagate `gql` to remote shards. Distributed GQL execution is not a usable path until GQL-to-SQL lowering exists.
- Interpreter / lowering is not yet wired: parsed GQL AST will enter `InterpreterFactory` but will fail with "unsupported" until lowering is implemented.

The current parser work therefore focuses on making:

`GQL text -> antlr4 parse tree -> normalized GQL IAST`

as complete and stable as possible before widening the top-level routing rules in `ParserQuery`.

### Raw Text Policy

The AST contract should keep obvious graph, catalog, and expression structure in typed `GQL*` nodes instead of storing source slices. Current acceptable raw-text fields are deliberately narrow:

- `GQLCatalogStatement::source_text` is only for `NestedSpec` graph-type specifications, which remain a deferred AST subtree.
- `GQLAssignmentItem::raw_type` and `GQLBindingVariableDefinition::raw_type` are temporary parser-only v1 strings for type declarations; designing a full type AST is out of scope for this front-end slice.
- Plain literal tokens can stay as `GQLExpr::Kind::Literal`; only raw text that loses catalog, graph-reference, or expression structure should be reduced further.

New or changed AST nodes should preserve a dense non-null `children` list, deep-copy all owned children in `clone`, and produce normalized `formatAST` output that can be reparsed through `ParserGQLQuery` / `parseStatement`.

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
