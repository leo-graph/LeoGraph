# `GQL` Refactor Status

## Stable Anchor

- Branch: `gql-ast-refactor`
- Last stable checkpoint pushed to remote: `463f4194807`
- Checkpoint summary:
  - `GQLParserUtils` owns the `antlr4` entry, `SLL -> LL` fallback, and error listeners.
  - `GQLParseTreeVisitor` builds a minimal `IAST`-based `GQL*` graph AST.
  - Graph AST nodes live under `src/Parsers/graph/AST/`.
  - `ParserGraphQuery` can return the new graph AST for the currently supported top-level prefixes, including `MATCH` and `OPTIONAL MATCH`.

## Current Shape

### Parser pipeline

- Entry lives in `src/Parsers/graph/GQLParserUtils.cpp`.
- The current internal root rule is `compositeQueryStatement`.
- This means the parser is currently centered around complete query statements, not standalone clause fragments.
- The current normalization rule is: keep `visit*` boundaries aligned with `GQL.g4`, but keep public query roots aligned with the `kgraph`-style `GQL*` query nodes.

### AST layer

- The new AST is intentionally kgraph-shaped but ClickHouse-native.
- All graph nodes inherit from `IAST` or `ASTWithAlias`, not kgraph `INode`.
- `IAST::children` must stay dense and non-null; optional graph children should stay in named fields and only be pushed into `children` when present.
- Query roots should stay stable:
  - linear clause queries return `GQLClausesQuery`;
  - set queries return `GQLSetQuery`;
  - top-level `SELECT` returns a one-clause `GQLClausesQuery` whose only clause is `GQLProjectClause::Type::Select`;
  - `nestedQuerySpecification` returns `GQLSubqueryClause`, and its inner `statement` child is itself a normalized query root.

### Visitor coverage

The currently supported minimal path is:

- `MATCH`
- `OPTIONAL MATCH`
- `WHERE` / `FILTER`
- `RETURN`
- structured `GROUP BY` inside `RETURN` and `SELECT`
- `ORDER BY`
- `OFFSET`
- `LIMIT`
- structured `FINISH` clauses
- structured standalone `ORDER BY` / `OFFSET` / `LIMIT` paging clauses
- focused `USE` query parts flattened into clause sequences with structured `USE` clauses
- focused nested queries now preserve a structured subquery wrapper after the `USE` clause
- nested query specifications now preserve a structured subquery wrapper, with dedicated nodes for `AT schema`, `schemaReference`, binding-variable definition blocks, binding initializers, and `NEXT YIELD`, and reject unsupported inner non-query statements explicitly instead of flattening them to raw text
- top-level `SELECT` statements now normalize to a one-clause `GQLClausesQuery` that holds a minimal `GQLProjectClause::Type::Select` with structured `WHERE` / `HAVING` / tails, graph-qualified nested-query `FROM` sources, and graph-match `FROM` lists preserved as structured source nodes
- the older `visitSelectStatement` exception on `SELECT ... FROM ...` paths is fixed by gathering parse-tree data before constructing the `GQLProjectClause`, so the full `GQLParser.*` suite is currently green again
- structured `CALL`, `LET`, and `FOR` clauses, including structured procedure references, inline query-compatible `CALL { ... }`, `CALL ... YIELD`, typed `LET VALUE`, and `FOR ... WITH OFFSET` / `WITH ORDINALITY` fields
- explicit rejection of unsupported inline `CALL` variable-scope clauses instead of top-level raw-text fallback
- `graphExpression` payloads in `USE`, graph-qualified `SELECT FROM`, and graph variable initializers now build `GQLGraphExpression` instead of top-level raw-text placeholders
- `bindingTableExpression` payloads in binding-table initializers now build `GQLBindingTableExpression`, including the nested-query form
- structured `IS` truth checks and a first predicate subset such as `IS NULL`, `PROPERTY_EXISTS`, `ALL_DIFFERENT`, `SAME`, and source / destination predicates
- structured path / graph prefixes, `MATCH ... YIELD`, optional `MATCH` blocks, parenthesized path patterns with quantifiers, and basic path / node / edge patterns
- basic label expressions
- a minimal expression subset with structured `ABS` / length / cardinality functions, structured `COUNT(*)`, structured `EXISTS` operands, and structured list / record literals
- simple set queries such as `UNION`

## Important Boundaries

- Top-level `ParserGraphQuery::parseImpl` is intentionally not the main focus yet. It should stay light until the new visitor and AST are more complete.
- Tests are useful as scratch coverage, but while the refactor is still long-lived they are not the primary progress gate.
- Parser exceptions should use existing ClickHouse codes such as `SYNTAX_ERROR`.
- Do not add a local fallback implementation of `__lsan_ignore_object`; use the shared declaration plus guarded call pattern.

## Recommended Next Steps

### 1. Keep the query contract stable while filling coverage

Keep working in `src/Parsers/graph/GQLParseTreeVisitor.cpp`, but do not widen the set of public query root shapes.

The immediate rule is:

- keep `GQLClausesQuery`, `GQLSetQuery`, and `GQLSubqueryClause` as the only intentional query-root contracts;
- keep `GQLProjectClause` as a clause node even when the grammar uses `selectStatement` as a direct query alternative.

Then reduce the query-level `throwUnsupported` cases.

Suggested order:

- keep the remaining top-level gaps focused on any future graph-selection forms that lowering proves it needs beyond the current `USE` / `SELECT FROM` / subquery wrappers
- widen nested query support beyond the single-statement unwrap path
- keep reducing the remaining query-level `throwUnsupported` branches before touching parser entry gating again

The goal of this phase is to make the current query-oriented root feel complete before adding more outer integration.

### 2. Continue filling the remaining pattern-side holes

After the main query chain is healthier, cover the remaining pattern-related unsupported branches:

- `KEEP ...` coverage beyond the current path-prefix wrapper
- simplified path-pattern expressions and other non-element path primaries
- broader optional-match block normalization if lowering wants something richer than the current block wrapper
- any missing path-search variants that round-trip tests expose

Most of this work should still stay in `GQLParseTreeVisitor`, with small AST additions under `src/Parsers/graph/AST/` only when the current nodes stop being expressive enough.

### 3. Improve the expression layer

`GQLExpr` is currently good enough for a first skeleton, but some branches still fall back to raw text.

Recommended direction:

- keep the generic `GQLExpr::Kind` approach for now;
- gradually replace the remaining `rawText` paths with structured expression nodes where they become important, especially higher-frequency value functions and remaining predicate payloads;
- postpone a large expression hierarchy until the parser coverage stabilizes.

### 4. Revisit lowering and top-level integration later

Only after the visitor coverage is broad enough should the refactor move to:

- deciding how `GQL*` AST lowers into the long-term analyzer or execution layer;
- widening or reshaping top-level parser gating in `ParserGraphQuery`.

## Files To Touch Next

- `src/Parsers/graph/GQLParseTreeVisitor.cpp`
- `src/Parsers/graph/GQLParseTreeVisitor.h`
- `src/Parsers/graph/AST/*.h`
- `src/Parsers/graph/fwd_decl.h`
- `src/Parsers/graph/GraphAST.h`

## Session Recovery Note

If a future session starts cold, read this file first, then read `.claude/learnings.md`, and only then inspect the current `graph` parser sources.
