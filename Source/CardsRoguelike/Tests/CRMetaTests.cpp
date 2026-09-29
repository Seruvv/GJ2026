// Automation tests for meta progression ("CardsRoguelike.Meta"): pure hub rules on transient definitions,
// the real hub catalog asset, and a profile SaveGame round trip on a throwaway slot.

#include "../Meta/CRMetaTypes.h"
#include "../Meta/CRProfileSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCRMetaResources MetaTestRes(int32 Silver, int32 Food, int32 Wood)
	{
		FCRMetaResources R;
		R.Silver = Silver;
		R.Food = Food;
		R.Wood = Wood;
		return R;
	}

	FCRHubEffect MetaTestEffect(ECRHubEffectType Type, int32 Amount)
	{
		FCRHubEffect Effect;
		Effect.Type = Type;
		Effect.Amount = Amount;
		return Effect;
	}

	/** Heart (1..3) gates Workshop (0..2); Workshop L1 adds a card, L2 start silver; Storage converts 10 silver -> 1 wood. */
	UCRHubCatalog* MakeMetaTestCatalog()
	{
		UCRHubCatalog* Catalog = NewObject<UCRHubCatalog>();

		UCRHubBuildingDefinition* Heart = NewObject<UCRHubBuildingDefinition>();
		Heart->BuildingId = TEXT("Heart");
		Heart->StartLevel = 1;
		Heart->Levels.SetNum(3);
		Heart->Levels[1].Cost = MetaTestRes(20, 0, 5);
		Heart->Levels[1].Effects.Add(MetaTestEffect(ECRHubEffectType::RunMaxHP, 3));
		Heart->Levels[2].Cost = MetaTestRes(40, 5, 10);

		UCRHubBuildingDefinition* Workshop = NewObject<UCRHubBuildingDefinition>();
		Workshop->BuildingId = TEXT("Workshop");
		Workshop->StartLevel = 0;
		Workshop->Levels.SetNum(2);
		Workshop->Levels[0].Cost = MetaTestRes(10, 0, 2);
		Workshop->Levels[0].RequiredBuildingId = TEXT("Heart");
		Workshop->Levels[0].RequiredLevel = 1;
		FCRHubEffect Card;
		Card.Type = ECRHubEffectType::RunStartCard;
		Card.CardId = TEXT("Guard");
		Workshop->Levels[0].Effects.Add(Card);
		Workshop->Levels[1].Cost = MetaTestRes(15, 0, 3);
		Workshop->Levels[1].RequiredBuildingId = TEXT("Heart");
		Workshop->Levels[1].RequiredLevel = 2;
		Workshop->Levels[1].Effects.Add(MetaTestEffect(ECRHubEffectType::RunStartSilver, 5));

		UCRHubBuildingDefinition* Storage = NewObject<UCRHubBuildingDefinition>();
		Storage->BuildingId = TEXT("Storage");
		Storage->StartLevel = 1;
		Storage->Levels.SetNum(1);
		FCRHubConversion Convert;
		Convert.ConversionId = TEXT("SilverToWood");
		Convert.Input = MetaTestRes(10, 0, 0);
		Convert.Output = MetaTestRes(0, 0, 1);
		Storage->Levels[0].Conversions.Add(Convert);

		Catalog->Buildings = { Heart, Workshop, Storage };
		return Catalog;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaUpgradeRulesTest, "CardsRoguelike.Meta.UpgradeRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaUpgradeRulesTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeMetaTestCatalog();
	const UCRHubBuildingDefinition& Heart = *Catalog->FindBuilding(TEXT("Heart"));
	const UCRHubBuildingDefinition& Workshop = *Catalog->FindBuilding(TEXT("Workshop"));
	TMap<FName, int32> Levels;

	TestEqual(TEXT("missing entry uses StartLevel"), CRMeta::GetLevel(Levels, Heart), 1);
	TestEqual(TEXT("workshop starts unbuilt"), CRMeta::GetLevel(Levels, Workshop), 0);

	TestEqual(TEXT("affordable heart upgrade"), CRMeta::GetUpgradeStatus(*Catalog, Levels, MetaTestRes(20, 0, 5), Heart), ECRUpgradeStatus::Available);
	TestEqual(TEXT("one wood short"), CRMeta::GetUpgradeStatus(*Catalog, Levels, MetaTestRes(20, 0, 4), Heart), ECRUpgradeStatus::NotEnoughResources);

	TestEqual(TEXT("workshop L1 needs heart 1 (met)"), CRMeta::GetUpgradeStatus(*Catalog, Levels, MetaTestRes(99, 99, 99), Workshop), ECRUpgradeStatus::Available);
	Levels.Add(TEXT("Workshop"), 1);
	TestEqual(TEXT("workshop L2 gated by heart 2"), CRMeta::GetUpgradeStatus(*Catalog, Levels, MetaTestRes(99, 99, 99), Workshop), ECRUpgradeStatus::Locked);
	Levels.Add(TEXT("Heart"), 2);
	TestEqual(TEXT("gate met after heart upgrade"), CRMeta::GetUpgradeStatus(*Catalog, Levels, MetaTestRes(99, 99, 99), Workshop), ECRUpgradeStatus::Available);
	Levels.Add(TEXT("Workshop"), 2);
	TestEqual(TEXT("max level"), CRMeta::GetUpgradeStatus(*Catalog, Levels, MetaTestRes(99, 99, 99), Workshop), ECRUpgradeStatus::MaxLevel);

	Levels.Add(TEXT("Workshop"), 7);
	TestEqual(TEXT("saved level above max is clamped"), CRMeta::GetLevel(Levels, Workshop), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRunBonusTest, "CardsRoguelike.Meta.RunBonusesAndConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRunBonusTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeMetaTestCatalog();
	TMap<FName, int32> Levels;

	FCRRunStartBonuses Bonuses = CRMeta::ComputeRunStartBonuses(*Catalog, Levels);
	TestEqual(TEXT("no bonus at start"), Bonuses.BonusMaxHP, 0);
	TestEqual(TEXT("no extra cards at start"), Bonuses.ExtraCardIds.Num(), 0);

	Levels.Add(TEXT("Heart"), 2);
	Levels.Add(TEXT("Workshop"), 2);
	Bonuses = CRMeta::ComputeRunStartBonuses(*Catalog, Levels);
	TestEqual(TEXT("heart L2 +3 HP"), Bonuses.BonusMaxHP, 3);
	TestEqual(TEXT("workshop L1 card stacks with L2"), Bonuses.ExtraCardIds.Num(), 1);
	TestEqual(TEXT("workshop L2 start silver"), Bonuses.StartSilver, 5);

	const TArray<FCRHubConversion>* Conversions = CRMeta::GetConversions(Levels, *Catalog->FindBuilding(TEXT("Storage")));
	TestTrue(TEXT("storage offers a conversion"), Conversions && Conversions->Num() == 1);
	FCRMetaResources Wallet = MetaTestRes(25, 0, 0);
	TestTrue(TEXT("can afford conversion"), Wallet.CanAfford((*Conversions)[0].Input));
	Wallet.Subtract((*Conversions)[0].Input);
	Wallet.Add((*Conversions)[0].Output);
	TestEqual(TEXT("silver spent"), Wallet.Silver, 15);
	TestEqual(TEXT("wood gained"), Wallet.Wood, 1);

	TestEqual(TEXT("keep 50% rounds down"), CRMeta::ApplyKeepPercent(9, 50), 4);
	TestEqual(TEXT("keep 100%"), CRMeta::ApplyKeepPercent(9, 100), 9);
	TestEqual(TEXT("keep 0%"), CRMeta::ApplyKeepPercent(9, 0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaCatalogAssetTest, "CardsRoguelike.Meta.CatalogAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaCatalogAssetTest::RunTest(const FString& Parameters)
{
	const UCRHubCatalog* Catalog = LoadObject<UCRHubCatalog>(nullptr, CRMeta::HubCatalogPath());
	if (!TestNotNull(TEXT("hub catalog asset loads"), Catalog))
	{
		return false;
	}
	for (const FName Id : { FName(TEXT("Heart")), FName(TEXT("Workshop")), FName(TEXT("Storage")) })
	{
		const UCRHubBuildingDefinition* Building = Catalog->FindBuilding(Id);
		if (!TestNotNull(*FString::Printf(TEXT("building %s exists"), *Id.ToString()), Building))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s has levels"), *Id.ToString()), Building->GetMaxLevel() > 0);
		TestTrue(*FString::Printf(TEXT("%s start level is valid"), *Id.ToString()), Building->StartLevel <= Building->GetMaxLevel());
		TestFalse(*FString::Printf(TEXT("%s has a display name"), *Id.ToString()), Building->DisplayName.IsEmpty());
		for (const FCRHubBuildingLevel& Level : Building->Levels)
		{
			TestTrue(*FString::Printf(TEXT("%s gate refers to a real building"), *Id.ToString()),
				Level.RequiredBuildingId.IsNone() || Catalog->FindBuilding(Level.RequiredBuildingId) != nullptr);
		}
	}
	const UCRHubBuildingDefinition* Storage = Catalog->FindBuilding(TEXT("Storage"));
	TMap<FName, int32> NoLevels;
	const TArray<FCRHubConversion>* Conversions = Storage ? CRMeta::GetConversions(NoLevels, *Storage) : nullptr;
	TestTrue(TEXT("a new profile's storage converts silver to wood"),
		Conversions && Conversions->ContainsByPredicate([](const FCRHubConversion& C) { return C.Input.Silver > 0 && C.Output.Wood > 0; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaSaveRoundTripTest, "CardsRoguelike.Meta.ProfileSaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaSaveRoundTripTest::RunTest(const FString& Parameters)
{
	const FString Slot = CRMeta::ProfileSlot(TEXT("AUTOMATION_TEST"));
	UCRProfileSaveGame* Save = Cast<UCRProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UCRProfileSaveGame::StaticClass()));
	Save->ProfileId = TEXT("AUTOMATION_TEST");
	Save->DisplayName = TEXT("Тестовый хранитель");
	Save->Resources = MetaTestRes(12, 3, 4);
	Save->BuildingLevels.Add(TEXT("Heart"), 2);
	Save->UnlockedFlags.Add(TEXT("TestFlag"));
	Save->Stats.RunsStarted = 5;
	Save->LastRun.bValid = true;
	Save->LastRun.Reason = ECRRunEndReason::Failed;
	Save->LastRun.Delivered = MetaTestRes(2, 0, 1);
	TestTrue(TEXT("saved"), UGameplayStatics::SaveGameToSlot(Save, Slot, CRMeta::SaveUserIndex));

	const UCRProfileSaveGame* Loaded = Cast<UCRProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, CRMeta::SaveUserIndex));
	if (TestNotNull(TEXT("loaded"), Loaded))
	{
		TestEqual(TEXT("name (Cyrillic)"), Loaded->DisplayName, FString(TEXT("Тестовый хранитель")));
		TestEqual(TEXT("silver"), Loaded->Resources.Silver, 12);
		TestEqual(TEXT("wood"), Loaded->Resources.Wood, 4);
		TestEqual(TEXT("heart level"), Loaded->BuildingLevels.FindRef(TEXT("Heart")), 2);
		TestTrue(TEXT("flag"), Loaded->UnlockedFlags.Contains(TEXT("TestFlag")));
		TestEqual(TEXT("runs started"), Loaded->Stats.RunsStarted, 5);
		TestTrue(TEXT("last run failed"), Loaded->LastRun.bValid && Loaded->LastRun.Reason == ECRRunEndReason::Failed);
	}
	TestTrue(TEXT("deleted"), UGameplayStatics::DeleteGameInSlot(Slot, CRMeta::SaveUserIndex));
	TestFalse(TEXT("gone after delete"), UGameplayStatics::DoesSaveGameExist(Slot, CRMeta::SaveUserIndex));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
