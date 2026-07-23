import json
from io import BytesIO
from pathlib import Path
from zipfile import ZipFile

import numpy as np
from PIL import Image
from scipy.ndimage import label

from .attributes import section_similarity_attribute
from .synthetic import GeneratedCase, save_case


def _png_names(archive, prefix):
    return {
        Path(name).name: name
        for name in archive.namelist()
        if name.startswith(prefix) and name.lower().endswith(".png")
    }


def _label_object_masks(rgb, minimum_component_cells=3):
    binary = np.any(np.asarray(rgb, dtype=np.uint8) < 250, axis=-1)
    components, count = label(binary, structure=np.ones((3, 3), dtype=np.uint8))
    masks = []
    for component_id in range(1, count + 1):
        mask = components == component_id
        if int(mask.sum()) >= int(minimum_component_cells):
            masks.append(mask)
    if not masks:
        masks = [binary]
    return np.stack(masks).astype(bool), binary


def prepare_cracks_auxiliary(
    images_zip,
    labels_zip,
    output_dir,
    annotator="expert",
    limit=None,
    half_window=5,
    minimum_component_cells=3,
):
    images_zip = Path(images_zip)
    labels_zip = Path(labels_zip)
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    cases = []
    with ZipFile(images_zip) as images, ZipFile(labels_zip) as labels:
        image_names = _png_names(images, "images/")
        label_names = _png_names(labels, f"Fault segmentations/{annotator}/")
        common = sorted(set(image_names) & set(label_names))
        if limit is not None:
            common = common[: int(limit)]
        if not common:
            raise ValueError(f"No common CRACKS sections found for annotator {annotator}.")
        for filename in common:
            section = np.asarray(Image.open(BytesIO(images.read(image_names[filename]))).convert("L"), dtype=np.float32)
            rgb = np.asarray(Image.open(BytesIO(labels.read(label_names[filename]))).convert("RGB"), dtype=np.uint8)
            if section.shape != rgb.shape[:2]:
                raise ValueError(f"CRACKS image/label shape mismatch for {filename}.")
            object_masks, centerline = _label_object_masks(rgb, minimum_component_cells)
            attribute = section_similarity_attribute(section, half_window=half_window)
            section_id = Path(filename).stem
            metadata = {
                "schema_version": 1,
                "seed": 0,
                "family": "cracks_expert_vertical_section",
                "section_id": section_id,
                "width": int(section.shape[1]),
                "height": int(section.shape[0]),
                "object_count": int(object_masks.shape[0]),
                "truth_source": f"CRACKS_{annotator}_annotation",
                "background_mode": "real_f3_seismic_section",
                "attribute_method": "horizontal_adjacent_trace_local_similarity_discontinuity",
                "direct_planar_input": False,
                "usage_boundary": "Auxiliary vertical-section evidence only; not direct planar contest evidence.",
            }
            case_dir = output_dir / section_id
            save_case(GeneratedCase(attribute, centerline, object_masks, metadata), case_dir)
            Image.fromarray(section.astype(np.uint8)).save(case_dir / "seismic_section.png")
            Image.fromarray(rgb).save(case_dir / "source_annotation.png")
            cases.append({"id": section_id, "path": section_id, **metadata})
    manifest = {
        "schema_version": 1,
        "benchmark_type": "real_vertical_seismic_section_auxiliary",
        "evidence_level": "real_data_auxiliary_not_planar",
        "real_data_claim": True,
        "direct_planar_evidence": False,
        "source_dataset": "CRACKS/F3 Netherlands",
        "annotator": annotator,
        "count": len(cases),
        "cases": cases,
    }
    (output_dir / "benchmark_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return manifest
