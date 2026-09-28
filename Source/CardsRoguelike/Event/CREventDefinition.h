// One authored narrative event (Data Asset). Designers create these in the editor; no C++ per event.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CREventTypes.h"
#include "CREventDefinition.generated.h"

UCLASS(BlueprintType)
class CARDSROGUELIKE_API UCREventDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id for logs, saves and future lookups (e.g. "AbandonedCamp"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FName EventId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (MultiLine = true))
	FText NarrativeBody;

	/** Free-form labels for future filtering (region, rarity, mood...). Not used by the runtime yet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	TArray<FName> Tags;

	/** Closing line on the result screen, e.g. "And the traveler moved on." */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (MultiLine = true))
	FText ContinuationText;

	/** Authored options. The prototype uses three; the runtime supports any count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	TArray<FCREventChoice> Choices;

	/** Built-in graybox illustration used when no custom presentation actor is set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event|Presentation")
	ECREventPresentation Presentation = ECREventPresentation::None;

	/**
	 * Extension point: a custom scene actor (e.g. a Blueprint diorama or illustration board) spawned
	 * in the event room instead of the built-in preset. Soft, so it only loads for this event.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event|Presentation")
	TSoftClassPtr<AActor> PresentationActorClass;
};
