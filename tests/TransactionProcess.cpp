#include <ArduinoOcpp/Tasks/Transactions/TransactionProcess.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#include <array>

using namespace ArduinoOcpp;

TEST_CASE( "Transaction process - trivial" ) {

    TransactionProcess txProcess {1};

    SECTION("Trivial transaction process"){
        txProcess.evaluateProcessSteps();
        REQUIRE( !txProcess.existsActiveTrigger() );
        REQUIRE( txProcess.getState() == TxEnableState::Inactive );
    }

    SECTION("Trivial transaction process with outputs"){
        bool notified = false;
        txProcess.addEnableStep([&notified] (TxTrigger trigger) -> TxEnableState {
            REQUIRE( trigger == TxTrigger::Inactive );
            notified = true;
            return TxEnableState::Inactive;
        });
        txProcess.evaluateProcessSteps();
        REQUIRE( notified );
        REQUIRE( txProcess.getState() == TxEnableState::Inactive );
    }
}

TEST_CASE("Transaction process - inputs") {

    TransactionProcess txProcess {1};

    TxPrecondition precondition = TxPrecondition::Active;
    txProcess.addPrecondition([&precondition] () {
        return precondition;
    });

    TxTrigger trigger = TxTrigger::Active;
    txProcess.addTrigger([&trigger] () {
        return trigger;
    });

    TxEnableState enable = TxEnableState::Active;
    TxTrigger checkTrigger = TxTrigger::Active;
    txProcess.addEnableStep([&enable, &checkTrigger] (TxTrigger input) {
        REQUIRE(checkTrigger == input);
        return enable;
    });

    SECTION("All active") {
        txProcess.evaluateProcessSteps();
        REQUIRE(txProcess.getState() == TxEnableState::Active);
    }

    SECTION("Precondition") {
        precondition = TxPrecondition::Inactive;
        checkTrigger = TxTrigger::Inactive;
        txProcess.evaluateProcessSteps();
        REQUIRE( txProcess.getState() != TxEnableState::Active );
    }

    SECTION("Trigger") {
        trigger = TxTrigger::Inactive;
        checkTrigger = TxTrigger::Inactive;
        txProcess.evaluateProcessSteps();
        REQUIRE( txProcess.getState() != TxEnableState::Active );
    }

    SECTION("Output enable") {
        enable = TxEnableState::Inactive;
        txProcess.evaluateProcessSteps();
        REQUIRE( txProcess.getState() == TxEnableState::Pending );

        enable = TxEnableState::Pending;
        txProcess.evaluateProcessSteps();
        REQUIRE( txProcess.getState() == TxEnableState::Pending );
    }
}

TEST_CASE("Transaction process - execution order") {

    TransactionProcess txProcess {1};

    std::array<TxPrecondition, 2> preconditions {TxPrecondition::Active, TxPrecondition::Active};
    for (unsigned int i = 0; i < preconditions.size(); i++) {
        txProcess.addPrecondition([&preconditions, i] () {
            return preconditions[i];
        });
    }
    
    std::array<TxTrigger, 2> triggers {TxTrigger::Active, TxTrigger::Active};
    for (unsigned int i = 0; i < triggers.size(); i++) {
        txProcess.addTrigger([&triggers, i] () {
            return triggers[i];
        });
    }
    
    std::array<TxEnableState, 2> enableSteps {TxEnableState::Active, TxEnableState::Active};
    std::array<int, 2> checkSeq {-1, -1};
    int checkCount = 0;
    for (unsigned int i = 0; i < enableSteps.size(); i++) {
        txProcess.addEnableStep([&enableSteps, &checkSeq, &checkCount, i] (TxTrigger) {
            checkSeq[i] = checkCount;
            checkCount++;
            return enableSteps[i];
        });
    }
    
    SECTION("Precondition"){
        preconditions[0] = TxPrecondition::Inactive;
        txProcess.evaluateProcessSteps();
        REQUIRE( txProcess.getState() != TxEnableState::Active );

        preconditions[0] = TxPrecondition::Active;
        preconditions[1] = TxPrecondition::Inactive;
        txProcess.evaluateProcessSteps();
        REQUIRE( txProcess.getState() != TxEnableState::Active );
    }

    SECTION("Trigger") {
        triggers[0] = TxTrigger::Inactive;
        txProcess.evaluateProcessSteps();
        REQUIRE( txProcess.getState() != TxEnableState::Active );
        REQUIRE( txProcess.existsActiveTrigger());

        triggers[0] = TxTrigger::Active;
        triggers[1] = TxTrigger::Inactive;
        txProcess.evaluateProcessSteps();
        REQUIRE( txProcess.getState() != TxEnableState::Active );
        REQUIRE( txProcess.existsActiveTrigger());
    }

    SECTION("Output enable - all active"){
        txProcess.evaluateProcessSteps();
        REQUIRE(checkSeq[0] == 1);
        REQUIRE(checkSeq[1] == 0);
    }

    SECTION("Output enable - active pending"){
        enableSteps[1] = TxEnableState::Pending;
        txProcess.evaluateProcessSteps();
        REQUIRE(checkSeq[0] == -1);
        REQUIRE(checkSeq[1] == 0);
    }

    SECTION("Output enable - inactive"){
        preconditions[0] = TxPrecondition::Inactive;
        enableSteps[0] = TxEnableState::Inactive;
        txProcess.evaluateProcessSteps();
        REQUIRE(checkSeq[0] == 0);
        REQUIRE(checkSeq[1] == 1);
    }

    SECTION("Output enable - inactive pending"){
        preconditions[0] = TxPrecondition::Inactive;
        enableSteps[0] = TxEnableState::Pending;
        txProcess.evaluateProcessSteps();
        REQUIRE(checkSeq[0] == 0);
        REQUIRE(checkSeq[1] == -1);
    }
}

TEST_CASE("Transaction process - begin new transaction") {
    TransactionProcess txProcess {1};

    txProcess.addPrecondition([] () {
        return TxPrecondition::Active;
    });

    TxTrigger trigger = TxTrigger::Active;
    txProcess.addTrigger([&trigger] () {
        return trigger;
    });

    TxEnableState enable = TxEnableState::Active;
    TxTrigger checkTrigger = TxTrigger::Active;
    txProcess.addEnableStep([&enable, &checkTrigger] (TxTrigger input) {
        checkTrigger = input;
        return enable;
    });

}
