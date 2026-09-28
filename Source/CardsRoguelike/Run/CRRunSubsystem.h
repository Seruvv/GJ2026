// Owns the current run. Lives on the GameInstance, so it survives map changes (run map <-> rooms).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CRRunTypes.h"
#include "CRRunSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FCROnRunStateChanged);

UCLASS()
class CARDSROGUELIKE_API UCRRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Starts the deterministic test run: test hamster, starter deck, fixed graph. */
	void StartPrototypeRun();
	void AbandonRun();

	ECRRunStatus GetStatus() const { return RunState.Status; }
	bool IsRunActive() const { return RunState.Status == ECRRunStatus::Active; }
	/** True once a run exists, including a failed or finished one. */
	bool HasRun() const { return RunState.Status != ECRRunStatus::NotStarted; }

	const FCRRunState& GetRunState() const { return RunState; }
	const FCRRunNodeData* FindNode(FName NodeId) const;
	const FCRRunNodeData* GetCurrentNode() const { return FindNode(RunState.CurrentNodeId); }

	/** True while the hamster stands in an unresolved Combat room of an active run. */
	bool IsInCombatRoom() const;

	/** Only Available nodes can be entered; there is no backtracking. */
	bool CanTravelTo(FName NodeId) const;

	/**
	 * Moves the hamster into an Available room. Combat rooms stay unresolved until
	 * CompleteCurrentRoom(); placeholder rooms (shop, event, boss, return) resolve immediately.
	 */
	bool EnterNode(FName NodeId);

	/** Resolves the current room: it becomes Completed and its outgoing rooms Available. */
	bool CompleteCurrentRoom();

	/** Ends the run as failed. The state is kept for inspection; nothing restarts automatically. */
	void FailCurrentRun();

	void SetHamsterHP(int32 CurrentHP);

	/** Appends a card id to the run deck (duplicates allowed). */
	void AddCardToDeck(FName CardId);

	/** Adds resources carried this run; totals never go below zero. */
	void AddCarriedResources(int32 Silver, int32 Food, int32 Wood);

	/** Broadcast whenever the run state changes. */
	FCROnRunStateChanged OnRunStateChanged;

private:
	void BuildPrototypeGraph();
	void RefreshNodeStates();

	/** Read-only in the editor/MCP for debugging during PIE. */
	UPROPERTY(VisibleInstanceOnly, Category = "CR|Run")
	FCRRunState RunState;
};
