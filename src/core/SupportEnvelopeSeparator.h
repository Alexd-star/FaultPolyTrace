#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"

#include <string>
#include <vector>

namespace fpt {

struct SupportEnvelopeSeparatorOptions {
    bool enabled = false;
    double lowThreshold = 0.05;
    double supportRadiusWorld = 0.0;
    int minPartCells = 80;
    double minPartRatio = 0.08;
    int minRemovedCells = 6;
    double minLowRatio = 0.55;
    double maxRemovedRatio = 0.08;
    double minResultingPartPhysicalAspect = 1.5;
    int maxEvidenceRecords = 4096;
};

struct SupportEnvelopeDecision {
    int componentId = 0;
    bool accepted = false;
    std::string decision;
    int componentCells = 0;
    int lowCells = 0;
    double lowRatio = 0.0;
    int observedSupportComponents = 0;
    int observedMajorSupportComponents = 0;
    int candidateCutCells = 0;
    double removedRatio = 0.0;
    int resultingComponents = 0;
    int resultingMajorComponents = 0;
    double minimumResultingPartPhysicalAspect = 0.0;
};

struct SupportEnvelopeSeparatorStats {
    int evaluatedComponents = 0;
    int candidateCutCells = 0;
    int acceptedCutCells = 0;
    int rejectedCutCells = 0;
    int acceptedComponents = 0;
    int rejectedComponents = 0;
    int protectedCells = 0;
};

struct SupportEnvelopeSeparatorResult {
    BinaryMask committedCutMask;
    BinaryMask candidateCutMask;
    SupportEnvelopeSeparatorStats stats;
    std::vector<SupportEnvelopeDecision> decisions;
};

void validateSupportEnvelopeSeparatorOptions(
    const SupportEnvelopeSeparatorOptions& options);

SupportEnvelopeSeparatorResult separateUnsupportedEnvelopes(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const SupportEnvelopeSeparatorOptions& options,
    const BinaryMask* protectedMask = nullptr);

} // namespace fpt
