import hashlib
import json
from pathlib import Path
import time

import requests


DEFAULT_REGISTRY = Path(__file__).resolve().parents[2] / "data" / "registry" / "public_datasets.json"


def load_registry(path=DEFAULT_REGISTRY):
    path = Path(path)
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("schema_version") != 1 or not isinstance(payload.get("datasets"), list):
        raise ValueError(f"Unsupported public dataset registry: {path}")
    ids = [item.get("id") for item in payload["datasets"]]
    if any(not item for item in ids) or len(ids) != len(set(ids)):
        raise ValueError("Dataset registry contains empty or duplicate identifiers.")
    return payload


def find_dataset(dataset_id, registry=None):
    registry = registry or load_registry()
    for item in registry["datasets"]:
        if item["id"] == dataset_id:
            return item
    raise KeyError(f"Unknown public dataset: {dataset_id}")


def file_checksum(path, algorithm="sha256", chunk_size=1024 * 1024):
    digest = hashlib.new(algorithm)
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(chunk_size), b""):
            digest.update(chunk)
    return digest.hexdigest().lower()


def verify_file_checksum(path, checksum):
    if not checksum:
        return True
    algorithm, expected = checksum.split(":", 1)
    return file_checksum(path, algorithm.lower()) == expected.strip().lower()


def _download_file(url, destination, checksum=None, timeout=60, max_retries=4, retry_backoff=1.0):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + ".part")
    retryable = (
        requests.exceptions.ConnectionError,
        requests.exceptions.Timeout,
        requests.exceptions.ChunkedEncodingError,
        OSError,
    )
    attempts = int(max_retries) + 1
    if attempts < 1:
        raise ValueError("max_retries must be zero or greater.")
    for attempt in range(attempts):
        existing = partial.stat().st_size if partial.exists() else 0
        headers = {"Range": f"bytes={existing}-"} if existing else {}
        try:
            with requests.get(
                url,
                stream=True,
                timeout=(15, timeout),
                headers=headers,
            ) as response:
                response.raise_for_status()
                append = existing > 0 and response.status_code == 206
                mode = "ab" if append else "wb"
                with partial.open(mode) as stream:
                    for chunk in response.iter_content(chunk_size=1024 * 1024):
                        if chunk:
                            stream.write(chunk)
            break
        except retryable:
            if attempt + 1 >= attempts:
                raise
            time.sleep(float(retry_backoff) * (2 ** attempt))
    if checksum and not verify_file_checksum(partial, checksum):
        raise ValueError(f"Checksum mismatch for downloaded file: {destination.name}")
    partial.replace(destination)
    return destination


def download_dataset(dataset_id, output_root, include=None, registry=None):
    registry = registry or load_registry()
    dataset = find_dataset(dataset_id, registry)
    downloads = dataset.get("downloads", [])
    if not downloads:
        raise ValueError(
            f"{dataset_id} does not provide an unattended download in the registry; "
            "follow its source and access instructions."
        )
    selected = set(include or [])
    dataset_dir = Path(output_root) / dataset_id
    dataset_dir.mkdir(parents=True, exist_ok=True)
    records = []
    for item in downloads:
        if selected and item["filename"] not in selected:
            continue
        destination = dataset_dir / item["filename"]
        if destination.exists() and verify_file_checksum(destination, item.get("checksum")):
            status = "already_verified"
        else:
            _download_file(item["url"], destination, item.get("checksum"))
            status = "downloaded_and_verified"
        records.append(
            {
                "filename": item["filename"],
                "status": status,
                "size_bytes": destination.stat().st_size,
                "sha256": file_checksum(destination),
                "source_url": item["url"],
            }
        )
    manifest = {
        "dataset_id": dataset_id,
        "dataset_name": dataset["name"],
        "source_url": dataset["source_url"],
        "license": dataset["license"],
        "citation": dataset["citation"],
        "usage_boundary": dataset["usage_boundary"],
        "files": records,
    }
    (dataset_dir / "source_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return manifest


def dataverse_file_inventory(persistent_id, server="https://dataverse.harvard.edu"):
    response = requests.get(
        f"{server.rstrip('/')}/api/datasets/:persistentId/",
        params={"persistentId": persistent_id},
        timeout=60,
    )
    response.raise_for_status()
    payload = response.json()
    if payload.get("status") != "OK":
        raise ValueError(f"Dataverse metadata request failed for {persistent_id}.")
    version = payload["data"]["latestVersion"]
    files = []
    for entry in version.get("files", []):
        data = entry["dataFile"]
        files.append(
            {
                "id": int(data["id"]),
                "filename": data["filename"],
                "size_bytes": int(data["filesize"]),
                "md5": data.get("md5"),
                "content_type": data.get("contentType"),
                "directory": entry.get("directoryLabel", ""),
                "download_url": f"{server.rstrip('/')}/api/access/datafile/{data['id']}",
            }
        )
    return {
        "persistent_id": persistent_id,
        "version": version.get("versionNumber"),
        "release_time": version.get("releaseTime"),
        "file_count": len(files),
        "total_size_bytes": int(sum(item["size_bytes"] for item in files)),
        "files": files,
    }


def download_dataverse_files(persistent_id, filenames, output_dir, server="https://dataverse.harvard.edu"):
    inventory = dataverse_file_inventory(persistent_id, server)
    by_name = {item["filename"]: item for item in inventory["files"]}
    missing = sorted(set(filenames) - set(by_name))
    if missing:
        raise KeyError(f"Dataverse files not found: {missing}")
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    records = []
    for name in filenames:
        item = by_name[name]
        destination = output_dir / name
        checksum = f"md5:{item['md5']}" if item.get("md5") else None
        if destination.exists() and verify_file_checksum(destination, checksum):
            status = "already_verified"
        else:
            _download_file(item["download_url"], destination, checksum)
            status = "downloaded_and_verified"
        records.append({**item, "status": status, "sha256": file_checksum(destination)})
    manifest = {"persistent_id": persistent_id, "files": records}
    (output_dir / "selected_download_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return manifest
