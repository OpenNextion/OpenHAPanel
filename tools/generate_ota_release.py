#!/usr/bin/env python3
"""Generate and validate the production OTA release metadata."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path


SCHEMA_VERSION = 2
PRODUCT = "open-ha-panel"
CHIP = "esp32s3"
PARTITION_LAYOUT = "ota-6m-v1"
DEFAULT_REPOSITORY = "OpenNextion/OpenHAPanel"
DEFAULT_DEVICE_MODEL = "ONX3248G035"
SUPPORTED_DEVICE_MODELS = {DEFAULT_DEVICE_MODEL}
CHECKSUM_FILENAME = "SHA256SUMS"
OTA_PARTITION_SIZE = 6 * 1024 * 1024
MANIFEST_MAX_SIZE = 8192
STABLE_TAG_PATTERN = re.compile(r"^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$")
GIT_SHA_PATTERN = re.compile(r"^[0-9a-fA-F]{40}$")
IDF_VERSION_PATTERN = re.compile(r"^v[0-9]+\.[0-9]+(?:\.[0-9]+)?(?:[-+][0-9A-Za-z.-]+)?$")
REPOSITORY_PATTERN = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
DEVICE_MODEL_PATTERN = re.compile(r"^[A-Z0-9][A-Z0-9_-]*$")


class ReleaseError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ReleaseError(message)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def device_model_slug(device_model: str) -> str:
    require(DEVICE_MODEL_PATTERN.fullmatch(device_model) is not None,
            "device model must contain only uppercase letters, digits, '_' or '-'")
    require(device_model in SUPPORTED_DEVICE_MODELS,
            f"unsupported device model: {device_model}")
    return device_model.lower().replace("_", "-")


def release_filenames(device_model: str) -> tuple[str, str, str]:
    prefix = f"{PRODUCT}-{device_model_slug(device_model)}"
    return (f"{prefix}.ota.bin",
            f"{prefix}.factory.bin",
            f"{prefix}.manifest.json")


def parse_partition_size(value: str) -> int:
    text = value.strip().upper()
    multiplier = 1
    if text.endswith("K"):
        multiplier = 1024
        text = text[:-1]
    elif text.endswith("M"):
        multiplier = 1024 * 1024
        text = text[:-1]
    try:
        return int(text, 0) * multiplier
    except ValueError as error:
        raise ReleaseError(f"Invalid partition size: {value}") from error


def validate_partitions(path: Path) -> None:
    require(path.is_file(), f"Partition table not found: {path}")
    ota_sizes: dict[str, int] = {}
    with path.open("r", encoding="utf-8", newline="") as source:
        rows = csv.reader(line for line in source if not line.lstrip().startswith("#"))
        for row in rows:
            if not row or len(row) < 5:
                continue
            name, partition_type, subtype, _, size = (item.strip() for item in row[:5])
            if partition_type == "app" and subtype in {"ota_0", "ota_1"}:
                ota_sizes[subtype] = parse_partition_size(size)

    require(set(ota_sizes) == {"ota_0", "ota_1"},
            "partitions.csv must contain ota_0 and ota_1 application partitions")
    for name, size in ota_sizes.items():
        require(size == OTA_PARTITION_SIZE,
                f"{name} must be exactly {OTA_PARTITION_SIZE} bytes for {PARTITION_LAYOUT}")


def validate_project_description(path: Path, version: str) -> None:
    require(path.is_file(), f"ESP-IDF project description not found: {path}")
    try:
        description = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ReleaseError(f"Cannot read project description: {error}") from error

    require(description.get("project_name") == PRODUCT,
            f"Unexpected project name: {description.get('project_name')!r}")
    require(description.get("project_version") == version,
            "Tag version does not match the firmware project version: "
            f"tag={version!r}, firmware={description.get('project_version')!r}")
    require(description.get("target") == CHIP,
            f"Unexpected ESP-IDF target: {description.get('target')!r}")


def validate_build_metadata(path: Path, *, version: str, device_model: str) -> None:
    require(path.is_file(), f"Build metadata not found: {path}")
    try:
        metadata = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ReleaseError(f"Cannot read build metadata: {error}") from error

    expected = {
        "product": PRODUCT,
        "deviceModel": device_model,
        "deviceModelSlug": device_model_slug(device_model),
        "version": version,
    }
    require(metadata == expected,
            f"Build metadata does not match release inputs: expected={expected!r} actual={metadata!r}")


def validate_timestamp(value: str) -> None:
    require(len(value) == 20 and value.endswith("Z"),
            "publishedAt must use UTC YYYY-MM-DDTHH:MM:SSZ format")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as error:
        raise ReleaseError(f"Invalid publishedAt timestamp: {value}") from error


def validate_manifest(manifest: dict, *, repository: str, tag: str, version: str,
                      git_sha: str, idf_version: str, device_model: str,
                      ota_filename: str, ota_path: Path) -> None:
    firmware_url = (
        f"https://github.com/{repository}/releases/download/{tag}/{ota_filename}"
    )
    release_url = f"https://github.com/{repository}/releases/tag/{tag}"
    expected = {
        "schemaVersion": SCHEMA_VERSION,
        "product": PRODUCT,
        "channel": "stable",
        "version": version,
        "build": {"gitSha": git_sha.lower(), "idfVersion": idf_version},
        "target": {
            "model": device_model,
            "chip": CHIP,
            "partitionLayout": PARTITION_LAYOUT,
        },
        "firmware": {
            "url": firmware_url,
            "size": ota_path.stat().st_size,
            "sha256": sha256_file(ota_path),
        },
        "secureVersion": 0,
        "mandatory": False,
        "releaseUrl": release_url,
    }
    for key, value in expected.items():
        require(manifest.get(key) == value,
                f"Manifest field {key!r} does not match the release inputs")
    require(set(manifest) == set(expected) | {"publishedAt"},
            "Manifest contains missing or unexpected top-level fields")
    validate_timestamp(manifest.get("publishedAt", ""))


def generate(args: argparse.Namespace) -> None:
    tag_match = STABLE_TAG_PATTERN.fullmatch(args.tag)
    require(tag_match is not None,
            "Only stable release tags in vMAJOR.MINOR.PATCH format are supported")
    version = args.tag[1:]
    require(GIT_SHA_PATTERN.fullmatch(args.git_sha) is not None,
            "git SHA must contain exactly 40 hexadecimal characters")
    require(IDF_VERSION_PATTERN.fullmatch(args.idf_version) is not None,
            "ESP-IDF version must look like v5.5.5")
    require(REPOSITORY_PATTERN.fullmatch(args.repository) is not None,
            "repository must use the GitHub owner/repository form")
    model_slug = device_model_slug(args.device_model)
    ota_filename, factory_filename, manifest_filename = release_filenames(args.device_model)

    ota_path = args.ota_bin.resolve()
    factory_path = args.factory_bin.resolve()
    require(ota_path.is_file(), f"OTA image not found: {ota_path}")
    require(factory_path.is_file(), f"Factory image not found: {factory_path}")
    require(ota_path.name == ota_filename,
            f"OTA image must be named {ota_filename}")
    require(factory_path.name == factory_filename,
            f"Factory image must be named {factory_filename}")
    require(0 < ota_path.stat().st_size <= OTA_PARTITION_SIZE,
            f"OTA image must fit in the {OTA_PARTITION_SIZE}-byte OTA partition")
    require(factory_path.stat().st_size > 0, "Factory image is empty")

    validate_partitions(args.partition_table.resolve())
    validate_project_description(args.project_description.resolve(), version)
    validate_build_metadata(args.build_metadata.resolve(),
                            version=version,
                            device_model=args.device_model)

    published_at = args.published_at or datetime.now(timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ"
    )
    manifest = {
        "schemaVersion": SCHEMA_VERSION,
        "product": PRODUCT,
        "channel": "stable",
        "version": version,
        "publishedAt": published_at,
        "build": {
            "gitSha": args.git_sha.lower(),
            "idfVersion": args.idf_version,
        },
        "target": {
            "model": args.device_model,
            "chip": CHIP,
            "partitionLayout": PARTITION_LAYOUT,
        },
        "firmware": {
            "url": (
                f"https://github.com/{args.repository}/releases/download/"
                f"{args.tag}/{ota_filename}"
            ),
            "size": ota_path.stat().st_size,
            "sha256": sha256_file(ota_path),
        },
        "secureVersion": 0,
        "mandatory": False,
        "releaseUrl": f"https://github.com/{args.repository}/releases/tag/{args.tag}",
    }
    validate_manifest(manifest,
                      repository=args.repository,
                      tag=args.tag,
                      version=version,
                      git_sha=args.git_sha,
                      idf_version=args.idf_version,
                      device_model=args.device_model,
                      ota_filename=ota_filename,
                      ota_path=ota_path)

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = output_dir / manifest_filename
    manifest_bytes = (json.dumps(manifest, indent=2, ensure_ascii=True) + "\n").encode("utf-8")
    require(len(manifest_bytes) <= MANIFEST_MAX_SIZE,
            f"Manifest exceeds the device limit of {MANIFEST_MAX_SIZE} bytes")
    manifest_path.write_bytes(manifest_bytes)

    checksum_paths = (ota_path, factory_path, manifest_path)
    checksums = "".join(f"{sha256_file(path)}  {path.name}\n" for path in checksum_paths)
    checksum_path = output_dir / CHECKSUM_FILENAME
    checksum_path.write_text(checksums, encoding="utf-8")

    parsed = json.loads(manifest_path.read_text(encoding="utf-8"))
    validate_manifest(parsed,
                      repository=args.repository,
                      tag=args.tag,
                      version=version,
                      git_sha=args.git_sha,
                      idf_version=args.idf_version,
                      device_model=args.device_model,
                      ota_filename=ota_filename,
                      ota_path=ota_path)
    print(f"Generated {manifest_path}")
    print(f"Generated {checksum_path}")
    print(f"OTA version={version} model={args.device_model} slug={model_slug} "
          f"size={ota_path.stat().st_size} sha256={sha256_file(ota_path)}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repository",
        default=DEFAULT_REPOSITORY,
        help=f"GitHub owner/repository used by release URLs (default: {DEFAULT_REPOSITORY})",
    )
    parser.add_argument(
        "--device-model",
        default=DEFAULT_DEVICE_MODEL,
        help=f"Firmware hardware model (default: {DEFAULT_DEVICE_MODEL})",
    )
    parser.add_argument("--tag", required=True, help="Stable Git tag, for example v1.2.3")
    parser.add_argument("--git-sha", required=True, help="Full 40-character release commit SHA")
    parser.add_argument("--idf-version", required=True, help="ESP-IDF version, for example v5.5.5")
    parser.add_argument("--ota-bin", required=True, type=Path)
    parser.add_argument("--factory-bin", required=True, type=Path)
    parser.add_argument("--project-description", required=True, type=Path)
    parser.add_argument("--build-metadata", required=True, type=Path)
    parser.add_argument("--partition-table", type=Path, default=Path("partitions.csv"))
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--published-at", help="Optional reproducible UTC timestamp")
    return parser


def main() -> int:
    try:
        generate(build_parser().parse_args())
    except (ReleaseError, OSError) as error:
        print(f"release metadata error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
