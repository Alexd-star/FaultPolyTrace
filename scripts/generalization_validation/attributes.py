import numpy as np
from scipy.ndimage import gaussian_filter, uniform_filter1d


def canonicalize_volume(volume, axis_order="xys"):
    order = axis_order.lower().strip()
    if len(order) != 3 or set(order) != {"x", "y", "s"}:
        raise ValueError("axis_order must contain x, y and s exactly once, for example xys or sxy.")
    array = np.asarray(volume)
    if array.ndim != 3:
        raise ValueError("Seismic and label volumes must be three-dimensional.")
    axes = tuple(order.index(name) for name in "xys")
    return np.transpose(array, axes)


def robust_normalize(values, lower_percentile=1.0, upper_percentile=99.0):
    array = np.asarray(values, dtype=np.float32)
    finite = np.isfinite(array)
    if not finite.any():
        return np.zeros_like(array, dtype=np.float32)
    low, high = np.percentile(array[finite], [lower_percentile, upper_percentile])
    if high <= low + 1e-12:
        result = np.zeros_like(array, dtype=np.float32)
        result[finite] = np.clip(array[finite], 0.0, 1.0)
        return result
    result = (np.where(finite, array, low) - low) / (high - low)
    return np.clip(result, 0.0, 1.0).astype(np.float32)


def _normalized_similarity(first, second, epsilon=1e-8):
    a = first.astype(np.float64, copy=False)
    b = second.astype(np.float64, copy=False)
    a = a - a.mean(axis=-1, keepdims=True)
    b = b - b.mean(axis=-1, keepdims=True)
    numerator = np.sum(a * b, axis=-1)
    denominator = np.sqrt(np.sum(a * a, axis=-1) * np.sum(b * b, axis=-1))
    correlation = np.ones_like(numerator)
    valid = denominator > epsilon
    correlation[valid] = numerator[valid] / denominator[valid]
    return np.clip((correlation + 1.0) * 0.5, 0.0, 1.0)


def _best_lag_similarity(first, second, max_vertical_lag=0):
    first = np.asarray(first)
    second = np.asarray(second)
    if first.shape != second.shape or first.ndim < 1:
        raise ValueError("Similarity inputs must have the same shape.")
    maximum = int(max_vertical_lag)
    if maximum < 0 or first.shape[-1] <= 2 * maximum + 2:
        raise ValueError("max_vertical_lag must be non-negative and fit inside the sample window.")
    best = None
    for lag in range(-maximum, maximum + 1):
        if lag < 0:
            similarity = _normalized_similarity(first[..., -lag:], second[..., :lag])
        elif lag > 0:
            similarity = _normalized_similarity(first[..., :-lag], second[..., lag:])
        else:
            similarity = _normalized_similarity(first, second)
        best = similarity if best is None else np.maximum(best, similarity)
    return best


def planar_similarity_attribute(
    volume,
    sample_index,
    half_window=5,
    stepout=1,
    axis_order="xys",
    smoothing_sigma=0.65,
    max_vertical_lag=0,
):
    canonical = canonicalize_volume(volume, axis_order)
    nx, ny, ns = canonical.shape
    if not 0 <= int(sample_index) < ns:
        raise IndexError(f"sample_index {sample_index} is outside [0, {ns - 1}].")
    if half_window < 1 or stepout < 1 or stepout >= min(nx, ny):
        raise ValueError("half_window and stepout must be positive and fit inside the volume.")
    lag = int(max_vertical_lag)
    lo = max(0, int(sample_index) - int(half_window) - lag)
    hi = min(ns, int(sample_index) + int(half_window) + lag + 1)
    traces = np.asarray(canonical[:, :, lo:hi], dtype=np.float32)
    if traces.shape[-1] < 3:
        raise ValueError("The selected vertical window must contain at least three samples.")

    similarity_x = np.ones((nx, ny), dtype=np.float64)
    pair_x = _best_lag_similarity(
        traces[:-stepout, :, :], traces[stepout:, :, :], lag
    )
    similarity_x[:-stepout, :] = np.minimum(similarity_x[:-stepout, :], pair_x)
    similarity_x[stepout:, :] = np.minimum(similarity_x[stepout:, :], pair_x)

    similarity_y = np.ones((nx, ny), dtype=np.float64)
    pair_y = _best_lag_similarity(
        traces[:, :-stepout, :], traces[:, stepout:, :], lag
    )
    similarity_y[:, :-stepout] = np.minimum(similarity_y[:, :-stepout], pair_y)
    similarity_y[:, stepout:] = np.minimum(similarity_y[:, stepout:], pair_y)

    discontinuity = 1.0 - np.minimum(similarity_x, similarity_y)
    if smoothing_sigma > 0:
        discontinuity = gaussian_filter(discontinuity, sigma=float(smoothing_sigma), mode="nearest")
    # Grid files store rows as Y and columns as X.
    return robust_normalize(discontinuity.T)


def extract_label_slice(labels, sample_index, axis_order="xys", threshold=0.5):
    canonical = canonicalize_volume(labels, axis_order)
    if not 0 <= int(sample_index) < canonical.shape[2]:
        raise IndexError("sample_index is outside the label volume.")
    return (np.asarray(canonical[:, :, int(sample_index)]).T >= float(threshold))


def section_similarity_attribute(image, half_window=5, horizontal_stepout=1, smoothing_sigma=0.65):
    section = robust_normalize(np.asarray(image, dtype=np.float32))
    if section.ndim != 2:
        raise ValueError("A seismic section must be a two-dimensional image.")
    _, width = section.shape
    stepout = int(horizontal_stepout)
    if half_window < 1 or stepout < 1 or stepout >= width:
        raise ValueError("half_window and horizontal_stepout must be positive and fit the section.")
    first = section[:, :-stepout]
    second = section[:, stepout:]
    size = int(half_window) * 2 + 1
    mean_first = uniform_filter1d(first, size=size, axis=0, mode="nearest")
    mean_second = uniform_filter1d(second, size=size, axis=0, mode="nearest")
    covariance = uniform_filter1d(first * second, size=size, axis=0, mode="nearest") - mean_first * mean_second
    variance_first = uniform_filter1d(first * first, size=size, axis=0, mode="nearest") - mean_first * mean_first
    variance_second = uniform_filter1d(second * second, size=size, axis=0, mode="nearest") - mean_second * mean_second
    denominator = np.sqrt(np.maximum(variance_first, 0.0) * np.maximum(variance_second, 0.0))
    correlation = np.ones_like(covariance)
    valid = denominator > 1e-8
    correlation[valid] = covariance[valid] / denominator[valid]
    pair_discontinuity = 1.0 - np.clip((correlation + 1.0) * 0.5, 0.0, 1.0)
    discontinuity = np.zeros_like(section)
    discontinuity[:, :-stepout] = np.maximum(discontinuity[:, :-stepout], pair_discontinuity)
    discontinuity[:, stepout:] = np.maximum(discontinuity[:, stepout:], pair_discontinuity)
    if smoothing_sigma > 0:
        discontinuity = gaussian_filter(discontinuity, sigma=float(smoothing_sigma), mode="nearest")
    return robust_normalize(discontinuity)
