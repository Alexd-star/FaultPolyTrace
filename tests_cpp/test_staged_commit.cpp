#include "../geoeast_plugin/bo_runner/StagedCommit.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct FakeStore {
    std::unordered_map<std::string, std::string> objects;
    std::string candidate = "candidate";
    bool failStageWrite = false;
    bool failTargetWrite = false;
    bool failTargetVerify = false;
    bool failRollback = false;
    int cleanupFailuresRemaining = 0;
};

fpt_geoeast::StagedCommitOperations operationsFor(FakeStore& store) {
    std::string previous;
    const auto old = store.objects.find("result");
    const bool hadPrevious = old != store.objects.end();
    if (hadPrevious) previous = old->second;

    fpt_geoeast::StagedCommitOperations operations;
    operations.writeStaging = [&](const std::string& name) {
        store.objects[name] = "partial-stage";
        if (store.failStageWrite) throw std::runtime_error("stage write failed");
        store.objects[name] = store.candidate;
    };
    operations.verifyStaging = [&](const std::string& name) {
        if (store.objects.at(name) != store.candidate) throw std::runtime_error("stage mismatch");
    };
    operations.replaceTarget = [&](const std::string& name) {
        store.objects[name] = "partial-target";
        if (store.failTargetWrite) throw std::runtime_error("target write failed");
        store.objects[name] = store.candidate;
    };
    operations.verifyTarget = [&](const std::string& name) {
        if (store.failTargetVerify || store.objects.at(name) != store.candidate) {
            throw std::runtime_error("target mismatch");
        }
    };
    operations.rollbackTarget = [&, previous, hadPrevious](const std::string& name) {
        if (store.failRollback) throw std::runtime_error("rollback failed");
        if (hadPrevious) store.objects[name] = previous;
        else store.objects.erase(name);
    };
    operations.deleteStaging = [&](const std::string& name) {
        if (store.cleanupFailuresRemaining-- > 0) throw std::runtime_error("cleanup failed");
        store.objects.erase(name);
    };
    return operations;
}

void testSuccessfulCommit() {
    FakeStore store;
    store.objects["result"] = "old";
    const auto result = fpt_geoeast::runStagedCommit(
        "result", "stage", operationsFor(store));
    require(result.success(), "A verified staged write must commit.");
    require(store.objects.at("result") == "candidate", "The final object must contain the candidate.");
    require(store.objects.count("stage") == 0, "The staging object must be removed.");
    require(!result.rollbackPerformed, "A successful commit must not roll back.");
}

void testStageFailureDoesNotTouchTarget() {
    FakeStore store;
    store.objects["result"] = "old";
    store.failStageWrite = true;
    const auto result = fpt_geoeast::runStagedCommit(
        "result", "stage", operationsFor(store));
    require(!result.success(), "A failed staging write must fail the commit.");
    require(store.objects.at("result") == "old", "A staging failure must preserve the target.");
    require(!result.rollbackPerformed, "No target rollback is needed before replacement starts.");
    require(store.objects.count("stage") == 0, "A partial staging object must be removed.");
}

void testTargetFailureRestoresExistingObject() {
    FakeStore store;
    store.objects["result"] = "old";
    store.failTargetWrite = true;
    const auto result = fpt_geoeast::runStagedCommit(
        "result", "stage", operationsFor(store));
    require(!result.success(), "A target write failure must fail the commit.");
    require(result.rollbackPerformed && result.rollbackSucceeded,
            "A changed target must be restored.");
    require(store.objects.at("result") == "old", "The previous object must be restored exactly.");
}

void testReadbackMismatchRemovesNewTargetWhenNoPreviousObject() {
    FakeStore store;
    store.failTargetVerify = true;
    const auto result = fpt_geoeast::runStagedCommit(
        "result", "stage", operationsFor(store));
    require(!result.success(), "A readback mismatch must fail the commit.");
    require(result.rollbackSucceeded, "Rollback must remove a newly created invalid target.");
    require(store.objects.count("result") == 0, "No invalid final object may remain.");
}

void testRollbackFailureIsReported() {
    FakeStore store;
    store.objects["result"] = "old";
    store.failTargetVerify = true;
    store.failRollback = true;
    const auto result = fpt_geoeast::runStagedCommit(
        "result", "stage", operationsFor(store));
    require(!result.success(), "Rollback failure must never be reported as success.");
    require(result.rollbackPerformed && !result.rollbackSucceeded,
            "Rollback failure status must be explicit.");
    require(!result.rollbackFailureMessage.empty(), "Rollback failure details must be retained.");
}

void testCleanupRetriesOnce() {
    FakeStore store;
    store.cleanupFailuresRemaining = 1;
    const auto result = fpt_geoeast::runStagedCommit(
        "result", "stage", operationsFor(store));
    require(result.success(), "A transient cleanup failure should recover on one retry.");
    require(store.objects.count("stage") == 0, "Retry must remove the staging object.");
}

} // namespace

int main() {
    try {
        testSuccessfulCommit();
        testStageFailureDoesNotTouchTarget();
        testTargetFailureRestoresExistingObject();
        testReadbackMismatchRemovesNewTargetWhenNoPreviousObject();
        testRollbackFailureIsReported();
        testCleanupRetriesOnce();
        std::cout << "test_staged_commit: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_staged_commit: FAIL: " << error.what() << "\n";
        return 1;
    }
}
