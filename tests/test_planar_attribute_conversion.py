# -*- coding: utf-8 -*-
import sys
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.attributes import (
    _best_lag_similarity,
    canonicalize_volume,
    extract_label_slice,
    planar_similarity_attribute,
)
from generalization_validation.io import load_numpy_array


def _faulted_volume():
    nx, ny, nz = 30, 26, 25
    z = np.linspace(0.0, 4.0 * np.pi, nz)
    base = np.sin(z) + 0.25 * np.sin(2.3 * z)
    shifted = np.sin(z + 1.1) + 0.25 * np.sin(2.3 * (z + 1.1))
    volume = np.empty((nx, ny, nz), dtype=np.float32)
    volume[:15, :, :] = base
    volume[15:, :, :] = shifted
    return volume


def test_planar_similarity_attribute_highlights_trace_discontinuity():
    attribute = planar_similarity_attribute(_faulted_volume(), sample_index=12, half_window=5)

    assert attribute.shape == (26, 30)
    assert np.isfinite(attribute).all()
    assert 0.0 <= float(attribute.min()) <= float(attribute.max()) <= 1.0
    boundary = float(attribute[:, 14:16].mean())
    background = float(np.concatenate([attribute[:, 3:8], attribute[:, 22:27]], axis=1).mean())
    assert boundary > background + 0.35


def test_axis_order_and_label_slice_are_explicit():
    canonical = _faulted_volume()
    stored = np.transpose(canonical, (2, 0, 1))
    restored = canonicalize_volume(stored, "sxy")
    assert restored.shape == canonical.shape
    assert np.allclose(restored, canonical)

    labels = np.zeros_like(canonical, dtype=np.uint8)
    labels[14:16, :, 12] = 1
    sliced = extract_label_slice(labels, sample_index=12, axis_order="xys")
    assert sliced.shape == (26, 30)
    assert int(sliced.sum()) == 52


def test_large_npz_can_be_staged_as_memory_mapped_npy(tmp_path):
    source = tmp_path / "volume.npz"
    expected = np.arange(8 * 9 * 10, dtype=np.float32).reshape(8, 9, 10)
    np.savez_compressed(source, seismic=expected)

    loaded = load_numpy_array(source, "seismic", staging_dir=tmp_path / "cache")

    assert isinstance(loaded, np.memmap)
    assert loaded.shape == expected.shape
    assert np.array_equal(loaded[:, :, 4:6], expected[:, :, 4:6])
    assert len(list((tmp_path / "cache").glob("*.npy"))) == 1


def test_lag_compensated_similarity_follows_dipping_reflector():
    sample = np.linspace(0.0, 5.0 * np.pi, 31, dtype=np.float32)
    first = (np.sin(sample) + 0.2 * np.sin(2.7 * sample))[None, :]
    second = np.zeros_like(first)
    second[:, 1:] = first[:, :-1]

    unsteered = _best_lag_similarity(first, second, max_vertical_lag=0)
    steered = _best_lag_similarity(first, second, max_vertical_lag=2)

    assert float(steered[0]) > 0.999
    assert float(steered[0]) > float(unsteered[0]) + 0.02
