# -*- coding: utf-8 -*-
import json
import sys
from pathlib import Path
from zipfile import ZipFile

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.attributes import section_similarity_attribute
from generalization_validation.cracks import prepare_cracks_auxiliary


def test_section_similarity_returns_normalized_discontinuity():
    y = np.linspace(0, 8 * np.pi, 80)
    image = np.empty((80, 60), dtype=np.float32)
    image[:, :30] = np.sin(y)[:, None]
    image[:, 30:] = np.sin(y + 1.2)[:, None]
    attribute = section_similarity_attribute(image, half_window=4)
    assert attribute.shape == image.shape
    assert 0.0 <= float(attribute.min()) <= float(attribute.max()) <= 1.0
    assert float(attribute[:, 29:31].mean()) > float(attribute[:, 8:15].mean()) + 0.3


def test_cracks_adapter_marks_vertical_sections_as_auxiliary(tmp_path):
    image = np.tile(np.linspace(20, 230, 64, dtype=np.uint8), (48, 1))
    annotation = np.full((48, 64, 3), 255, dtype=np.uint8)
    annotation[5:42, 31, :] = np.array([31, 119, 180], dtype=np.uint8)
    image_path = tmp_path / "section_001.png"
    label_path = tmp_path / "label.png"
    Image.fromarray(image).save(image_path)
    Image.fromarray(annotation).save(label_path)
    images_zip = tmp_path / "images.zip"
    labels_zip = tmp_path / "labels.zip"
    with ZipFile(images_zip, "w") as archive:
        archive.write(image_path, "images/section_001.png")
    with ZipFile(labels_zip, "w") as archive:
        archive.write(label_path, "Fault segmentations/expert/section_001.png")

    output = tmp_path / "prepared"
    manifest = prepare_cracks_auxiliary(images_zip, labels_zip, output, annotator="expert")
    assert manifest["count"] == 1
    assert manifest["real_data_claim"] is True
    assert manifest["direct_planar_evidence"] is False
    metadata = json.loads((output / "section_001" / "metadata.json").read_text(encoding="utf-8"))
    assert metadata["usage_boundary"].startswith("Auxiliary vertical-section")
    assert (output / "section_001" / "attribute.txt").exists()
