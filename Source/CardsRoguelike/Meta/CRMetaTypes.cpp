#include "CRMetaTypes.h"

#include "../Combat/CRCardLibrary.h"

void FCRMetaResources::Subtract(const FCRMetaResources& Other)
{
	Silver = FMath::Max(0, Silver - Other.Silver);
	Food = FMath::Max(0, Food - Other.Food);
	Wood = FMath::Max(0, Wood - Other.Wood);
}

const UCRHubBuildingDefinition* UCRHubCatalog::FindBuilding(FName BuildingId) const
{
	for (const UCRHubBuildingDefinition* Building : Buildings)
	{
		if (Building && Building->BuildingId == BuildingId)
		{
			return Building;
		}
	}
	return nullptr;
}

namespace CRMeta
{
	int32 GetLevel(const TMap<FName, int32>& Levels, const UCRHubBuildingDefinition& Building)
	{
		const int32* Saved = Levels.Find(Building.BuildingId);
		return FMath::Clamp(Saved ? *Saved : Building.StartLevel, 0, Building.GetMaxLevel());
	}

	ECRUpgradeStatus GetUpgradeStatus(const UCRHubCatalog& Catalog, const TMap<FName, int32>& Levels, const FCRMetaResources& Resources,
		const UCRHubBuildingDefinition& Building, const FCRHubBuildingLevel** OutNextLevel)
	{
		const FCRHubBuildingLevel* Next = Building.FindLevel(GetLevel(Levels, Building) + 1);
		if (OutNextLevel)
		{
			*OutNextLevel = Next;
		}
		if (!Next)
		{
			return ECRUpgradeStatus::MaxLevel;
		}
		if (!Next->RequiredBuildingId.IsNone())
		{
			const UCRHubBuildingDefinition* Required = Catalog.FindBuilding(Next->RequiredBuildingId);
			if (!Required)
			{
				return ECRUpgradeStatus::Invalid;
			}
			if (GetLevel(Levels, *Required) < Next->RequiredLevel)
			{
				return ECRUpgradeStatus::Locked;
			}
		}
		return Resources.CanAfford(Next->Cost) ? ECRUpgradeStatus::Available : ECRUpgradeStatus::NotEnoughResources;
	}

	const TArray<FCRHubConversion>* GetConversions(const TMap<FName, int32>& Levels, const UCRHubBuildingDefinition& Building)
	{
		const FCRHubBuildingLevel* Current = Building.FindLevel(GetLevel(Levels, Building));
		return Current ? &Current->Conversions : nullptr;
	}

	FCRRunStartBonuses ComputeRunStartBonuses(const UCRHubCatalog& Catalog, const TMap<FName, int32>& Levels)
	{
		FCRRunStartBonuses Bonuses;
		for (const UCRHubBuildingDefinition* Building : Catalog.Buildings)
		{
			if (!Building)
			{
				continue;
			}
			const int32 Level = GetLevel(Levels, *Building);
			for (int32 L = 1; L <= Level; ++L)
			{
				for (const FCRHubEffect& Effect : Building->Levels[L - 1].Effects)
				{
					switch (Effect.Type)
					{
					case ECRHubEffectType::RunMaxHP:       Bonuses.BonusMaxHP += Effect.Amount; break;
					case ECRHubEffectType::RunStartCard:   if (!Effect.CardId.IsNone()) { Bonuses.ExtraCardIds.Add(Effect.CardId); } break;
					case ECRHubEffectType::RunStartSilver: Bonuses.StartSilver += Effect.Amount; break;
					case ECRHubEffectType::RunStartFood:   Bonuses.StartFood += Effect.Amount; break;
					default: break;
					}
				}
			}
		}
		return Bonuses;
	}

	TArray<FName> ComputeUnlockedFlags(const UCRHubCatalog& Catalog, const TMap<FName, int32>& Levels)
	{
		TArray<FName> Flags;
		for (const UCRHubBuildingDefinition* Building : Catalog.Buildings)
		{
			if (!Building)
			{
				continue;
			}
			const int32 Level = GetLevel(Levels, *Building);
			for (int32 L = 1; L <= Level; ++L)
			{
				for (const FCRHubEffect& Effect : Building->Levels[L - 1].Effects)
				{
					if (Effect.Type == ECRHubEffectType::UnlockFlag && !Effect.Flag.IsNone())
					{
						Flags.AddUnique(Effect.Flag);
					}
				}
			}
		}
		return Flags;
	}

	int32 ApplyKeepPercent(int32 Amount, int32 Percent)
	{
		return FMath::Max(0, Amount) * FMath::Clamp(Percent, 0, 100) / 100;
	}

	namespace
	{
		FString JoinResources(const FCRMetaResources& Resources, bool bSigned)
		{
			TArray<FString> Parts;
			const auto Part = [&Parts, bSigned](const TCHAR* Name, int32 Value)
			{
				if (Value != 0)
				{
					Parts.Add(bSigned ? FString::Printf(TEXT("%s %+d"), Name, Value) : FString::Printf(TEXT("%s %d"), Name, Value));
				}
			};
			Part(TEXT("Серебро"), Resources.Silver);
			Part(TEXT("Еда"), Resources.Food);
			Part(TEXT("Дерево"), Resources.Wood);
			return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("—"));
		}
	}

	FString FormatResources(const FCRMetaResources& Resources)
	{
		return JoinResources(Resources, false);
	}

	FString FormatGain(const FCRMetaResources& Resources)
	{
		return JoinResources(Resources, true);
	}

	FString DescribeEffect(const FCRHubEffect& Effect)
	{
		switch (Effect.Type)
		{
		case ECRHubEffectType::RunMaxHP:
			return FString::Printf(TEXT("+%d к максимальному здоровью в походе"), Effect.Amount);
		case ECRHubEffectType::RunStartCard:
		{
			const FCRCardDef* Card = CRCardLibrary::FindCard(Effect.CardId);
			return FString::Printf(TEXT("Карта %s в начальной колоде"), Card ? *Card->Name : *Effect.CardId.ToString());
		}
		case ECRHubEffectType::RunStartSilver:
			return FString::Printf(TEXT("Поход начинается с %d серебра"), Effect.Amount);
		case ECRHubEffectType::RunStartFood:
			return FString::Printf(TEXT("Поход начинается с %d еды"), Effect.Amount);
		case ECRHubEffectType::UnlockFlag:
			return FString::Printf(TEXT("Открыто: %s"), *Effect.Flag.ToString());
		default:
			return FString();
		}
	}
}
