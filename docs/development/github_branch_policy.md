# GitHub Branch Policy

This repository uses two layers of enforcement for branch flow:

1. GitHub rulesets protect the target branches.
2. The `branch policy / validate` workflow checks whether the pull request source branch is allowed for the target branch.

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

GitHub rulesets can require pull requests and block direct pushes on protected targets, but the head-branch restriction is enforced by the workflow because GitHub does not provide a native rule for "base branch X only accepts head branches matching Y".

After merging this file and the workflow, configure these repository rulesets in GitHub:

1. Ruleset for `master`
   - Target branches: `master`
   - Require a pull request before merging
   - Restrict updates
   - Require status checks to pass before merging
     - Required check: `branch policy / validate`

2. Ruleset for module main branches
   - Target branches: `*/main-*`
   - Require a pull request before merging
   - Restrict updates
   - Require status checks to pass before merging
     - Required check: `branch policy / validate`

Optional hardening:

- Apply rules to administrators
- Require at least one approval before merging
- Require linear history if you want to avoid merge commits on protected branches
