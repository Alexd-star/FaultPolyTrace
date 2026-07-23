from dataclasses import dataclass
import hashlib
from pathlib import Path
import shutil
from zipfile import ZipFile

import numpy as np


@dataclass(frozen=True)
class GridData:
    values: np.ndarray
    sx: float = 0.0
    sy: float = 0.0
    dx: float = 1.0
    dy: float = 1.0


def write_ascii_grid(path, values, sx=0.0, sy=0.0, dx=1.0, dy=1.0):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    array = np.asarray(values, dtype=np.float32)
    if array.ndim != 2:
        raise ValueError("ASCII grid values must be a two-dimensional array.")
    ny, nx = array.shape
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(f"{nx} {ny}\n")
        stream.write(f"{float(sx):.9g} {float(sy):.9g}\n")
        stream.write(f"{float(dx):.9g} {float(dy):.9g}\n")
        np.savetxt(stream, array, fmt="%.8g")
    return path


def read_ascii_grid(path):
    path = Path(path)
    with path.open("r", encoding="utf-8") as stream:
        nx, ny = (int(value) for value in stream.readline().split())
        sx, sy = (float(value) for value in stream.readline().split())
        dx, dy = (float(value) for value in stream.readline().split())
        values = np.loadtxt(stream, dtype=np.float32)
    values = np.asarray(values, dtype=np.float32).reshape(ny, nx)
    return GridData(values=values, sx=sx, sy=sy, dx=dx, dy=dy)


def _npz_members(path):
    with ZipFile(path) as archive:
        return [name for name in archive.namelist() if name.lower().endswith(".npy")]


def _select_npz_member(path, key):
    members = _npz_members(path)
    by_key = {Path(name).stem: name for name in members}
    selected = key or (next(iter(by_key)) if len(by_key) == 1 else None)
    if selected is None:
        raise ValueError(f"NPZ contains multiple arrays; select one of: {sorted(by_key)}")
    if selected not in by_key:
        raise KeyError(f"Array key not found in NPZ: {selected}")
    return selected, by_key[selected]


def _stage_npz_member(path, key, staging_dir):
    path = Path(path)
    selected, member = _select_npz_member(path, key)
    stat = path.stat()
    identity = f"{path.resolve()}|{stat.st_size}|{stat.st_mtime_ns}|{selected}"
    suffix = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:16]
    staging_dir = Path(staging_dir)
    staging_dir.mkdir(parents=True, exist_ok=True)
    target = staging_dir / f"{path.stem}_{selected}_{suffix}.npy"
    if target.exists():
        return target
    partial = target.with_suffix(".npy.part")
    try:
        with ZipFile(path) as archive, archive.open(member, "r") as source, partial.open("wb") as output:
            shutil.copyfileobj(source, output, length=8 * 1024 * 1024)
        partial.replace(target)
    except Exception:
        partial.unlink(missing_ok=True)
        raise
    return target


def load_numpy_array(path, key=None, mmap_mode="r", staging_dir=None):
    path = Path(path)
    if path.suffix.lower() == ".npy":
        return np.load(path, mmap_mode=mmap_mode, allow_pickle=False)
    if path.suffix.lower() == ".npz":
        if staging_dir is not None:
            staged = _stage_npz_member(path, key, staging_dir)
            return np.load(staged, mmap_mode=mmap_mode, allow_pickle=False)
        archive = np.load(path, allow_pickle=False)
        names = list(archive.files)
        selected = key or (names[0] if len(names) == 1 else None)
        if selected is None:
            archive.close()
            raise ValueError(f"NPZ contains multiple arrays; select one of: {names}")
        if selected not in archive:
            archive.close()
            raise KeyError(f"Array key not found in NPZ: {selected}")
        result = np.asarray(archive[selected])
        archive.close()
        return result
    raise ValueError("Only .npy and .npz arrays are supported by this converter.")
