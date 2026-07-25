from __future__ import annotations

import draw_algorithm
import draw_architecture
import draw_delivery
import draw_quality
import draw_workflows


def main() -> None:
    draw_architecture.system_architecture()
    draw_architecture.layered_architecture()
    draw_architecture.decoupled_deployment()
    draw_architecture.data_contracts()

    draw_workflows.standalone_workflow()
    draw_workflows.geoeast_workflow()
    draw_workflows.layer_view_workflow()
    draw_workflows.guidance_states()

    draw_algorithm.algorithm_pipeline()
    draw_algorithm.segmentation_to_polygon()
    draw_algorithm.weak_response_gate()
    draw_algorithm.branch_attribution()
    draw_algorithm.root_antimerge()
    draw_algorithm.topology_gate()
    draw_algorithm.area_filter()

    draw_quality.quality_overview()
    draw_quality.performance_breakdown()
    draw_quality.verification_matrix()
    draw_quality.scoring_traceability()
    draw_quality.official_evidence_plate()

    draw_delivery.packaging_flow()
    draw_delivery.geoeast_roundtrip()
    draw_delivery.display_scope()


if __name__ == "__main__":
    main()

