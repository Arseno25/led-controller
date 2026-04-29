#!/usr/bin/env python3
"""Generate or update CHANGELOG.md from git commits between release tags."""

from __future__ import annotations

import argparse
import datetime as dt
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CHANGELOG = PROJECT_ROOT / "CHANGELOG.md"


@dataclass(frozen=True)
class Commit:
    sha: str
    subject: str
    author: str


def run_git(args: list[str], allow_fail: bool = False) -> str:
    completed = subprocess.run(
        ["git", *args],
        cwd=PROJECT_ROOT,
        text=True,
        capture_output=True,
    )
    if completed.returncode != 0:
        if allow_fail:
            return ""
        raise RuntimeError(completed.stderr.strip() or completed.stdout.strip())
    return completed.stdout.strip()


def previous_tag(ref: str) -> str | None:
    value = run_git(["describe", "--tags", "--abbrev=0", f"{ref}^"], allow_fail=True)
    return value or None


def collect_commits(ref: str, previous: str | None) -> list[Commit]:
    revision_range = f"{previous}..{ref}" if previous else ref
    output = run_git(
        ["log", "--no-merges", "--pretty=format:%H%x1f%s%x1f%an", revision_range],
        allow_fail=True,
    )
    commits: list[Commit] = []
    for line in output.splitlines():
        parts = line.split("\x1f")
        if len(parts) != 3:
            continue
        sha, subject, author = parts
        if not subject:
            continue
        commits.append(Commit(sha=sha, subject=subject, author=author))
    return commits


def category_for(subject: str) -> str:
    lowered = subject.lower()
    match = re.match(r"^([a-z]+)(\([^)]+\))?(!)?:", lowered)
    prefix = match.group(1) if match else ""
    breaking = bool(match and match.group(3))

    if breaking:
        return "Breaking Changes"
    if prefix == "feat":
        return "Added"
    if prefix == "fix":
        return "Fixed"
    if prefix == "perf":
        return "Performance"
    if prefix in {"refactor", "style"}:
        return "Changed"
    if prefix == "docs":
        return "Documentation"
    if prefix in {"ci", "build"}:
        return "Build and CI"
    if prefix in {"test", "chore"}:
        return "Maintenance"
    return "Other"


def clean_subject(subject: str) -> str:
    cleaned = re.sub(r"^[a-z]+(\([^)]+\))?!?:\s*", "", subject, flags=re.IGNORECASE)
    return cleaned[:1].upper() + cleaned[1:] if cleaned else subject


def commit_link(commit: Commit, repo: str | None) -> str:
    short = commit.sha[:7]
    if repo:
        return f"[`{short}`](https://github.com/{repo}/commit/{commit.sha})"
    return f"`{short}`"


def compare_line(version: str, previous: str | None, repo: str | None) -> str:
    if not repo:
        return ""
    if previous:
        return f"Compare: https://github.com/{repo}/compare/{previous}...{version}\n"
    return f"Release: https://github.com/{repo}/releases/tag/{version}\n"


def render_section(version: str, ref: str, date: str, previous: str | None, commits: list[Commit], repo: str | None) -> str:
    buckets: dict[str, list[Commit]] = {}
    order = [
        "Breaking Changes",
        "Added",
        "Fixed",
        "Performance",
        "Changed",
        "Documentation",
        "Build and CI",
        "Maintenance",
        "Other",
    ]

    for commit in commits:
        subject = commit.subject.lower()
        if subject.startswith("docs(changelog)") or subject.startswith("changelog:"):
            continue
        buckets.setdefault(category_for(commit.subject), []).append(commit)

    lines = [f"## [{version}] - {date}", ""]
    link = compare_line(version, previous, repo)
    if link:
        lines.extend([link.rstrip(), ""])

    if not any(buckets.values()):
        lines.extend(["- No user-facing changes recorded.", ""])
        return "\n".join(lines)

    for category in order:
        items = buckets.get(category, [])
        if not items:
            continue
        lines.extend([f"### {category}", ""])
        for commit in items:
            lines.append(f"- {clean_subject(commit.subject)} ({commit_link(commit, repo)})")
        lines.append("")

    return "\n".join(lines)


def base_changelog() -> str:
    return (
        "# Changelog\n\n"
        "All notable firmware changes are documented here. Release sections are "
        "generated from git commits when a GitHub Release is published.\n\n"
    )


def update_content(existing: str, section: str, version: str) -> str:
    if not existing.strip():
        existing = base_changelog()

    heading_pattern = re.compile(
        rf"^## \[{re.escape(version)}\].*?(?=^## \[|\Z)",
        re.MULTILINE | re.DOTALL,
    )
    if heading_pattern.search(existing):
        updated = heading_pattern.sub(section.strip() + "\n\n", existing)
        return updated.rstrip() + "\n"

    if existing.startswith("# Changelog"):
        parts = existing.split("\n", 1)
        rest = parts[1] if len(parts) > 1 else "\n"
        return f"{parts[0]}\n{rest.strip()}\n\n{section.strip()}\n"

    return f"{base_changelog()}{section.strip()}\n\n{existing.strip()}\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Update CHANGELOG.md for a release.")
    parser.add_argument("--version", required=True, help="Release version/tag, for example v1.2.0.")
    parser.add_argument("--ref", help="Git ref to inspect. Defaults to --version.")
    parser.add_argument("--repo", help="GitHub repository in owner/name format.")
    parser.add_argument("--date", default=dt.date.today().isoformat())
    parser.add_argument("--output", default=str(DEFAULT_CHANGELOG))
    args = parser.parse_args()

    ref = args.ref or args.version
    previous = previous_tag(ref)
    commits = collect_commits(ref, previous)
    section = render_section(args.version, ref, args.date, previous, commits, args.repo)

    output = Path(args.output)
    if not output.is_absolute():
        output = PROJECT_ROOT / output
    existing = output.read_text(encoding="utf-8") if output.exists() else ""
    output.write_text(update_content(existing, section, args.version), encoding="utf-8")
    print(f"Updated {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
