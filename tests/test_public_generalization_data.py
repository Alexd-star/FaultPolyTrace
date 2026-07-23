# -*- coding: utf-8 -*-
import hashlib
import sys
from pathlib import Path

import pytest
import requests

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.registry import _download_file, load_registry, verify_file_checksum


def test_public_dataset_registry_has_real_and_labeled_sources():
    registry = load_registry(ROOT / "data" / "registry" / "public_datasets.json")
    datasets = {item["id"]: item for item in registry["datasets"]}

    assert {"thebe", "f3_netherlands", "penobscot", "cracks", "faultseg3d"} <= set(datasets)
    assert datasets["thebe"]["is_real"] is True
    assert datasets["thebe"]["has_fault_labels"] is True
    assert datasets["thebe"]["planar_validation_role"] == "primary_after_attribute_conversion"
    assert datasets["cracks"]["is_real"] is True
    assert datasets["cracks"]["direct_planar_input"] is False
    assert datasets["faultseg3d"]["is_real"] is False

    for item in datasets.values():
        assert item["source_url"].startswith("https://")
        assert item["license"]
        assert item["citation"]
        assert item["usage_boundary"]


def test_checksum_verifier_accepts_expected_sha256_and_rejects_mismatch(tmp_path):
    path = tmp_path / "sample.bin"
    path.write_bytes(b"fault-poly-trace")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()

    assert verify_file_checksum(path, f"sha256:{digest}") is True
    assert verify_file_checksum(path, "sha256:" + "0" * 64) is False


class _FakeResponse:
    def __init__(self, status_code, chunks):
        self.status_code = status_code
        self._chunks = chunks

    def __enter__(self):
        return self

    def __exit__(self, *_):
        return False

    def raise_for_status(self):
        return None

    def iter_content(self, chunk_size):
        del chunk_size
        for chunk in self._chunks:
            if isinstance(chunk, Exception):
                raise chunk
            yield chunk


def test_downloader_retries_and_resumes_after_interrupted_stream(tmp_path, monkeypatch):
    destination = tmp_path / "payload.bin"
    calls = []
    responses = [
        _FakeResponse(200, [b"abc", requests.exceptions.ChunkedEncodingError("interrupted")]),
        _FakeResponse(206, [b"def"]),
    ]

    def fake_get(url, **kwargs):
        calls.append((url, kwargs.get("headers", {})))
        return responses.pop(0)

    monkeypatch.setattr("generalization_validation.registry.requests.get", fake_get)
    monkeypatch.setattr("generalization_validation.registry.time.sleep", lambda _: None)
    digest = hashlib.sha256(b"abcdef").hexdigest()

    _download_file("https://example.test/payload", destination, f"sha256:{digest}", max_retries=2)

    assert destination.read_bytes() == b"abcdef"
    assert calls[0][1] == {}
    assert calls[1][1] == {"Range": "bytes=3-"}


def test_downloader_restarts_when_server_ignores_range(tmp_path, monkeypatch):
    destination = tmp_path / "payload.bin"
    destination.with_suffix(".bin.part").write_bytes(b"stale")
    monkeypatch.setattr(
        "generalization_validation.registry.requests.get",
        lambda *_, **__: _FakeResponse(200, [b"fresh"]),
    )
    digest = hashlib.sha256(b"fresh").hexdigest()

    _download_file("https://example.test/payload", destination, f"sha256:{digest}")

    assert destination.read_bytes() == b"fresh"


def test_downloader_keeps_partial_file_when_retries_are_exhausted(tmp_path, monkeypatch):
    destination = tmp_path / "payload.bin"
    monkeypatch.setattr(
        "generalization_validation.registry.requests.get",
        lambda *_, **__: _FakeResponse(
            200,
            [b"partial", requests.exceptions.ConnectionError("offline")],
        ),
    )
    monkeypatch.setattr("generalization_validation.registry.time.sleep", lambda _: None)

    with pytest.raises(requests.exceptions.ConnectionError):
        _download_file("https://example.test/payload", destination, max_retries=1)

    assert destination.with_suffix(".bin.part").read_bytes() == b"partial"
