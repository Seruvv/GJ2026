// Persistent run data types. Plain data only: no actor references, so the state survives map changes.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "CRRunTypes.generated.h"

class UCREventDefinition;
class UCREventPool;

UENUM(BlueprintType)
enum class ECRRoomType : uint8
{
	Start,
	Combat,
	Shop,
	Event,
	Boss,
	Return
};

/** Distinguishes "no run yet" from "a run that ended", so a failed run is not silently replaced. */
UENUM(BlueprintType)
enum class ECRRunStatus : uint8
{
	NotStarted,
	Active,
	Failed,
	Completed
};

UENUM(BlueprintType)
enum class ECRRunNodeState : uint8
{
	Locked,
	Available,
	Current,
	Completed
};

/** One room on the run map. Connections are directed: they list the legal next rooms. */
USTRUCT(BlueprintType)
struct FCRRunNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	ECRRoomType RoomType = ECRRoomType::Combat;

	/** Column of the run graph (0 = Start). Edges only ever lead to the next layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 Layer = 0;

	/** Position on the 3D run map (visualization only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	TArray<FName> ConnectedNodeIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	ECRRunNodeState State = ECRRunNodeState::Locked;

	/** Event rooms: the pool this node draws its event from (the default pool if unset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	TSoftObjectPtr<UCREventPool> EventPool;
};

USTRUCT(BlueprintType)
struct FCRHamsterRunData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 CurrentHP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 MaxHP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 ManaPerTurn = 0;
};

/** Everything the hamster is carrying back toward the Bottle during this run. */
USTRUCT(BlueprintType)
struct FCRCarriedLoot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 Silver = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 Food = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 Wood = 0;

	/** Cards found this run (not yet part of the deck). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	TArray<FName> CardIds;

	/** Artifacts found this run (not yet equipped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	TArray<FName> ArtifactIds;
};

/** Contents of one shop room, created on first visit and kept for the rest of the run. */
USTRUCT(BlueprintType)
struct FCRShopState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	bool bInitialized = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TArray<FName> OfferCardIds;

	/** Parallel to OfferCardIds: true once that offer has been bought. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TArray<bool> OfferPurchased;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	bool bHealPurchased = false;

	/** Merchant placeholder line picked when the shop was created. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	FString MerchantLine;
};

/**
 * One event room's state, created on first visit and kept for the rest of the run. The event never
 * rerolls, and once a choice is committed its result is replayed rather than re-applied.
 */
USTRUCT(BlueprintType)
struct FCREventNodeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	bool bInitialized = false;

	/** Event drawn from the node's pool on first visit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TSoftObjectPtr<UCREventDefinition> SelectedEvent;

	/** Chosen option, or INDEX_NONE while undecided. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	int32 SelectedChoiceIndex = INDEX_NONE;

	/** True once the chosen option's effects were applied (never twice). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	bool bEffectsCommitted = false;

	/** Card the player sacrificed for a RemoveSelectedCard effect, if any. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	FName SacrificedCardId;

	/** Cards gained by the committed option (random picks are fixed here). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TArray<FName> GainedCardIds;

	/** Effect summary lines shown on the result screen ("HP -3", "Card gained: BLAST"). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TArray<FString> ResultLines;
};

USTRUCT(BlueprintType)
struct FCRRunState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	ECRRunStatus Status = ECRRunStatus::NotStarted;

	/** Seed the run graph was generated from; the same seed always rebuilds the same map. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	int32 RunSeed = 0;

	/**
	 * Whether the room at CurrentNodeId has been resolved (e.g. its combat won).
	 * Outgoing rooms only become Available once the current room is resolved.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	bool bCurrentRoomResolved = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	FCRHamsterRunData Hamster;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	FName CurrentNodeId;

	/** Rooms entered this run, in order; the last entry is the current room. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TArray<FName> VisitedNodeIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TArray<FName> DeckCardIds;

	/** Equipped artifacts (no behavior yet). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TArray<FName> ArtifactIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	FCRCarriedLoot Carried;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TArray<FCRRunNodeData> Nodes;

	/** Shop contents per shop node id; offers never reroll within a run. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TMap<FName, FCRShopState> ShopStates;

	/** Event state per event node id; independent for every event room of the run. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TMap<FName, FCREventNodeState> EventStates;
};

namespace CRRun
{
	FString RoomTypeName(ECRRoomType Type);
	FLinearColor RoomTypeColor(ECRRoomType Type);

	/** Prototype maps used by the run loop. */
	inline const TCHAR* RunMapPath() { return TEXT("/Game/Dev/TestMaps/LV_RunMapSandbox"); }
	inline const TCHAR* CombatMapPath() { return TEXT("/Game/Dev/TestMaps/LV_CombatSandbox"); }
	inline const TCHAR* ShopMapPath() { return TEXT("/Game/Dev/TestMaps/LV_ShopSandbox"); }
	inline const TCHAR* EventMapPath() { return TEXT("/Game/Dev/TestMaps/LV_EventSandbox"); }

	/** Pool used by event nodes that do not name their own. */
	inline const TCHAR* DefaultEventPoolPath() { return TEXT("/Game/Dev/Events/Pools/DA_EventPool_Default.DA_EventPool_Default"); }

	/** Generic placeholder merchant lines (setting-neutral until the jam theme is known). */
	const TArray<FString>& MerchantLines();
}
