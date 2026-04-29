# GQL grammar

This grammar is based on the upstream OpenGQL grammar:

https://github.com/opengql/grammar/blob/main/GQL.g4

The local copy is kept in the repository so the generated C++ parser can be
reviewed and built without fetching grammar sources during normal builds.

## Local changes

- Added `gqlStatement : statement SEMICOLON* EOF;` as the ClickHouse production
  parser entry for the current parser-only phase. We currently parse one
  complete executable GQL `statement` into AST, not the full session or
  transaction-level `gqlProgram`.
- Added the `SEMICOLON` token so statement parsing can accept trailing
  semicolons in the grammar instead of trimming them in C++ driver code.
- Kept the upstream `gqlProgram` rule for future full GQL program support,
  including session and transaction activity.
- Updated `generate.sh` to use Homebrew `antlr` on macOS and the existing jar
  lookup flow on Linux.

After changing `GQL.g4`, run:

```bash
./src/Parsers/graph/grammar/generate.sh
```

The generated C++ files live in `src/Parsers/graph/generated`.
