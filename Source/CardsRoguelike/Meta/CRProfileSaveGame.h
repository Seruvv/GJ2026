// Profile persistence (UE SaveGame slots):
//  - "CR_ProfileIndex": list of profiles (metadata only) and the last selected profile id.
//  - "CR_Profile_<ProfileId>": one profile's progression (resources, building levels, flags, stats).
// Every save carries a version so later milestones can migrate old data instead of discarding it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CRMetaTypes.h"
#include "CRProfileSaveGame.generated.h"

/** Metadata shown in the profile list (kept in the index so the menu never loads every profile). */
USTRUCT(BlueprintType)
struct FCRProfileSummary
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FString ProfileId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FDateTime CreatedAt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FDateTime LastPlayedAt;
};

UCLASS()
class CARDSROGUELIKE_API UCRProfileIndexSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SaveVersion = 1;

	UPROPERTY()
	TArray<FCRProfileSummary> Profiles;

	/** Profile highlighted in the menu on the next launch. */
	UPROPERTY()
	FString LastSelectedProfileId;

	/** Numbering for default names and ids ("Хранитель 3"). */
	UPROPERTY()
	int32 NextProfileNumber = 1;
};

/** Outcome of the last finished run, shown in the hub. */
USTRUCT(BlueprintType)
struct FCRRunEndSummary
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	bool bValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	ECRRunEndReason Reason = ECRRunEndReason::Completed;

	/** Resources the hamster carried when the run ended. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FCRMetaResources Carried;

	/** Resources delivered to the profile (after the keep percent). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FCRMetaResources Delivered;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RoomsVisited = 0;
};

USTRUCT(BlueprintType)
struct FCRProfileStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunsStarted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunsCompleted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunsFailed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunsAbandoned = 0;
};

UCLASS()
class CARDSROGUELIKE_API UCRProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	int32 SaveVersion = 1;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FString ProfileId;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FDateTime CreatedAt;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FDateTime LastPlayedAt;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FCRMetaResources Resources;

	/** Hub building levels by BuildingId (missing = the building's StartLevel). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	TMap<FName, int32> BuildingLevels;

	/** Unlocked hub systems / flags (recomputed from building levels, plus any granted elsewhere). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	TArray<FName> UnlockedFlags;

	/** Free-form counters for future systems (achievements, journal...). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	TMap<FName, int32> Counters;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FCRProfileStats Stats;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FCRRunEndSummary LastRun;
};

namespace CRMeta
{
	inline const TCHAR* ProfileIndexSlot() { return TEXT("CR_ProfileIndex"); }
	inline FString ProfileSlot(const FString& ProfileId) { return FString(TEXT("CR_Profile_")) + ProfileId; }
	constexpr int32 SaveUserIndex = 0;
}
