#include "MultiScaleEvidence.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fpt {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;

double clamp01(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::max(0.0, std::min(1.0, value));
}

struct ValueSample {
    double value = 0.0;
    bool finite = false;
};

ValueSample valueAt(const Grid2D& grid, int x, int y) {
    if (!grid.inBounds(x, y)) {
        return {};
    }
    const double value = grid.value(x, y);
    return {value, std::isfinite(value)};
}

ValueSample meanNeighborhood(const Grid2D& grid, int x, int y, int rx, int ry) {
    double sum = 0.0;
    int count = 0;
    for (int yy = y - ry; yy <= y + ry; ++yy) {
        for (int xx = x - rx; xx <= x + rx; ++xx) {
            const ValueSample sample = valueAt(grid, xx, yy);
            if (!sample.finite) {
                continue;
            }
            sum += sample.value;
            ++count;
        }
    }
    return count > 0 ? ValueSample{sum / static_cast<double>(count), true} : ValueSample{};
}

bool finiteDifferenceSample(
    const Grid2D& grid,
    int x,
    int y,
    int rx,
    int ry,
    double& ridge,
    double& contrast,
    double& confidence,
    double& orientationX,
    double& orientationY) {
    const ValueSample center = valueAt(grid, x, y);
    const ValueSample xp = valueAt(grid, x + rx, y);
    const ValueSample xm = valueAt(grid, x - rx, y);
    const ValueSample yp = valueAt(grid, x, y + ry);
    const ValueSample ym = valueAt(grid, x, y - ry);
    const ValueSample xyp = valueAt(grid, x + rx, y + ry);
    const ValueSample xym = valueAt(grid, x + rx, y - ry);
    const ValueSample xmy = valueAt(grid, x - rx, y + ry);
    const ValueSample xmm = valueAt(grid, x - rx, y - ry);
    if (!center.finite || !xp.finite || !xm.finite || !yp.finite || !ym.finite ||
        !xyp.finite || !xym.finite || !xmy.finite || !xmm.finite) {
        return false;
    }

    const double sx = static_cast<double>(rx) * grid.dx();
    const double sy = static_cast<double>(ry) * grid.dy();
    const double hxx = (xp.value - 2.0 * center.value + xm.value) / (sx * sx);
    const double hyy = (yp.value - 2.0 * center.value + ym.value) / (sy * sy);
    const double hxy = (xyp.value - xym.value - xmy.value + xmm.value) /
        (4.0 * sx * sy);

    const double trace = hxx + hyy;
    const double discriminant = std::sqrt(std::max(
        0.0, (hxx - hyy) * (hxx - hyy) + 4.0 * hxy * hxy));
    const double lambda1 = 0.5 * (trace + discriminant);
    const double lambda2 = 0.5 * (trace - discriminant);
    const double abs1 = std::abs(lambda1);
    const double abs2 = std::abs(lambda2);
    const double normalLambda = abs1 >= abs2 ? lambda1 : lambda2;
    const double tangentLambda = abs1 >= abs2 ? lambda2 : lambda1;

    // A stable eigenvector for the eigenvalue of the weaker curvature.
    double vx = hxy;
    double vy = tangentLambda - hxx;
    if (std::hypot(vx, vy) <= 1e-12) {
        vx = tangentLambda - hyy;
        vy = hxy;
    }
    if (std::hypot(vx, vy) <= 1e-12) {
        vx = 1.0;
        vy = 0.0;
    }
    const double directionLength = std::hypot(vx, vy);
    orientationX = vx / directionLength;
    orientationY = vy / directionLength;

    const double curvatureScale = abs1 + abs2 + 1e-12;
    ridge = clamp01(std::max(0.0, -normalLambda) / curvatureScale);
    const ValueSample neighborhood = meanNeighborhood(
        grid, x, y, std::max(1, rx), std::max(1, ry));
    const double localScale = std::abs(center.value) +
        (neighborhood.finite ? std::abs(neighborhood.value) : 0.0) + 1e-12;
    contrast = clamp01(std::max(0.0, center.value -
        (neighborhood.finite ? neighborhood.value : center.value)) / localScale * 2.0);
    const double anisotropy = clamp01(std::abs(normalLambda - tangentLambda) /
        (abs1 + abs2 + 1e-12));
    confidence = clamp01(0.5 * anisotropy + 0.5 * std::min(1.0, curvatureScale));
    return true;
}

std::vector<double> defaultScales(const Grid2D& grid) {
    const double base = std::min(grid.dx(), grid.dy());
    return {base, 2.0 * base, 4.0 * base};
}

double angleBetweenUndirected(double ax, double ay, double bx, double by) {
    const double first = std::hypot(ax, ay);
    const double second = std::hypot(bx, by);
    if (first <= 1e-12 || second <= 1e-12) {
        return 180.0;
    }
    const double cosine = std::max(-1.0, std::min(1.0,
        std::abs((ax * bx + ay * by) / (first * second))));
    return std::acos(cosine) * 180.0 / kPi;
}

bool localSkeletonDirection(
    const Grid2D& grid,
    const BinaryMask& skeleton,
    int x,
    int y,
    double& directionX,
    double& directionY) {
    if (!skeleton.get(x, y)) {
        return false;
    }
    double covarianceXX = 0.0;
    double covarianceXY = 0.0;
    double covarianceYY = 0.0;
    int count = 0;
    constexpr int kDirectionRadius = 2;
    for (int yy = y - kDirectionRadius; yy <= y + kDirectionRadius; ++yy) {
        for (int xx = x - kDirectionRadius; xx <= x + kDirectionRadius; ++xx) {
            if ((xx == x && yy == y) || !skeleton.get(xx, yy)) {
                continue;
            }
            const double offsetX = static_cast<double>(xx - x) * grid.dx();
            const double offsetY = static_cast<double>(yy - y) * grid.dy();
            covarianceXX += offsetX * offsetX;
            covarianceXY += offsetX * offsetY;
            covarianceYY += offsetY * offsetY;
            ++count;
        }
    }
    if (count < 2) {
        return false;
    }
    const double trace = covarianceXX + covarianceYY;
    const double discriminant = std::sqrt(std::max(
        0.0,
        (covarianceXX - covarianceYY) * (covarianceXX - covarianceYY) +
            4.0 * covarianceXY * covarianceXY));
    const double major = 0.5 * (trace + discriminant);
    const double minor = 0.5 * (trace - discriminant);
    if (major <= 1e-12 || (major - minor) / major < 0.10) {
        return false;
    }
    const double angle = 0.5 * std::atan2(
        2.0 * covarianceXY, covarianceXX - covarianceYY);
    directionX = std::cos(angle);
    directionY = std::sin(angle);
    return true;
}

} // namespace

void validateMultiScaleEvidenceOptions(const MultiScaleEvidenceOptions& options) {
    if (options.maximumSamples == 0) {
        throw std::runtime_error("Multi-scale evidence sample budget must be positive.");
    }
    if (!std::isfinite(options.minimumConfidence) ||
        options.minimumConfidence < 0.0 || options.minimumConfidence > 1.0 ||
        !std::isfinite(options.minimumRidgeResponse) ||
        options.minimumRidgeResponse < 0.0 || options.minimumRidgeResponse > 1.0) {
        throw std::runtime_error("Multi-scale evidence thresholds must be within [0,1].");
    }
    for (const double scale : options.scalesWorld) {
        if (!std::isfinite(scale) || scale <= 0.0) {
            throw std::runtime_error("Multi-scale evidence scales must be finite and positive.");
        }
    }
}

MultiScaleEvidenceResult computeMultiScaleEvidence(
    const Grid2D& grid,
    const BinaryMask* targetMask,
    const MultiScaleEvidenceOptions& options) {
    validateMultiScaleEvidenceOptions(options);
    if (targetMask && (targetMask->width() != grid.nx() || targetMask->height() != grid.ny())) {
        throw std::runtime_error("Multi-scale evidence target mask dimensions must match the grid.");
    }

    MultiScaleEvidenceResult result;
    const std::vector<double> scales = options.scalesWorld.empty()
        ? defaultScales(grid) : options.scalesWorld;
    const std::size_t targetCount = targetMask
        ? static_cast<std::size_t>(targetMask->countActive()) : grid.size();
    result.summary.targetCells = static_cast<int>(std::min<std::size_t>(
        targetCount, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    result.samples.reserve(std::min(targetCount, options.maximumSamples));

    double ridgeSum = 0.0;
    double confidenceSum = 0.0;
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            if (targetMask && !targetMask->get(x, y)) {
                continue;
            }
            if (result.samples.size() >= options.maximumSamples) {
                ++result.summary.truncatedCells;
                continue;
            }
            ++result.summary.evaluatedCells;
            MultiScaleEvidenceSample best;
            best.x = x;
            best.y = y;
            double bestScore = -1.0;
            bool sawFinite = false;
            for (const double scale : scales) {
                const int rx = std::max(1, static_cast<int>(std::llround(scale / grid.dx())));
                const int ry = std::max(1, static_cast<int>(std::llround(scale / grid.dy())));
                double ridge = 0.0;
                double contrast = 0.0;
                double confidence = 0.0;
                double orientationX = 1.0;
                double orientationY = 0.0;
                if (!finiteDifferenceSample(grid, x, y, rx, ry, ridge,
                                             contrast, confidence, orientationX, orientationY)) {
                    continue;
                }
                sawFinite = true;
                const double score = ridge * (0.5 + 0.5 * confidence) +
                    0.15 * contrast;
                if (score > bestScore + 1e-12 ||
                    (std::abs(score - bestScore) <= 1e-12 && scale < best.scaleWorld)) {
                    best.scaleWorld = scale;
                    best.ridgeResponse = ridge;
                    best.contrastResponse = contrast;
                    best.confidence = confidence;
                    best.orientationX = orientationX;
                    best.orientationY = orientationY;
                    bestScore = score;
                }
            }
            best.finite = sawFinite;
            best.confident = sawFinite &&
                best.confidence >= options.minimumConfidence &&
                best.ridgeResponse >= options.minimumRidgeResponse;
            if (!sawFinite) {
                ++result.summary.invalidNeighborhoods;
            } else {
                ++result.summary.finiteCells;
                ridgeSum += best.ridgeResponse;
                confidenceSum += best.confidence;
                result.summary.maximumRidgeResponse = std::max(
                    result.summary.maximumRidgeResponse, best.ridgeResponse);
                if (best.confident) {
                    ++result.summary.confidentCells;
                }
            }
            result.samples.push_back(best);
        }
    }
    if (result.summary.finiteCells > 0) {
        result.summary.meanRidgeResponse = ridgeSum /
            static_cast<double>(result.summary.finiteCells);
        result.summary.meanConfidence = confidenceSum /
            static_cast<double>(result.summary.finiteCells);
    }
    return result;
}

void evaluateMultiScaleEvidenceAgainstSkeleton(
    const Grid2D& grid,
    const BinaryMask& skeleton,
    double maximumAgreementAngleDegrees,
    MultiScaleEvidenceResult& result) {
    if (!std::isfinite(maximumAgreementAngleDegrees) ||
        maximumAgreementAngleDegrees < 0.0 || maximumAgreementAngleDegrees > 90.0) {
        throw std::runtime_error("Skeleton orientation agreement angle is out of range.");
    }
    double angleSum = 0.0;
    for (const MultiScaleEvidenceSample& sample : result.samples) {
        if (!skeleton.get(sample.x, sample.y) || !sample.finite) {
            continue;
        }
        double skeletonX = 0.0;
        double skeletonY = 0.0;
        if (!localSkeletonDirection(grid, skeleton, sample.x, sample.y,
                                    skeletonX, skeletonY)) {
            continue;
        }
        const double angle = angleBetweenUndirected(
            sample.orientationX, sample.orientationY, skeletonX, skeletonY);
        ++result.summary.skeletonCellsEvaluated;
        angleSum += angle;
        if (angle <= maximumAgreementAngleDegrees) {
            ++result.summary.skeletonOrientationAgreementCells;
        }
    }
    result.summary.meanSkeletonOrientationAngle =
        result.summary.skeletonCellsEvaluated > 0
        ? angleSum / static_cast<double>(result.summary.skeletonCellsEvaluated)
        : 180.0;
}

} // namespace fpt
