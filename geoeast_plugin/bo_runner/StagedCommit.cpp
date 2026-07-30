#include "StagedCommit.h"

#include <exception>
#include <stdexcept>

namespace fpt_geoeast {
namespace {

std::string currentExceptionMessage() {
    try {
        throw;
    } catch (const std::exception& error) {
        return error.what();
    } catch (...) {
        return "unknown failure";
    }
}

void requireOperation(bool available, const char* name) {
    if (!available) {
        throw std::invalid_argument(std::string("Missing staged-commit operation: ") + name);
    }
}

} // namespace

StagedCommitResult runStagedCommit(
    const std::string& targetName,
    const std::string& stagingName,
    const StagedCommitOperations& operations) {
    requireOperation(static_cast<bool>(operations.writeStaging), "writeStaging");
    requireOperation(static_cast<bool>(operations.verifyStaging), "verifyStaging");
    requireOperation(static_cast<bool>(operations.replaceTarget), "replaceTarget");
    requireOperation(static_cast<bool>(operations.verifyTarget), "verifyTarget");
    requireOperation(static_cast<bool>(operations.rollbackTarget), "rollbackTarget");
    requireOperation(static_cast<bool>(operations.deleteStaging), "deleteStaging");
    if (targetName.empty() || stagingName.empty() || targetName == stagingName) {
        throw std::invalid_argument("Staged commit requires distinct non-empty target and staging names.");
    }

    StagedCommitResult result;
    try {
        result.stagingWriteStarted = true;
        operations.writeStaging(stagingName);
        operations.verifyStaging(stagingName);
        result.stagingVerified = true;

        result.targetReplaceStarted = true;
        operations.replaceTarget(targetName);
        operations.verifyTarget(targetName);
        result.targetVerified = true;
        result.committed = true;
    } catch (...) {
        result.failureMessage = currentExceptionMessage();
        if (result.targetReplaceStarted) {
            result.rollbackPerformed = true;
            try {
                operations.rollbackTarget(targetName);
                result.rollbackSucceeded = true;
            } catch (...) {
                result.rollbackFailureMessage = currentExceptionMessage();
            }
        }
    }

    if (result.stagingWriteStarted) {
        try {
            operations.deleteStaging(stagingName);
            result.stagingCleanupSucceeded = true;
        } catch (...) {
            result.cleanupFailureMessage = currentExceptionMessage();
            try {
                operations.deleteStaging(stagingName);
                result.stagingCleanupSucceeded = true;
                result.cleanupFailureMessage.clear();
            } catch (...) {
                result.cleanupFailureMessage = currentExceptionMessage();
            }
        }
    }
    return result;
}

} // namespace fpt_geoeast
