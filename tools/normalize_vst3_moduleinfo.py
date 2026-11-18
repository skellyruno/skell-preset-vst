#!/usr/bin/env python3
"""Normalize and validate JUCE-generated VST3 moduleinfo.json.

Only removes trailing commas outside JSON string literals. Never changes the
VST3 binary. Strictly validates the plugin identity and unique class IDs.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def normalized_json(source: str) -> str:
    out: list[str] = []
    quoted = False
    escaped = False
    for i, char in enumerate(source):
        if quoted:
            out.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            continue
        if char == '"':
            quoted = True
        if char == ",":
            j = i + 1
            while j < len(source) and source[j].isspace():
                j += 1
            if j < len(source) and source[j] in "}]":
                continue
        out.append(char)
    fixed = "".join(out)
    json.loads(fixed)
    return fixed


def validate_metadata(
    data: dict[str, Any],
    expected_name: str | None = None,
    expected_vendor: str | None = None,
    expected_version: str | None = None,
) -> None:
    if not isinstance(data.get("Name"), str) or not data["Name"].strip():
        raise ValueError("VST3 moduleinfo must contain a non-empty Name")
    if not isinstance(data.get("Version"), str) or not data["Version"].strip():
        raise ValueError("VST3 moduleinfo must contain a non-empty Version")
    factory = data.get("Factory Info")
    if not isinstance(factory, dict) or not isinstance(factory.get("Vendor"), str):
        raise ValueError("VST3 moduleinfo must contain Factory Info.Vendor")
    if expected_name is not None and data["Name"] != expected_name:
        raise ValueError(f"VST3 name mismatch: expected {expected_name!r}, got {data['Name']!r}")
    if expected_version is not None and data["Version"] != expected_version:
        raise ValueError(f"VST3 version mismatch: expected {expected_version!r}, got {data['Version']!r}")
    if expected_vendor is not None and factory["Vendor"] != expected_vendor:
        raise ValueError(f"VST3 vendor mismatch: expected {expected_vendor!r}, got {factory['Vendor']!r}")

    classes = data.get("Classes")
    if not isinstance(classes, list) or len(classes) != 2:
        raise ValueError("VST3 moduleinfo must contain exactly two plugin classes")
    ids: list[str] = []
    categories: set[str] = set()
    for entry in classes:
        if not isinstance(entry, dict):
            raise ValueError("VST3 Classes entries must be objects")
        cid = entry.get("CID")
        if not isinstance(cid, str) or len(cid) != 32:
            raise ValueError("VST3 class CID must be a 32-character hexadecimal string")
        try:
            int(cid, 16)
        except ValueError as exc:
            raise ValueError(f"VST3 class CID is not hexadecimal: {cid!r}") from exc
        ids.append(cid.upper())
        category = entry.get("Category")
        if not isinstance(category, str) or not category:
            raise ValueError("VST3 class Category must be non-empty")
        categories.add(category)
        if expected_name is not None and entry.get("Name") != expected_name:
            raise ValueError("VST3 class Name does not match expected plugin name")
        if expected_vendor is not None and entry.get("Vendor") != expected_vendor:
            raise ValueError("VST3 class Vendor does not match expected vendor")
        if expected_version is not None and entry.get("Version") != expected_version:
            raise ValueError("VST3 class Version does not match expected version")
    if len(set(ids)) != len(ids):
        raise ValueError("Duplicate plugin class IDs in VST3 moduleinfo")
    if categories != {"Audio Module Class", "Component Controller Class"}:
        raise ValueError(f"Unexpected VST3 class categories: {sorted(categories)}")


def self_test() -> None:
    samples = [
        ('{"value":[1, 2,],"text":"literal ,} ,] here",}',
         {"value": [1, 2], "text": "literal ,} ,] here"}),
        ('{"nested":{"ok":true,},"empty":[],"quote":"a\\\" ,}",}',
         {"nested": {"ok": True}, "empty": [], "quote": 'a" ,}'}),
        ('{"valid":[1,2],"next":true}', {"valid": [1, 2], "next": True}),
    ]
    for source, expected in samples:
        actual = json.loads(normalized_json(source))
        if actual != expected:
            raise AssertionError(f"metadata scanner changed valid data: {actual}")
    duplicate = {
        "Name": "X", "Version": "1", "Factory Info": {"Vendor": "Y"},
        "Classes": [
            {"CID": "0" * 32, "Category": "Audio Module Class"},
            {"CID": "0" * 32, "Category": "Component Controller Class"},
        ],
    }
    try:
        validate_metadata(duplicate)
    except ValueError as exc:
        if "Duplicate" not in str(exc):
            raise
    else:
        raise AssertionError("duplicate class IDs were accepted")
    print("PASS: strict JSON normalization and duplicate-CID self-tests")


def main() -> None:
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("moduleinfo", type=Path, nargs="?")
    cli.add_argument("--self-test", action="store_true")
    cli.add_argument("--expected-name")
    cli.add_argument("--expected-vendor")
    cli.add_argument("--expected-version")
    args = cli.parse_args()
    if args.self_test:
        self_test()
    if args.moduleinfo is None:
        if args.self_test:
            return
        cli.error("VST3 moduleinfo.json path is required")
    original = args.moduleinfo.read_text(encoding="utf-8-sig")
    normalized = normalized_json(original)
    data = json.loads(normalized)
    validate_metadata(data, args.expected_name, args.expected_vendor, args.expected_version)
    if normalized != original:
        args.moduleinfo.write_text(normalized.rstrip() + "\n", encoding="utf-8", newline="\n")
        print(f"Fixed strict JSON in {args.moduleinfo}")
    else:
        print(f"PASS: metadata already valid strict JSON: {args.moduleinfo}")
    print(f"PASS: {data['Name']} {data['Version']} · {len(data['Classes'])} unique VST3 classes · {data['Factory Info']['Vendor']}")


if __name__ == "__main__":
    main()
