#pragma once

#include <functional>
#include <string>

namespace fpt_geoeast {

struct StagedCommitOperations {
    std::function<void(const std::string&)> writeStaging;
    std::function<void(const std::string&)> verifyStaging;
    std::function<void(const std::string&)> replaceTarget;
    std::function<void(const std::string&)> verifyTarget;
    std::function<void(const std::string&)> rollbackTarget;
    std::function<void(const std::string&)> deleteStaging;
};

struct StagedCommitResult {
    bool stagingWriteStarted = false;
    bool stagingVerified = false;
    bool targetReplaceStarted = false;
    bool targetVerified = false;
    bool rollbackPerformed = false;
    bool rollbackSucceeded = false;
    bool stagingCleanupSucceeded = false;
    bool committed = false;
    std::string failureMessage;
    std::string rollbackFailureMessage;
    std::string cleanupFailureMessage;

    bool success() const {
        return committed && targetVerified && stagingCleanupSucceeded;
    }
};

StagedCommitResult runStagedCommit(
    const std::string& targetName,
    const std::string& stagingName,
    const StagedCommitOperations& operations);

} // namespace fpt_geoeast
