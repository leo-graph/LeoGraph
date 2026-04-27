# `GQL` Refactor Status

## Stable Anchor

- Branch: `parser/dev-gql-expression-completion`
- Last stable checkpoint: see recent `[parser] ...` commits on the current branch.
- Checkpoint summary:
  - `GQLParserUtils` owns the `antlr4` entry, `SLL -> LL` fallback, and error listeners.
  - `GQLParseTreeVisitor` builds a minimal `IAST`-based `GQL*` graph AST.
  - Graph AST nodes live under `src/Parsers/graph/AST/`.
  - `ParserGQLQuery` is the main parser-only entry for `Dialect::gql`; it routes full statements through `GQLParserUtils::parseStatement`.
  - Ordinary ClickHouse `ParserQuery` no longer routes graph-shaped prefixes into GQL; production GQL parsing requires explicit `Dialect::gql`.

## Current Shape

### Parser pipeline

- Entry helpers live in `src/Parsers/graph/GQLParserUtils.cpp`.
- The current main dialect-mode root rule is `statement`, reached through `ParserGQLQuery` and `GQLParserUtils::parseStatement`.
- `parseStatement` covers query, DML, and catalog DDL AST construction; it is the only public ANTLR rule used by the GQL parser entry.
- The current normalization rule is: keep `visit*` boundaries aligned with `GQL.g4`, but keep public query roots aligned with a semantic query model instead of grammar wrappers.

### AST layer

- The new AST is intentionally kgraph-shaped but ClickHouse-native.
- All graph nodes inherit from `IAST` or `ASTWithAlias`, not kgraph `INode`.
- `IAST::children` must stay dense and non-null; optional graph children should stay in named fields and only be pushed into `children` when present.
- Query roots should stay stable:
  - linear clause queries return `GQLSingleQuery`;
  - set queries return `GQLCombinedQuery`;
  - `GQLCombinedQuery` stores `queries + operators`, and operators preserve `ALL` / `DISTINCT` semantics explicitly;
  - top-level `SELECT` returns a `GQLSingleQuery` whose first clause is `GQLSelectClause`, followed by `GQLPageClause` when paging is present;
  - `nestedQuerySpecification` returns `GQLSubquery`, and its inner `query` child is itself a normalized query root.

### Visitor coverage

The currently supported minimal path is:

- `MATCH`
- `OPTIONAL MATCH`
- `WHERE` / `FILTER`
- `RETURN` via `GQLReturnClause`
- `SELECT` via `GQLSelectClause`
- structured `GROUP BY` inside `RETURN` and `SELECT`
- `ORDER BY`
- `OFFSET`
- `LIMIT`
- structured `FINISH` clauses
- structured standalone `ORDER BY` / `OFFSET` / `LIMIT` paging clauses
- focused `USE` query parts flattened into clause sequences with structured `USE` clauses
- focused nested queries now preserve a structured subquery wrapper after the `USE` clause
- nested query specifications now preserve a structured `GQLSubquery` wrapper, with dedicated nodes for `AT schema`, `schemaReference`, binding-variable definition blocks, binding initializers, and `NEXT YIELD`, and reject unsupported inner non-query statements explicitly instead of flattening them to raw text
- top-level `SELECT` statements now normalize to `GQLSingleQuery`, with `GQLSelectClause` carrying `FROM` / `WHERE` / `GROUP BY` / `HAVING`, graph-qualified nested-query `FROM` sources preserved as structured source nodes, and `ORDER BY` / `OFFSET` / `LIMIT` emitted as a separate `GQLPageClause`
- the older `visitSelectStatement` exception on `SELECT ... FROM ...` paths is fixed by gathering parse-tree data before constructing the final clause node
- structured `CALL`, `LET`, and `FOR` clauses, including separate `GQLCallNamedClause` / `GQLCallInlineClause` nodes, structured procedure references, inline query-compatible `CALL { ... }`, inline `CALL (x, y) { ... }` variable scopes, `CALL ... YIELD`, typed `LET VALUE`, and `FOR ... WITH OFFSET` / `WITH ORDINALITY` fields
- `graphExpression` payloads in `USE`, graph-qualified `SELECT FROM`, and graph variable initializers now build `GQLGraphExpression` instead of top-level raw-text placeholders
- graph references, catalog object names, graph copy sources, and catalog DDL sources are structured as thin AST nodes where the grammar shape is clear
- `bindingTableExpression` payloads in binding-table initializers now build `GQLBindingTableExpression`, including the nested-query form
- structured `IS` truth checks and a first predicate subset such as `IS NULL`, `PROPERTY_EXISTS`, `ALL_DIFFERENT`, `SAME`, and source / destination predicates
- structured path / graph prefixes, `GQLCountSpec` for counted path-search prefixes, classic `pathPatternExpression` alternation via `GQLPathTerm` / `GQLPathPatternAlternation`, `MATCH ... YIELD`, optional `MATCH` blocks, parenthesized path patterns with quantifiers, simplified path-pattern expressions, `GQLQuantifiedPathPrimary` wrappers for quantified / questioned non-element path primaries, and basic path / node / edge patterns
- basic label expressions
- a minimal expression subset with structured `ABS` / length / cardinality functions, structured aggregate `DISTINCT` / `ALL` via `GQLExpr::SetQuantifier`, structured `COUNT(*)`, structured `ELEMENT_ID`, structured `EXISTS` operands, structured dynamic parameters / `SESSION_USER`, structured list / record literals, structured `DURATION_BETWEEN` with `TemporalQualifier`, and structured `TRIM` with `TrimSpec` + optional trim character
- simple set queries such as `UNION`, `UNION ALL`, and `EXCEPT`
- DML (`INSERT`, `SET`, `REMOVE`, `DELETE`) and catalog DDL (`CREATE` / `DROP` schema, graph, graph type) through `ParserGQLQuery` / `parseStatement`

### Parser-only raw text policy

- `GQLCatalogStatement::source_text` is reserved for `NestedSpec` graph-type specifications and represents a deferred AST subtree.
- `GQLAssignmentItem::raw_type` and `GQLBindingVariableDefinition::raw_type` remain raw type strings for parser-only v1; do not design a full type AST in this phase.
- Plain literal tokens may stay as `GQLExpr::Kind::Literal`; raw text should be reduced only when it loses obvious graph-reference, catalog-name, graph-source, or expression structure.
- New or changed `GQL*` nodes must keep `children` dense and non-null, deep-copy owned children in `clone`, and round-trip through normalized `formatAST`.

### Current `throwUnsupported` coverage map

Track remaining `throwUnsupported` sites by parser-only impact, not by stale line numbers:

| Category | Current status | Priority |
|----------|----------------|----------|
| `predicate` | Existing grammar alternatives are structurally handled in `visitPredicateExprAlt`; the generic `predicate` fallback is a defensive guardrail. | Low until a concrete grammar branch reproduces. |
| `valueExpressionPrimary` / `nonParenthesizedValueExpressionPrimary` / `objectExpressionPrimary` | Current grammar alternatives are covered through `GQLExpr`, `GQLCaseExpr`, `GQLSubquery`, and path / let wrappers; remaining throws are defensive. | Low; do not churn without a failing input. |
| value-function families | Numeric, string, datetime, duration, list, aggregate, and datetime-subtraction branches have thin `GQLExpr` coverage; remaining throws are family guardrails. | Medium only when a new canonical input exposes a missed branch. |
| graph / binding / schema references | Most common graph references, catalog object names, graph sources, and binding-table references are thin structured AST; some grammar variants remain intentionally explicit unsupported. | Medium for obvious object-reference structure, otherwise defer. |
| query-shape guardrails | focused query, primitive query, `SELECT` shape, alias-on-non-aliasable, and complex optional-match branches protect the stable root contract. | Keep unless a stable `GQLSingleQuery` / `GQLCombinedQuery` / `GQLSubquery` shape is clear. |
| nested procedure metadata | `AT schema`, binding definitions, initializers, and `NEXT YIELD` are structured; unsupported initializer / non-query cases should stay explicit until represented. | Medium after expression gaps. |

The next implementation slice should start from a concrete parser input that currently throws, then add the minimum AST structure and contract test for that input. Do not delete a guardrail just to reduce the count.

### Phase 5: Expression completion

- `caseExpression` fully structured: `NULLIF` / `COALESCE` map to `GQLExpr::FunctionCall`, `CASE WHEN...END` (both simple and searched forms) use the new `GQLCaseExpr` AST node with `when_operands` / `then_results` / `else_result` fields; simple-CASE `caseOperand` and `whenOperand` now use `makeNpvepExpr` / `makeWhenOperandExpr` helpers for full expression dispatch; `compOp valueExpression` when-operands (e.g. `WHEN > 5`) structure as `GQLExpr::UnaryOp`; multi-operand `whenOperandList` (`WHEN 1, 2, 3`) uses `GQLExpr::ExprList` with individually structured children; `compOp` and predicate-part2 when-operands (IS NULL, IS DIRECTED, IS SOURCE OF, etc.) structured as `GQLExpr::BinaryOp` with cloned `caseOperand` as left child for complete predicate semantics, matching the existing full-predicate modeling pattern; `GQLCaseExpr::formatImplWithoutAlias` formats these BinaryOp when_operands as suffix-only (op + right) to preserve round-trip; `normalizedPredicatePart2` now fully structured as `BinaryOp` with `IS [NOT]` as operator and `[form] NORMALIZED` as literal right operand
- `castSpecification` structured: `CAST(operand AS type)` maps to `GQLExpr::Cast` with `children[0]` holding the operand and `text` holding the target type; round-trip formats as `CAST(x AS T)`
- `numericValueFunction` now covers all 13 grammar branches: the original 3 (`lengthExpression`, `cardinalityExpression`, `absoluteValueExpression`) plus `FLOOR`, `CEILING` / `CEIL`, `MOD`, `POWER`, `SQRT`, `SIN` / `COS` / `TAN` / `COT` / `SINH` / `COSH` / `TANH`, `LOG`, `LOG10`, `LN`, `EXP`
- `characterOrByteStringFunction` now covers all 5 branches structurally: `LEFT` / `RIGHT` (sub-character), `UPPER` / `LOWER` (fold), `BTRIM` / `LTRIM` / `RTRIM` (multi-trim), `NORMALIZE`, and `TRIM(trimOperands)` via `GQLExpr::TrimString` with explicit `TrimSpec` (Leading / Trailing / Both / None) and optional trim character child
- `makeValueFunction` refactored: split into `makeNumericValueFunction` + `makeCharacterOrByteStringFunction` for cleaner structure; `datetimeValueFunction`, `durationValueFunction`, `listValueFunction` structurally covered as `GQLExpr::FunctionCall` (Round C); `datetimeSubtraction` structurally covered as `GQLExpr::DurationBetween` with `TemporalQualifier` (Round D)
- `valueExpressionPrimary` now fully structured: `valueQueryExpression` maps to `GQLExpr::Kind::ValueQuery` with `GQLSubquery` child; `letValueExpression` maps to `GQLExpr::Kind::LetExpr` reusing `GQLAssignmentItem` bindings; `pathValueConstructor` maps to `GQLExpr::Kind::PathConstructor` with ordered node/edge children; `normalizedPredicateExprAlt` now has a dedicated `visitNormalizedPredicateExprAlt` override producing `BinaryOp` with `IS [NOT]` operator and `[form] NORMALIZED` literal right
- remaining `throwUnsupported(...)` sites should be tracked by category instead of stale line counts; the high-value parser-only categories are `predicate`, `valueExpressionPrimary`, `objectExpressionPrimary`, value-function families, and query-shape guardrails.

### Phase 6: DML statement support

- `linearDataModifyingStatement` now fully structured: the visitor handles both `focusedLinearDataModifyingStatement` and `ambientLinearDataModifyingStatement`, producing `GQLSingleQuery` with mixed query + DML clauses
- 4 new DML clause AST nodes: `GQLInsertClause` (with `GQLInsertPathPattern` for node-edge-node chains), `GQLSetClause` (with `GQLSetItem` for property / all-properties / label forms), `GQLRemoveClause` (with `GQLRemoveItem` for property / label), `GQLDeleteClause` (with `DetachMode` for `DETACH` / `NODETACH`)
- insert patterns reuse existing `GQLNodePattern` / `GQLEdgePattern` nodes, with `labelSetSpecification` built as `GQLLabelExpression::Conjunction`
- `callDataModifyingProcedureStatement` shares the same `makeCallClause` helper with `visitCallQueryStatement`, ensuring consistent AST output for both query-CALL and DML-CALL paths
- `buildSubquery` now handles `linearDataModifyingStatement` in nested procedure bodies (resolves the L1365 `nested non-query statement` gap for DML)
- catalog DDL is now represented by `GQLCatalogStatement`; nested procedure bodies can carry catalog statements where the query-root contract permits it.

- `GQLParserUtils::parseStatement` is the main ANTLR entry for `ParserGQLQuery`, used for query, DML, and catalog DDL in explicit `Dialect::gql` mode; do not reattach heuristic GQL routing to ordinary `ParserQuery`.

### Simplified-path follow-up

- `getSimplifiedOverrideDirection` / `getSimplifiedOverrideSecondary` now cover all 7 `simplifiedDirectionOverride` grammar variants explicitly and fall back to `throwUnsupported(...)` instead of silently treating unknown branches as `EdgeDirection::Any`.
- `GQLSimplifiedPathExpr::clone()` now clears the copied `operands` vector before rebuilding cloned n-ary children, so the clone no longer carries source-child pointers in its intermediate state.
- `docs/graph/parser.md` now documents simplified-path whitespace normalization during round-trip formatting; `gtest_gql_parser.cpp` covers the 6 new shape cases for Any / Conjunction / Undirected / UndirectedOrRight / Group+Repetition / Clone n-ary plus a quoted-label round-trip test.

## Important Boundaries

- Top-level GQL parsing is dialect-selected, not prefix-selected. Ordinary ClickHouse `ParserQuery` must not produce `GQL*` AST nodes for graph-shaped input.
- Tests are useful as scratch coverage, but while the refactor is still long-lived they are not the primary progress gate.
- Parser exceptions should use existing ClickHouse codes such as `SYNTAX_ERROR`.
- Do not add a local fallback implementation of `__lsan_ignore_object`; use the shared declaration plus guarded call pattern.

## Recommended Next Steps

Current parser-only priority is `GQL text -> normalized GQL IAST`. Do not spend this phase on binder, semantic analysis, interpreter, storage, catalog execution, or lowering. Prefer small implementation slices verified through `ParserGQLQuery` / `Dialect::gql` contract tests.

Suggested order for the next implementation slices:

1. reduce high-value `predicate` unsupported branches that affect common `WHERE`, `FILTER`, and `CASE WHEN` text;
2. reduce `valueExpressionPrimary` and `objectExpressionPrimary` branches that can map cleanly onto existing `GQLExpr` / thin wrapper nodes;
3. handle one value-function family at a time only when it closes a real AST shape gap;
4. keep query-shape guardrails explicit and documented unless a stable query-root representation is clear.

### 1. Keep the new query contract stable while filling coverage

Keep working in `src/Parsers/graph/GQLParseTreeVisitor.cpp`, but do not widen the set of public query root shapes.

The immediate rule is:

- keep `GQLSingleQuery`, `GQLCombinedQuery`, and `GQLSubquery` as the intentional query-level wrappers;
- keep `GQLSelectClause`, `GQLReturnClause`, and `GQLPageClause` as separate clause nodes instead of collapsing them back into one projection node.

Then reduce the query-level `throwUnsupported` cases.

Suggested order:

- keep the remaining top-level graph-selection gaps focused on explicit `Dialect::gql` coverage
- widen nested query support beyond the single-statement unwrap path
- keep reducing the remaining query-level `throwUnsupported` branches before touching parser entry gating again, with richer path semantics beyond the current simplified path-pattern AST as the next pattern-side topic
- introduce a thin `GQLQuery` base or equivalent shared query helper only if later lowering really benefits from it; do not churn the current roots just for naming

The goal of this phase is to make the current query-oriented root feel complete before adding more outer integration.

### 2. Continue filling the remaining pattern-side holes

After the main query chain is healthier, cover the remaining pattern-related unsupported branches:

- `KEEP ...` coverage beyond the current path-prefix wrapper
- richer path semantics beyond the current simplified path-pattern expressions, including any future lowering that needs more than the current wrapper/expression split
- broader optional-match block normalization if lowering wants something richer than the current block wrapper
- any missing path-search variants that round-trip tests expose

Most of this work should still stay in `GQLParseTreeVisitor`, with small AST additions under `src/Parsers/graph/AST/` only when the current nodes stop being expressive enough.

### 3. Continue improving the expression layer

Phase 5 + Rounds C/D/E/F covered most value-function and CASE branches. The three remaining `valueExpressionPrimary` gaps are now filled:

- `valueQueryExpression` (`VALUE { subquery }`) structured as `GQLExpr::Kind::ValueQuery` with `children[0]` = `GQLSubquery`
- `letValueExpression` (`LET ... IN ... END`) structured as `GQLExpr::Kind::LetExpr` with `children[0..n-2]` = `GQLAssignmentItem` bindings and `children[n-1]` = body expression
- `pathValueConstructor` (`PATH [ ... ]`) structured as `GQLExpr::Kind::PathConstructor` with ordered node/edge children
- `normalizedPredicatePart2` (`IS [NOT] [normalForm] NORMALIZED`) now fully structured as `BinaryOp` both in top-level `visitNormalizedPredicateExprAlt` and in CASE `whenOperand`, with `IS [NOT]` as operator and `[form] NORMALIZED` as literal right operand
- all simple-case predicate-part2 `whenOperand` forms (IS NULL / IS TYPED / IS DIRECTED / IS LABELED / IS SOURCE OF / IS DESTINATION OF / IS NORMALIZED) are now fully structured

Recommended direction:

- keep the generic `GQLExpr::Kind` approach; extend with dedicated nodes only when structure demands it (as done with `GQLCaseExpr`, `DurationBetween`, `TrimString`);
- postpone a large expression hierarchy until the parser coverage stabilizes.

### 4. Revisit lowering later

Only after the visitor coverage is broad enough should the refactor move to:

- deciding how `GQL*` AST lowers into the long-term analyzer or execution layer;
- revisiting product-level GQL dispatch if requirements change beyond explicit `Dialect::gql`.

## Files To Touch Next

- `src/Parsers/graph/GQLParseTreeVisitor.cpp`
- `src/Parsers/graph/GQLParseTreeVisitor.h`
- `src/Parsers/graph/AST/*.h`
- `src/Parsers/graph/fwd_decl.h`
- `src/Parsers/graph/GraphAST.h`
- `src/Parsers/graph/tests/gtest_gql_parser.cpp`

## Session Recovery Note

If a future session starts cold, read this file first, then read `.claude/learnings.md`, and only then inspect the current `graph` parser sources.
