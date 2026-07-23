import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "生成分支归属差异证据.py"


def load_module():
    spec = importlib.util.spec_from_file_location(
        "branch_attribution_evidence", SCRIPT
    )
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def test_rank_prefers_connectivity_difference_then_ambiguity_and_low_confidence():
    module = load_module()
    rows = [
        {
            "cluster_id": "1",
            "differs_from_legacy": "0",
            "pairing_differs_from_legacy": "1",
            "ambiguous": "0",
            "confidence": "0.01",
            "min_grid_x": "0",
            "max_grid_x": "2",
            "min_grid_y": "0",
            "max_grid_y": "2",
            "legacy_pairs": "1-2",
            "new_pairs": "1-3",
        },
        {
            "cluster_id": "2",
            "differs_from_legacy": "1",
            "pairing_differs_from_legacy": "1",
            "ambiguous": "0",
            "confidence": "0.20",
            "min_grid_x": "0",
            "max_grid_x": "2",
            "min_grid_y": "0",
            "max_grid_y": "2",
            "legacy_pairs": "1-2",
            "new_pairs": "1-3",
        },
        {
            "cluster_id": "3",
            "differs_from_legacy": "1",
            "pairing_differs_from_legacy": "1",
            "ambiguous": "1",
            "confidence": "0.01",
            "min_grid_x": "0",
            "max_grid_x": "8",
            "min_grid_y": "0",
            "max_grid_y": "8",
            "legacy_pairs": "1-2;2-3",
            "new_pairs": "1-3",
        },
    ]

    ranked = module.rank_difference_rows(rows)

    assert [row["cluster_id"] for row in ranked] == ["3", "2"]
    assert ranked[0]["review_priority"] == "high"
    assert ranked[1]["review_priority"] in {"medium", "high"}


def test_pair_parser_returns_unique_branch_ids():
    module = load_module()

    assert module.branch_ids_from_pairs("8-16;16-23;23-8") == {8, 16, 23}
    assert module.branch_ids_from_pairs("") == set()


def test_figure_title_keeps_ambiguous_and_non_ambiguous_results_as_candidates():
    module = load_module()

    ambiguous = module.describe_difference_candidate(
        {
            "ambiguous": "1",
            "accepted_differs_from_legacy": "0",
        }
    )
    eligible = module.describe_difference_candidate(
        {
            "ambiguous": "0",
            "accepted_differs_from_legacy": "1",
        }
    )

    assert ambiguous == "根系连通歧义候选（不进入候选分组）"
    assert eligible == "根系连通差异候选（可进入受控实验）"


def test_rank_prefers_explicit_candidate_connectivity_field():
    module = load_module()
    rows = [
        {
            "cluster_id": "1",
            "candidate_differs_from_legacy": "0",
            "differs_from_legacy": "1",
            "ambiguous": "0",
            "confidence": "0.2",
            "min_grid_x": "0",
            "max_grid_x": "2",
            "min_grid_y": "0",
            "max_grid_y": "2",
            "legacy_pairs": "1-2",
            "new_pairs": "1-3",
        },
        {
            "cluster_id": "2",
            "candidate_differs_from_legacy": "1",
            "differs_from_legacy": "0",
            "ambiguous": "0",
            "confidence": "0.2",
            "min_grid_x": "0",
            "max_grid_x": "2",
            "min_grid_y": "0",
            "max_grid_y": "2",
            "legacy_pairs": "1-2",
            "new_pairs": "1-3",
        },
    ]

    ranked = module.rank_difference_rows(rows)

    assert [row["cluster_id"] for row in ranked] == ["2"]


def test_script_uses_publication_exports_and_no_dataset_specific_polygon_ids():
    text = SCRIPT.read_text(encoding="utf-8")

    assert "savefig" in text
    assert ".svg" in text
    assert ".pdf" in text
    assert "differs_from_legacy" in text
    assert "polygon 172" not in text.lower()
    assert "test.dat" not in text.lower()
