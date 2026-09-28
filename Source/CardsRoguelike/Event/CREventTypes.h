// Authored narrative event data: effects, choices and pool entries. Plain data edited inside Data Assets;
// the runtime never branches on a specific event, it only interprets these specs.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "CREventTypes.generated.h"

class UCREventDefinition;

UENUM(BlueprintType)
enum class ECREventEffectType : uint8
{
	/** Signed HP change. Healing is capped at MaxHP. */
	ModifyHP,
	/** Signed change of one carried resource. A choice cannot spend more than the hamster carries. */
	ModifyResource,
	/** One random card from the shared card catalog (duplicates allowed). */
	AddRandomCard,
	/** A specific card id from the shared card catalog. */
	AddSpecificCard,
	/** The player picks one deck card to lose before the choice commits. */
	RemoveSelectedCard
};

UENUM(BlueprintType)
enum class ECREventResource : uint8
{
	Silver,
	Food,
	Wood
};

/**
 * Graybox illustration presets for the prototype event room. Future events can instead set
 * UCREventDefinition::PresentationActorClass to a custom Blueprint scene.
 */
UENUM(BlueprintType)
enum class ECREventPresentation : uint8
{
	None,
	AbandonedCamp,
	BlackShrine,
	ThingBehindDoor
};

/** One consequence of a choice. Which fields matter depends on Type. */
USTRUCT(BlueprintType)
struct FCREventEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	ECREventEffectType Type = ECREventEffectType::ModifyResource;

	/** Signed amount for ModifyHP / ModifyResource (e.g. -2 Silver, +3 HP). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event",
		meta = (EditCondition = "Type == ECREventEffectType::ModifyHP || Type == ECREventEffectType::ModifyResource", EditConditionHides))
	int32 Amount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event",
		meta = (EditCondition = "Type == ECREventEffectType::ModifyResource", EditConditionHides))
	ECREventResource Resource = ECREventResource::Silver;

	/** Card id from the shared catalog (e.g. "Blast"), for AddSpecificCard. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event",
		meta = (EditCondition = "Type == ECREventEffectType::AddSpecificCard", EditConditionHides))
	FName CardId;
};

/** One authored option: what the player reads, what happens, and the narrative result. */
USTRUCT(BlueprintType)
struct FCREventChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (MultiLine = true))
	FText ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (MultiLine = true))
	FText ResultText;

	/** Applied together, in order, exactly once. The choice is disabled if any of them cannot be paid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	TArray<FCREventEffect> Effects;
};

/** Weighted reference to an event inside a pool. Soft, so large libraries are not all loaded at once. */
USTRUCT(BlueprintType)
struct FCREventPoolEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	TSoftObjectPtr<UCREventDefinition> Event;

	/** Relative chance; entries with weight <= 0 never appear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "0.0"))
	float Weight = 1.f;
};

namespace CREvent
{
	/** Internal English name (logs). */
	FString ResourceName(ECREventResource Resource);

	/** Player-facing (Russian) resource name, e.g. "Серебро". */
	FString ResourceDisplayName(ECREventResource Resource);

	/** Player-facing (Russian) shortage reason, e.g. "Нужно серебра: 2". */
	FString NeedResourceText(ECREventResource Resource, int32 Amount);

	/** Short signed label, e.g. "+3" / "-2". */
	FString SignedAmount(int32 Amount);
}
