// Owns the current run. Lives on the GameInstance, so it survives map changes (run map <-> rooms).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CRRunTypes.h"
#include "CRRunSubsystem.generated.h"

class UCREventDefinition;
struct FCREventChoice;

DECLARE_MULTICAST_DELEGATE(FCROnRunStateChanged);
/** Fired exactly once when a run ends (Completed, Failed or Abandoned), with the run as it ended. */
DECLARE_MULTICAST_DELEGATE_OneParam(FCROnRunEnded, const FCRRunState& /*EndedRun*/);

UCLASS()
class CARDSROGUELIKE_API UCRRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Starts a new run (test hamster, starter deck) on a freshly generated map from a new random seed. */
	void StartFreshRun();

	/** Development/repro API: a fresh run whose graph is generated from Seed (same seed = same map). */
	void StartFreshRunWithSeed(int32 Seed);

	/** Starts a new run for a profile (from the hub): random seed, the selected hamster and the hub bonuses. */
	void StartProfileRun(const FCRRunStartConfig& Config);

	/** Clears the run. A run still in progress is reported as Abandoned first. */
	void AbandonRun();

	/** True if the run was started from a profile (its end is delivered to that profile). */
	bool IsProfileRun() const { return !RunState.ProfileId.IsEmpty(); }

	ECRRunStatus GetStatus() const { return RunState.Status; }
	bool IsRunActive() const { return RunState.Status == ECRRunStatus::Active; }
	/** True once a run exists, including a failed or finished one. */
	bool HasRun() const { return RunState.Status != ECRRunStatus::NotStarted; }

	const FCRRunState& GetRunState() const { return RunState; }
	const FCRRunNodeData* FindNode(FName NodeId) const;
	const FCRRunNodeData* GetCurrentNode() const { return FindNode(RunState.CurrentNodeId); }

	/** True while the hamster stands in an unresolved Combat room of an active run. */
	bool IsInCombatRoom() const;

	/** True while the hamster stands in an unresolved Shop room of an active run. */
	bool IsInShopRoom() const;

	// Shops (state per shop node, persistent for the run)

	/** Creates the shop's offers and merchant line on first call; later calls return the same state. */
	const FCRShopState* EnsureShopState(FName ShopNodeId, int32 OfferCount);
	const FCRShopState* FindShopState(FName ShopNodeId) const;

	/** Buys an offer in the current (unresolved) shop: spends Silver, appends the card, marks it sold. */
	bool BuyShopCard(FName ShopNodeId, int32 OfferIndex, int32 SilverPrice);

	/** Buys the shop's one heal: spends Silver, heals clamped to MaxHP, marks it sold. */
	bool BuyShopHeal(FName ShopNodeId, int32 SilverPrice, int32 HealAmount);

	// Events (state per event node, persistent for the run)

	/** True while the hamster stands in an unresolved Event room of an active run. */
	bool IsInEventRoom() const;

	/** Draws the node's event from its pool on first call; later calls return the same state. Null if no event could be drawn. */
	const FCREventNodeState* EnsureEventState(FName EventNodeId);
	const FCREventNodeState* FindEventState(FName EventNodeId) const;

	/** Why the choice cannot be taken right now ("Need 2 Silver"), or empty if it can. */
	FString GetEventChoiceBlockReason(const FCREventChoice& Choice) const;

	/** True if the choice needs the player to pick a deck card before it can commit. */
	static bool ChoiceNeedsCardSelection(const FCREventChoice& Choice);

	/**
	 * Commits a choice of the current (unresolved) event: validates every effect, then applies all of
	 * them exactly once and records the result. SacrificeDeckIndex selects the card for RemoveSelectedCard.
	 * Returns false (changing nothing) if the event already committed or the choice is not payable.
	 */
	bool CommitEventChoice(FName EventNodeId, const UCREventDefinition* Event, int32 ChoiceIndex, int32 SacrificeDeckIndex);

	/**
	 * Completes the current event room after its result was shown. Only valid once a choice is committed and
	 * the hamster survived it (a lethal choice already failed the run inside CommitEventChoice).
	 */
	bool ContinueFromEvent(FName EventNodeId);

	/** Only Available nodes can be entered; there is no backtracking. */
	bool CanTravelTo(FName NodeId) const;

	/**
	 * Moves the hamster into an Available room. Combat, Shop and Event rooms stay unresolved until
	 * their map calls CompleteCurrentRoom(); placeholder rooms (boss, return) resolve immediately.
	 */
	bool EnterNode(FName NodeId);

	/** Resolves the current room: it becomes Completed and its outgoing rooms Available. */
	bool CompleteCurrentRoom();

	/** Ends the run as failed (the hamster died of Cause). The state is kept for inspection; nothing restarts automatically. */
	void FailCurrentRun(ECRHamsterDeathCause Cause = ECRHamsterDeathCause::Combat);

	/** True once the run failed (the hamster is dead). */
	bool IsRunFailed() const { return RunState.Status == ECRRunStatus::Failed; }

	void SetHamsterHP(int32 CurrentHP);

	/** Appends a card id to the run deck (duplicates allowed). */
	void AddCardToDeck(FName CardId);

	/** Adds resources carried this run; totals never go below zero. */
	void AddCarriedResources(int32 Silver, int32 Food, int32 Wood);

	/** Broadcast whenever the run state changes. */
	FCROnRunStateChanged OnRunStateChanged;

	FCROnRunEnded OnRunEnded;

#if WITH_DEV_AUTOMATION_TESTS
	/** Automation tests only: direct access to set up a room situation without playing to it. */
	FCRRunState& GetMutableRunStateForTests() { return RunState; }
#endif

private:
	/** Picks the seed for a normal new run (outside the graph stream). */
	static int32 MakeRandomRunSeed();
	void StartRun(int32 Seed, const FCRRunStartConfig& Config);
	void RefreshNodeStates();
	void ReportRunEnd(ECRRunEndReason Reason);

	/** Mutable state of a shop the hamster is currently standing in (unresolved), or nullptr. */
	FCRShopState* GetActiveShopState(FName ShopNodeId);

	/** Mutable state of the event the hamster is currently standing in (unresolved, initialized), or nullptr. */
	FCREventNodeState* GetActiveEventState(FName EventNodeId);

	/** Read-only in the editor/MCP for debugging during PIE. */
	UPROPERTY(VisibleInstanceOnly, Category = "CR|Run")
	FCRRunState RunState;
};
