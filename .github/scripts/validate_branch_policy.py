#!/usr/bin/env python3

import os
import re
import sys


BRANCH_PATTERN = re.compile(r"^(?P<module>[^/]+)/(?P<kind>dev|main)-(?P<topic>[A-Za-z0-9._-]+)$")


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def parse_branch(name: str):
    match = BRANCH_PATTERN.fullmatch(name)
    return match.groupdict() if match else None


def main() -> int:
    base = os.environ.get("BASE_REF", "").strip()
    head = os.environ.get("HEAD_REF", "").strip()

    if not base or not head:
        return fail("Missing `BASE_REF` or `HEAD_REF` environment variable.")

    if base == "master":
        head_branch = parse_branch(head)
        if not head_branch or head_branch["kind"] != "main":
            return fail(
                f"`master` only accepts pull requests from `<module>/main-*` branches. "
                f"Received head branch `{head}`."
            )

        print(f"`master` <- `{head}` is allowed.")
        return 0

    base_branch = parse_branch(base)
    if not base_branch or base_branch["kind"] != "main":
        print(f"No branch policy applies to base branch `{base}`.")
        return 0

    head_branch = parse_branch(head)
    if not head_branch or head_branch["kind"] != "dev":
        return fail(
            f"`{base}` only accepts pull requests from `{base_branch['module']}/dev-*` branches. "
            f"Received head branch `{head}`."
        )

    if head_branch["module"] != base_branch["module"]:
        return fail(
            f"`{base}` only accepts pull requests from the same module namespace. "
            f"Expected `{base_branch['module']}/dev-*`, received `{head}`."
        )

    print(f"`{base}` <- `{head}` is allowed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
