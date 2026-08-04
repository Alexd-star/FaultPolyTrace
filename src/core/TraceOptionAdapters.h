#pragma once

#include "AdaptiveBoundary.h"
#include "LocalPolygonQuality.h"
#include "MultiScaleEvidence.h"
#include "ScopedWeakBridgePolicy.h"
#include "SupportEnvelopeSeparator.h"
#include "WeakContinuityConnector.h"
#include "WeakTailRestorer.h"

namespace fpt {

struct TraceOptions;

LocalPolygonQualityOptions makeLocalPolygonQualityOptions(
    const TraceOptions& options);

WeakContinuityOptions makeWeakContinuityOptions(
    const TraceOptions& options);

SupportEnvelopeSeparatorOptions makeSupportEnvelopeSeparatorOptions(
    const TraceOptions& options,
    const Grid2D& grid);

ScopedWeakBridgeOptions makeScopedWeakBridgeOptions(
    const TraceOptions& options,
    const Grid2D& grid);

WeakTailOptions makeWeakTailOptions(
    const TraceOptions& options,
    bool requireEndpointPortOwnership);

MultiScaleEvidenceOptions makeMultiScaleEvidenceOptions(
    const TraceOptions& options);

AdaptiveBoundaryOptions makeAdaptiveBoundaryOptions(
    const TraceOptions& options,
    double supportThreshold,
    double weakSupportThreshold);

} // namespace fpt
