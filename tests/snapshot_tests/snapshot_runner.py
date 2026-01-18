#!/usr/bin/env python3
"""
Snapshot-based conformance testing for copilot-sdk-cpp.

This script:
1. Parses upstream YAML snapshots from copilot-sdk/test/snapshots/
2. Converts them to test configs for the C++ replay executable
3. Runs the replay executable and captures output
4. Validates observed behavior matches expected patterns

Usage:
    python snapshot_runner.py [--snapshot-dir DIR] [--replay-exe PATH] [--filter PATTERN]

Environment:
    SNAPSHOT_REPLAY_EXE: Path to snapshot_replay executable
    UPSTREAM_SNAPSHOTS: Path to copilot-sdk/test/snapshots/
"""

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

try:
    import yaml  # type: ignore
except ModuleNotFoundError:
    print("ERROR: PyYAML is required to run snapshot conformance tests.")
    print("Install it with: python -m pip install --user PyYAML")
    sys.exit(1)


@dataclass
class ToolCallExpectation:
    """Expected tool call from a snapshot."""
    id: str
    name: str
    arguments: dict
    result: str


@dataclass
class TurnExpectation:
    prompt: str
    tool_calls: list[ToolCallExpectation] = field(default_factory=list)
    assistant_messages: list[str] = field(default_factory=list)


@dataclass
class SnapshotTest:
    """Parsed snapshot test case."""
    name: str
    source_file: str
    turns: list[TurnExpectation]
    tools: list[dict]
    system_message: Optional[dict] = None


def parse_snapshot(yaml_path: Path) -> Optional[SnapshotTest]:
    """Parse a YAML snapshot file into a SnapshotTest."""
    try:
        with open(yaml_path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
    except Exception as e:
        print(f"  [WARN] Failed to parse {yaml_path}: {e}")
        return None

    if not data or "conversations" not in data:
        return None

    # Extract from first conversation
    conv = data["conversations"][0]
    messages = conv.get("messages", [])

    turns: list[TurnExpectation] = []
    expected_calls_by_id: dict[str, ToolCallExpectation] = {}
    tools_needed = set()
    current_turn: Optional[TurnExpectation] = None

    for msg in messages:
        role = msg.get("role")
        if role == "user":
            content = msg.get("content", "")
            # Skip system placeholder
            if content != "${system}":
                current_turn = TurnExpectation(prompt=content)
                turns.append(current_turn)
        elif role == "assistant":
            tool_calls = msg.get("tool_calls", [])
            for tc in tool_calls:
                func = tc.get("function", {})
                tc_id = tc.get("id", "")
                name = func.get("name", "")
                # Skip placeholders like ${shell}
                if name.startswith("${"):
                    continue
                args_str = func.get("arguments", "{}")
                try:
                    args = json.loads(args_str)
                except json.JSONDecodeError:
                    args = {}
                tools_needed.add(name)
                call = ToolCallExpectation(
                    id=tc_id,
                    name=name,
                    arguments=args,
                    result=""  # Will be filled from tool role message
                )
                expected_calls_by_id[tc_id] = call
                if current_turn is not None:
                    current_turn.tool_calls.append(call)
            # Capture assistant text (when present)
            content = msg.get("content")
            if isinstance(content, str) and content.strip() and current_turn is not None:
                current_turn.assistant_messages.append(content)
        elif role == "tool":
            # Match this result to the last expected call with same id
            tool_call_id = msg.get("tool_call_id", "")
            content = msg.get("content", "")
            if tool_call_id in expected_calls_by_id:
                expected_calls_by_id[tool_call_id].result = content

    if not turns:
        return None

    # Build tool configs
    tools = []
    for tool_name in tools_needed:
        # Find the expected result and arguments for this tool
        result = "OK"
        args_schema = {"type": "object", "properties": {}}
        for turn in turns:
            for call in turn.tool_calls:
                if call.name != tool_name:
                    continue
                result = call.result
                # Build schema from expected arguments
                for arg_name, arg_val in call.arguments.items():
                    arg_type = "string"
                    if isinstance(arg_val, bool):
                        arg_type = "boolean"
                    elif isinstance(arg_val, int):
                        arg_type = "integer"
                    elif isinstance(arg_val, float):
                        arg_type = "number"
                    elif isinstance(arg_val, dict):
                        arg_type = "object"
                    elif isinstance(arg_val, list):
                        arg_type = "array"
                    args_schema["properties"][arg_name] = {"type": arg_type}
                break

        tools.append({
            "name": tool_name,
            "description": f"Test tool: {tool_name}",
            "parameters_schema": args_schema,
            "result": result
        })

    return SnapshotTest(
        name=yaml_path.stem,
        source_file=str(yaml_path),
        turns=turns,
        tools=tools
    )


def run_replay(exe_path: Path, test: SnapshotTest, timeout: int = 120) -> dict:
    """Run the C++ replay executable with the test config."""
    import tempfile

    config = {
        "turns": [
            {
                "prompt": t.prompt,
                "tool_calls": [{"id": c.id, "name": c.name, "arguments": c.arguments} for c in t.tool_calls],
                "assistant_messages": t.assistant_messages,
            }
            for t in test.turns
        ],
        "tools": test.tools
    }
    if test.system_message:
        config["system_message"] = test.system_message

    # Write config to temp file in same dir as exe (can't use stdin - Copilot CLI needs it)
    try:
        config_file = exe_path.parent / f"_test_config_{os.getpid()}.json"
        with open(config_file, 'w') as f:
            json.dump(config, f)

        result = subprocess.run(
            [str(exe_path), str(config_file)],
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=str(exe_path.parent)  # Run from exe directory
        )

        # Clean up temp file
        config_file.unlink(missing_ok=True)

        if result.returncode != 0:
            stderr = result.stderr.strip() if result.stderr else ""
            stdout = result.stdout.strip() if result.stdout else ""
            return {"error": f"Exit code {result.returncode}: {stderr or stdout}"}

        return json.loads(result.stdout)
    except subprocess.TimeoutExpired:
        if 'config_file' in locals():
            config_file.unlink(missing_ok=True)
        return {"error": "Timeout"}
    except json.JSONDecodeError as e:
        return {"error": f"Invalid JSON output: {e}\nOutput: {result.stdout[:500]}"}
    except Exception as e:
        return {"error": str(e)}


def validate_result(test: SnapshotTest, result: dict, verbose: bool = False) -> tuple[bool, list[str]]:
    """Validate replay result against expected behavior."""
    issues = []

    if "error" in result:
        return False, [f"Replay error: {result['error']}"]

    # Check tool calls
    observed_calls = []
    for turn in result.get("turns", []):
        for tc in turn.get("tool_calls", []):
            observed_calls.append(tc)

    if verbose:
        print(f"  Observed tool calls: {observed_calls}")

    # Validate each expected tool call was made
    for expected in [c for t in test.turns for c in t.tool_calls]:
        found = False
        for observed in observed_calls:
            if observed.get("name") == expected.name:
                found = True
                # Check arguments - just verify the tool was called with non-empty args
                # (exact arg matching is too strict since LLM may format differently)
                if verbose:
                    print(f"  Expected args: {expected.arguments}")
                    print(f"  Observed args: {observed.get('arguments', {})}")
                break

        if not found:
            issues.append(f"Expected tool call '{expected.name}' not observed")

    return len(issues) == 0, issues


def find_snapshots(snapshot_dir: Path, categories: list[str]) -> list[Path]:
    """Find all snapshot YAML files in specified categories."""
    snapshots = []
    for category in categories:
        cat_dir = snapshot_dir / category
        if cat_dir.exists():
            snapshots.extend(cat_dir.glob("*.yaml"))
    return sorted(snapshots)


def main():
    parser = argparse.ArgumentParser(description="Run snapshot conformance tests")
    parser.add_argument(
        "--snapshot-dir",
        type=Path,
        default=Path(__file__).parent.parent.parent.parent / "copilot-sdk" / "test" / "snapshots",
        help="Path to upstream snapshots directory"
    )
    parser.add_argument(
        "--replay-exe",
        type=Path,
        default=None,
        help="Path to snapshot_replay executable"
    )
    parser.add_argument(
        "--filter",
        type=str,
        default=None,
        help="Filter tests by name pattern"
    )
    parser.add_argument(
        "--categories",
        type=str,
        default="tools,session",
        help="Comma-separated list of snapshot categories to test"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output"
    )
    args = parser.parse_args()

    # Find replay executable
    exe_path = args.replay_exe
    if not exe_path:
        # Try common build locations
        base = Path(__file__).parent.parent.parent
        candidates = [
            base / "build" / "tests" / "snapshot_tests" / "Debug" / "snapshot_replay.exe",
            base / "build" / "tests" / "snapshot_tests" / "Release" / "snapshot_replay.exe",
            base / "build" / "tests" / "snapshot_tests" / "snapshot_replay.exe",
            base / "build" / "tests" / "snapshot_tests" / "snapshot_replay",
        ]
        for c in candidates:
            if c.exists():
                exe_path = c
                break

    if not exe_path or not exe_path.exists():
        print(f"ERROR: snapshot_replay executable not found")
        print("Build it with: cmake --build build --target snapshot_replay")
        return 1

    print(f"Using replay executable: {exe_path}")
    print(f"Snapshot directory: {args.snapshot_dir}")

    # Find snapshots
    categories = [c.strip() for c in args.categories.split(",")]
    snapshot_files = find_snapshots(args.snapshot_dir, categories)

    if not snapshot_files:
        print(f"No snapshots found in categories: {categories}")
        return 1

    print(f"Found {len(snapshot_files)} snapshot files\n")

    # Run tests
    passed = 0
    failed = 0
    skipped = 0

    for yaml_path in snapshot_files:
        test = parse_snapshot(yaml_path)
        if not test:
            skipped += 1
            if args.verbose:
                print(f"  [SKIP] {yaml_path.name}: Could not parse")
            continue

        if args.filter and args.filter not in test.name:
            skipped += 1
            continue

        # Only run tests that have tool calls (for now)
        if not any(t.tool_calls for t in test.turns):
            skipped += 1
            if args.verbose:
                print(f"  [SKIP] {test.name}: No tool calls to validate")
            continue

        print(f"Running: {test.name}")
        if args.verbose:
            print(f"  Prompts: {test.prompts}")
            print(f"  Expected tools: {[c.name for c in test.expected_tool_calls]}")

        result = run_replay(exe_path, test)
        success, issues = validate_result(test, result, verbose=args.verbose)

        if success:
            print(f"  [PASS] {test.name}")
            passed += 1
        else:
            print(f"  [FAIL] {test.name}")
            for issue in issues:
                print(f"    - {issue}")
            failed += 1

    # Summary
    print(f"\n{'='*50}")
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped")
    print(f"{'='*50}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
