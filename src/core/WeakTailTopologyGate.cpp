#include "WeakTailTopologyGate.h"

#include "MaskTopology.h"

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fpt {
namespace {

BinaryMask branchMaskWithTails(
    const BinaryMask& baseBranchMask,
    const BinaryMask& acceptedTailMask) {
    return unionMask(baseBranchMask, acceptedTailMask);
}

bool candidateTouchesMask(
    const WeakTailCandidate& candidate,
    const BinaryMask& mask) {
    for (const SkeletonPoint& point : candidate.cells) {
        if (mask.get(point.x, point.y)) {
            return true;
        }
    }
    return false;
}

BinaryMask withoutCandidate(
    const BinaryMask& acceptedTailMask,
    const WeakTailCandidate& candidate) {
    BinaryMask out = acceptedTailMask;
    for (const SkeletonPoint& point : candidate.cells) {
        out.set(point.x, point.y, false);
    }
    return out;
}

int activeCandidateCellCount(
    const BinaryMask& acceptedTailMask,
    const WeakTailCandidate& candidate) {
    int count = 0;
    for (const SkeletonPoint& point : candidate.cells) {
        count += acceptedTailMask.get(point.x, point.y) ? 1 : 0;
    }
    return count;
}

std::vector<SkeletonPoint> candidateCellsFromFreeTip(
    const WeakTailCandidate& candidate,
    const BinaryMask& baseBranchMask,
    const BinaryMask& acceptedTailMask) {
    const int width = baseBranchMask.width();
    const auto indexOf = [width](int x, int y) { return y * width + x; };
    std::vector<int> localIndex(
        static_cast<std::size_t>(width * baseBranchMask.height()), -1);
    std::vector<SkeletonPoint> activeCells;
    activeCells.reserve(candidate.cells.size());
    for (const SkeletonPoint& point : candidate.cells) {
        if (!acceptedTailMask.get(point.x, point.y)) {
            continue;
        }
        localIndex[static_cast<std::size_t>(indexOf(point.x, point.y))] =
            static_cast<int>(activeCells.size());
        activeCells.push_back(point);
    }
    if (activeCells.empty()) {
        return {};
    }

    std::vector<int> distance(activeCells.size(), -1);
    std::queue<int> queue;
    for (std::size_t i = 0; i < activeCells.size(); ++i) {
        const SkeletonPoint& point = activeCells[i];
        bool touchesBase = false;
        for (int oy = -1; oy <= 1 && !touchesBase; ++oy) {
            for (int ox = -1; ox <= 1; ++ox) {
                if ((ox != 0 || oy != 0) &&
                    baseBranchMask.get(point.x + ox, point.y + oy)) {
                    touchesBase = true;
                    break;
                }
            }
        }
        if (touchesBase) {
            distance[i] = 0;
            queue.push(static_cast<int>(i));
        }
    }
    if (queue.empty()) {
        return {};
    }

    while (!queue.empty()) {
        const int current = queue.front();
        queue.pop();
        const SkeletonPoint& point = activeCells[static_cast<std::size_t>(current)];
        for (int oy = -1; oy <= 1; ++oy) {
            for (int ox = -1; ox <= 1; ++ox) {
                if (ox == 0 && oy == 0) {
                    continue;
                }
                const int x = point.x + ox;
                const int y = point.y + oy;
                if (!baseBranchMask.inBounds(x, y)) {
                    continue;
                }
                const int next = localIndex[
                    static_cast<std::size_t>(indexOf(x, y))];
                if (next < 0 || distance[static_cast<std::size_t>(next)] >= 0) {
                    continue;
                }
                distance[static_cast<std::size_t>(next)] =
                    distance[static_cast<std::size_t>(current)] + 1;
                queue.push(next);
            }
        }
    }
    if (std::find(distance.begin(), distance.end(), -1) != distance.end()) {
        return {};
    }

    std::vector<int> order(activeCells.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<int>(i);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const int distanceA = distance[static_cast<std::size_t>(a)];
        const int distanceB = distance[static_cast<std::size_t>(b)];
        if (distanceA != distanceB) {
            return distanceA > distanceB;
        }
        const SkeletonPoint& pointA = activeCells[static_cast<std::size_t>(a)];
        const SkeletonPoint& pointB = activeCells[static_cast<std::size_t>(b)];
        if (pointA.y != pointB.y) {
            return pointA.y > pointB.y;
        }
        return pointA.x > pointB.x;
    });

    std::vector<SkeletonPoint> ordered;
    ordered.reserve(order.size());
    for (const int index : order) {
        ordered.push_back(activeCells[static_cast<std::size_t>(index)]);
    }
    return ordered;
}

BinaryMask buildCandidateBelt(
    const BinaryMask& baseBranchMask,
    const BinaryMask& observedSkeleton,
    const BinaryMask& attributeMask,
    const BinaryMask& acceptedWeakContinuityMask,
    const BinaryMask& acceptedTailMask,
    const FaultBeltBuildOptions& options) {
    return buildFaultBelt(
        branchMaskWithTails(baseBranchMask, acceptedTailMask),
        observedSkeleton,
        attributeMask,
        acceptedWeakContinuityMask,
        acceptedTailMask,
        options);
}

struct CandidateTrimTrial {
    BinaryMask tailMask;
    NewInactiveEnclosureAnalysis analysis;
    int removedCells = 0;
    bool improves = false;
};

CandidateTrimTrial trimFreeTipUntilTopologyImproves(
    const BinaryMask& baseBranchMask,
    const BinaryMask& observedSkeleton,
    const BinaryMask& attributeMask,
    const BinaryMask& acceptedWeakContinuityMask,
    const BinaryMask& baseBelt,
    const BinaryMask& acceptedTailMask,
    const WeakTailCandidate& candidate,
    const NewInactiveEnclosureAnalysis& current,
    const FaultBeltBuildOptions& beltOptions) {
    CandidateTrimTrial trial{
        acceptedTailMask,
        {BinaryMask(baseBranchMask.width(), baseBranchMask.height()),
         current.componentCount,
         current.cellCount},
        0,
        false};
    const std::vector<SkeletonPoint> trimOrder = candidateCellsFromFreeTip(
        candidate, baseBranchMask, acceptedTailMask);
    for (const SkeletonPoint& point : trimOrder) {
        trial.tailMask.set(point.x, point.y, false);
        ++trial.removedCells;
        trial.analysis = analyzeNewInactiveEnclosures(
            baseBelt,
            buildCandidateBelt(
                baseBranchMask,
                observedSkeleton,
                attributeMask,
                acceptedWeakContinuityMask,
                trial.tailMask,
                beltOptions));
        if (trial.analysis.componentCount < current.componentCount ||
            trial.analysis.cellCount < current.cellCount) {
            trial.improves = true;
            break;
        }
    }
    return trial;
}

} // namespace

void applyWeakTailTopologyGate(
    const BinaryMask& baseBranchMask,
    const BinaryMask& observedSkeleton,
    const BinaryMask& attributeMask,
    const BinaryMask& acceptedWeakContinuityMask,
    const WeakTailTopologyGateOptions& options,
    WeakTailResult& tails) {
    if (options.influencePadding < 0) {
        throw std::runtime_error(
            "Weak-tail topology influence padding must be non-negative.");
    }
    BinaryMask emptyTail(baseBranchMask.width(), baseBranchMask.height());
    const BinaryMask baseBelt = buildCandidateBelt(
        baseBranchMask,
        observedSkeleton,
        attributeMask,
        acceptedWeakContinuityMask,
        emptyTail,
        options.belt);

    BinaryMask acceptedTailMask = tails.acceptedMask;
    NewInactiveEnclosureAnalysis current = analyzeNewInactiveEnclosures(
        baseBelt,
        buildCandidateBelt(
            baseBranchMask,
            observedSkeleton,
            attributeMask,
            acceptedWeakContinuityMask,
            acceptedTailMask,
            options.belt));
    tails.stats.topologyInitialNewEnclosures = current.componentCount;
    tails.stats.topologyInitialNewEnclosureCells = current.cellCount;

    while (current.componentCount > 0) {
        const int influenceRadius = std::max(
            1,
            options.belt.beltRadius +
                options.belt.attributeEnvelopeRadius +
                options.belt.closeRadius +
                options.influencePadding);
        const BinaryMask influence = dilate(current.cells, influenceRadius);

        WeakTailCandidate* best = nullptr;
        BinaryMask bestTailMask = acceptedTailMask;
        NewInactiveEnclosureAnalysis bestAnalysis{
            BinaryMask(baseBranchMask.width(), baseBranchMask.height()),
            current.componentCount,
            current.cellCount};
        int bestComponentReduction = 0;
        int bestCellReduction = 0;
        int bestRemovedCells = 0;

        for (WeakTailCandidate& candidate : tails.candidates) {
            if (!candidate.accepted || !candidateTouchesMask(candidate, influence)) {
                continue;
            }
            ++tails.stats.topologyGateEvaluatedComponents;
            CandidateTrimTrial trim = trimFreeTipUntilTopologyImproves(
                baseBranchMask,
                observedSkeleton,
                attributeMask,
                acceptedWeakContinuityMask,
                baseBelt,
                acceptedTailMask,
                candidate,
                current,
                options.belt);
            BinaryMask trialTailMask = trim.improves
                ? std::move(trim.tailMask)
                : withoutCandidate(acceptedTailMask, candidate);
            NewInactiveEnclosureAnalysis trial = trim.improves
                ? std::move(trim.analysis)
                : analyzeNewInactiveEnclosures(
                    baseBelt,
                    buildCandidateBelt(
                        baseBranchMask,
                        observedSkeleton,
                        attributeMask,
                        acceptedWeakContinuityMask,
                        trialTailMask,
                        options.belt));
            const int removedCells = trim.improves
                ? trim.removedCells
                : activeCandidateCellCount(acceptedTailMask, candidate);
            candidate.topologyGateEvaluated = true;
            candidate.newEnclosuresBefore = current.componentCount;
            candidate.newEnclosuresWithoutCandidate = trial.componentCount;
            candidate.newEnclosureCellsBefore = current.cellCount;
            candidate.newEnclosureCellsWithoutCandidate = trial.cellCount;

            const int componentReduction =
                current.componentCount - trial.componentCount;
            const int cellReduction = current.cellCount - trial.cellCount;
            const bool better = componentReduction > bestComponentReduction ||
                (componentReduction == bestComponentReduction &&
                 cellReduction > bestCellReduction) ||
                (componentReduction == bestComponentReduction &&
                 cellReduction == bestCellReduction && best &&
                 removedCells < bestRemovedCells) ||
                (componentReduction == bestComponentReduction &&
                 cellReduction == bestCellReduction && best &&
                 removedCells == bestRemovedCells &&
                 candidate.id < best->id);
            if ((componentReduction > 0 || cellReduction > 0) &&
                (!best || better)) {
                best = &candidate;
                bestTailMask = std::move(trialTailMask);
                bestAnalysis = std::move(trial);
                bestComponentReduction = componentReduction;
                bestCellReduction = cellReduction;
                bestRemovedCells = removedCells;
            }
        }

        if (best) {
            acceptedTailMask = std::move(bestTailMask);
            const int remainingCells = activeCandidateCellCount(
                acceptedTailMask, *best);
            if (remainingCells > 0) {
                best->accepted = true;
                best->decision = "accepted_topology_trimmed";
                if (best->topologyTrimmedCells == 0) {
                    ++tails.stats.topologyTrimmedComponents;
                }
                best->topologyTrimmedCells += bestRemovedCells;
                tails.stats.topologyTrimmedCells += bestRemovedCells;
            } else {
                best->accepted = false;
                best->decision = "new_internal_hole_after_belt";
                ++tails.stats.topologyRejectedComponents;
                tails.stats.topologyRejectedCells += bestRemovedCells;
            }
            current = std::move(bestAnalysis);
            continue;
        }

        int fallbackRejected = 0;
        for (WeakTailCandidate& candidate : tails.candidates) {
            if (!candidate.accepted || !candidateTouchesMask(candidate, influence)) {
                continue;
            }
            const int removedCells = activeCandidateCellCount(
                acceptedTailMask, candidate);
            acceptedTailMask = withoutCandidate(acceptedTailMask, candidate);
            candidate.accepted = false;
            candidate.decision = "new_internal_hole_group_fallback";
            ++tails.stats.topologyRejectedComponents;
            ++tails.stats.topologyFallbackRejectedComponents;
            tails.stats.topologyRejectedCells += removedCells;
            ++fallbackRejected;
        }
        if (fallbackRejected == 0) {
            for (WeakTailCandidate& candidate : tails.candidates) {
                if (!candidate.accepted) {
                    continue;
                }
                const int removedCells = activeCandidateCellCount(
                    acceptedTailMask, candidate);
                acceptedTailMask = withoutCandidate(acceptedTailMask, candidate);
                candidate.accepted = false;
                candidate.decision = "new_internal_hole_global_fallback";
                ++tails.stats.topologyRejectedComponents;
                ++tails.stats.topologyFallbackRejectedComponents;
                tails.stats.topologyRejectedCells += removedCells;
            }
        }
        current = analyzeNewInactiveEnclosures(
            baseBelt,
            buildCandidateBelt(
                baseBranchMask,
                observedSkeleton,
                attributeMask,
                acceptedWeakContinuityMask,
                acceptedTailMask,
                options.belt));
        if (current.componentCount > 0 && acceptedTailMask.countActive() == 0) {
            throw std::runtime_error(
                "Weak-tail topology gate could not restore the base topology.");
        }
    }

    tails.acceptedMask = std::move(acceptedTailMask);
    tails.mask = branchMaskWithTails(baseBranchMask, tails.acceptedMask);
    tails.stats.acceptedComponents = 0;
    tails.stats.acceptedCells = tails.acceptedMask.countActive();
    for (const WeakTailCandidate& candidate : tails.candidates) {
        if (candidate.accepted) {
            ++tails.stats.acceptedComponents;
        }
    }
    tails.stats.topologyRemainingNewEnclosures = current.componentCount;
    tails.stats.topologyRemainingNewEnclosureCells = current.cellCount;
}

} // namespace fpt
