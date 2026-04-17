# GitHub Branch Policy

This repository uses GitHub rulesets to protect target branches. Branch naming and source-to-target flow stay as project conventions.

## Branch naming

- Development branches: `<module>/dev-<topic>`
- Module main branches: `<module>/main-<topic>`
- Default branch: `master`

Examples:

- `parser/dev-gql-query-root-followup`
- `parser/main-gql-refactor`

## Allowed pull request flow

- `master` only accepts pull requests from `<module>/main-*`
- `<module>/main-*` only accepts pull requests from the same module's `<module>/dev-*`

Examples:

- `parser/dev-gql-query-root-followup` -> `parser/main-gql-refactor`
- `parser/main-gql-refactor` -> `master`

## Required GitHub rulesets

GitHub rulesets can require pull requests and block direct pushes on protected targets. They do not natively enforce "base branch X only accepts head branches matching Y", so the source-branch flow above is currently a repository convention rather than a hard check.

Configure these repository rulesets in GitHub:

1. Ruleset for `master`
   - Target branches: `master`
   - Require a pull request before merging
   - Restrict updates

2. Ruleset for module main branches
   - Target branches: `*/main-*`
   - Require a pull request before merging
   - Restrict updates

Optional hardening:

- Apply rules to administrators
- Require at least one approval before merging
- Require linear history if you want to avoid merge commits on protected branches
