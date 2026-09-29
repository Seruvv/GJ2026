// Weighted list of events a run node can draw from (Data Asset), e.g. a default, regional or rare pool.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CREventTypes.h"
#include "CREventPool.generated.h"

UCLASS(BlueprintType)
class CARDSROGUELIKE_API UCREventPool : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	TArray<FCREventPoolEntry> Entries;

	/** Weighted random pick among entries with an event set and weight > 0. Null if none qualify. */
	TSoftObjectPtr<UCREventDefinition> PickEvent() const;
};
