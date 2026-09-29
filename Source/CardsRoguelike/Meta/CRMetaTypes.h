// Out-of-run (meta) progression data: profile resources, data-driven hub buildings and their upgrade levels.
// Definitions live in Data Assets (tunable in the editor); the pure helpers in CRMeta work on plain data so
// they can be tested without a world.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../Run/CRRunTypes.h"
#include "CRMetaTypes.generated.h"

/** Persistent profile resources (the same three the run carries). */
USTRUCT(BlueprintType)
struct FCRMetaResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta", meta = (ClampMin = "0"))
	int32 Silver = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta", meta = (ClampMin = "0"))
	int32 Food = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta", meta = (ClampMin = "0"))
	int32 Wood = 0;

	bool IsZero() const { return Silver == 0 && Food == 0 && Wood == 0; }
	bool CanAfford(const FCRMetaResources& Cost) const { return Silver >= Cost.Silver && Food >= Cost.Food && Wood >= Cost.Wood; }
	void Add(const FCRMetaResources& Other) { Silver += Other.Silver; Food += Other.Food; Wood += Other.Wood; }
	/** Subtracts, never going below zero. */
	void Subtract(const FCRMetaResources& Other);
};

/** What an upgrade level does. Effects of every reached level stack. */
UENUM(BlueprintType)
enum class ECRHubEffectType : uint8
{
	/** Description only (e.g. a gate that other buildings' levels check). */
	None,
	/** +Amount max (and starting) HP for every run. */
	RunMaxHP,
	/** CardId is added to the starting deck of every run. */
	RunStartCard,
	/** Every run starts with Amount carried Silver. */
	RunStartSilver,
	/** Every run starts with Amount carried Food. */
	RunStartFood,
	/** Stores Flag as unlocked in the profile (for systems that read it later). */
	UnlockFlag
};

USTRUCT(BlueprintType)
struct FCRHubEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	ECRHubEffectType Type = ECRHubEffectType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	int32 Amount = 0;

	/** RunStartCard: card id from the card library (Push, Guard...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	FName CardId;

	/** UnlockFlag: flag stored in the profile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	FName Flag;
};

/** A resource exchange offered by a building level (e.g. the Storage turning Silver into Wood). */
USTRUCT(BlueprintType)
struct FCRHubConversion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	FName ConversionId;

	/** Button text, e.g. "Перегнать серебро в дерево". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	FCRMetaResources Input;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	FCRMetaResources Output;
};

/** One upgrade level of a building (index 0 = level 1). */
USTRUCT(BlueprintType)
struct FCRHubBuildingLevel
{
	GENERATED_BODY()

	/** Paid to reach this level (ignored for levels at or below the building's StartLevel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	FCRMetaResources Cost;

	/** Optional gate: another building must be at least RequiredLevel first (e.g. the Heart). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	FName RequiredBuildingId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	int32 RequiredLevel = 0;

	/** Player-facing summary of what this level gives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta", meta = (MultiLine = "true"))
	FString Summary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	TArray<FCRHubEffect> Effects;

	/** Exchanges available while the building is exactly at this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
	TArray<FCRHubConversion> Conversions;
};

/** One hub activity/building. Add a new asset and list it in the catalog to add a building. */
UCLASS(BlueprintType)
class CARDSROGUELIKE_API UCRHubBuildingDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id saved in profiles (never rename once shipped). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FName BuildingId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (MultiLine = "true"))
	FString Description;

	/** What the building will do later (shown as a note while it is still a prototype). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (MultiLine = "true"))
	FString FutureNote;

	/** Scene anchor of the hub room this building is shown at (Heart, Workshop, Storage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FName AnchorId;

	/** Level of a new profile (0 = not built yet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "0"))
	int32 StartLevel = 0;

	/** Level N is Levels[N - 1]; the max level is Levels.Num(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	TArray<FCRHubBuildingLevel> Levels;

	int32 GetMaxLevel() const { return Levels.Num(); }
	/** Level data for Level (1-based), or nullptr. */
	const FCRHubBuildingLevel* FindLevel(int32 Level) const { return Levels.IsValidIndex(Level - 1) ? &Levels[Level - 1] : nullptr; }
};

/** Everything the hub needs: its buildings and the profile/run economy rules. */
UCLASS(BlueprintType)
class CARDSROGUELIKE_API UCRHubCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Buildings in display order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	TArray<TObjectPtr<UCRHubBuildingDefinition>> Buildings;

	/** Resources a new profile starts with. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	FCRMetaResources StartingResources;

	/** Share (%) of carried run resources delivered to the profile when the run reaches the Return. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CompletedRunKeepPercent = 100;

	/** Share (%) kept when the hamster dies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0", ClampMax = "100"))
	int32 FailedRunKeepPercent = 50;

	/** Share (%) kept when the player leaves a run early. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0", ClampMax = "100"))
	int32 AbandonedRunKeepPercent = 0;

	const UCRHubBuildingDefinition* FindBuilding(FName BuildingId) const;
};

UENUM()
enum class ECRUpgradeStatus : uint8
{
	Available,
	MaxLevel,
	/** Another building's level gate is not met. */
	Locked,
	NotEnoughResources,
	Invalid
};

namespace CRMeta
{
	/** Maps used by the out-of-run flow. */
	inline const TCHAR* MainMenuMapPath() { return TEXT("/Game/Dev/TestMaps/LV_MainMenu"); }
	inline const TCHAR* HubMapPath() { return TEXT("/Game/Dev/TestMaps/LV_SanctuaryHub"); }
	inline const TCHAR* HubCatalogPath() { return TEXT("/Game/Dev/Hub/DA_HubCatalog.DA_HubCatalog"); }

	/** Level a profile has for Building (its StartLevel if the profile never touched it). */
	int32 GetLevel(const TMap<FName, int32>& Levels, const UCRHubBuildingDefinition& Building);

	/** Whether Building can go up one level with these levels and resources. OutRequirement names an unmet gate. */
	ECRUpgradeStatus GetUpgradeStatus(const UCRHubCatalog& Catalog, const TMap<FName, int32>& Levels, const FCRMetaResources& Resources,
		const UCRHubBuildingDefinition& Building, const FCRHubBuildingLevel** OutNextLevel = nullptr);

	/** Conversions the building offers at its current level. */
	const TArray<FCRHubConversion>* GetConversions(const TMap<FName, int32>& Levels, const UCRHubBuildingDefinition& Building);

	/** Stacks the effects of every reached level of every building. */
	FCRRunStartBonuses ComputeRunStartBonuses(const UCRHubCatalog& Catalog, const TMap<FName, int32>& Levels);

	/** Flags unlocked by reached levels (UnlockFlag effects). */
	TArray<FName> ComputeUnlockedFlags(const UCRHubCatalog& Catalog, const TMap<FName, int32>& Levels);

	/** Percent of Amount, rounded down. */
	int32 ApplyKeepPercent(int32 Amount, int32 Percent);

	/** "Серебро 10, Дерево 1" (zero entries skipped; "—" if everything is zero). */
	FString FormatResources(const FCRMetaResources& Resources);

	/** "Серебро +10, Еда +2" for gains. */
	FString FormatGain(const FCRMetaResources& Resources);

	/** Player-facing one-line text of an effect ("+3 к максимальному здоровью"). */
	FString DescribeEffect(const FCRHubEffect& Effect);
}
