// Automation tests for meta progression ("CardsRoguelike.Meta"): pure hub rules on transient definitions,
// the real hub catalog asset, and a profile SaveGame round trip on a throwaway slot. M2.8 adds permanent
// death ("Meta.Death") and recruitment ("Meta.Recruitment"), driven through the real run subsystem where a
// run has to end.

#include "../Event/CREventDefinition.h"
#include "../Meta/CRMetaTypes.h"
#include "../Meta/CRProfileSaveGame.h"
#include "../Run/CRRunSubsystem.h"
#include "Engine/GameInstance.h"
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

		// Three hamsters with different stats (a tank, a caster, an average one).
		UCRHamsterRosterDefinition* Roster = NewObject<UCRHamsterRosterDefinition>();
		const auto AddHamster = [Roster](const TCHAR* Id, const TCHAR* Name, int32 HP, int32 Mana)
		{
			FCRHamsterDefinition& Def = Roster->Hamsters.AddDefaulted_GetRef();
			Def.HamsterId = Id;
			Def.DisplayName = Name;
			Def.BaseMaxHP = HP;
			Def.BaseManaPerTurn = Mana;
			Def.EpitaphText = FString::Printf(TEXT("Здесь лежит %s."), Name);
		};
		AddHamster(TEXT("Tank"), TEXT("Валун"), 36, 2);
		AddHamster(TEXT("Caster"), TEXT("Искра"), 24, 4);
		AddHamster(TEXT("Average"), TEXT("Пуговка"), 30, 3);
		Catalog->DefaultRoster = Roster;
		return Catalog;
	}

	UCRProfileSaveGame* MakeMetaTestProfile()
	{
		return Cast<UCRProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UCRProfileSaveGame::StaticClass()));
	}

	/** Recruitment rules like the real asset: 3 candidates, roster target 6, the default stat templates. */
	UCRRecruitmentDefinition* MakeMetaTestRecruitment()
	{
		UCRRecruitmentDefinition* Recruitment = NewObject<UCRRecruitmentDefinition>();
		Recruitment->CandidateCount = 3;
		Recruitment->TargetLivingRosterSize = 6;
		Recruitment->NamePool = { TEXT("Орешек"), TEXT("Кнопка"), TEXT("Фасолька"), TEXT("Шуршик"), TEXT("Пушок"), TEXT("Валун") };
		Recruitment->EpitaphPool = { TEXT("{Name} ушёл за горизонт."), TEXT("Здесь спит {Name}.") };
		for (const FIntPoint& Stats : { FIntPoint(36, 2), FIntPoint(34, 2), FIntPoint(32, 3), FIntPoint(30, 3), FIntPoint(27, 4), FIntPoint(24, 4) })
		{
			FCRRecruitStatTemplate& Template = Recruitment->StatTemplates.AddDefaulted_GetRef();
			Template.BaseMaxHP = Stats.X;
			Template.BaseManaPerTurn = Stats.Y;
		}
		Recruitment->TintOptions = { FLinearColor(0.8f, 0.5f, 0.3f), FLinearColor(0.4f, 0.6f, 0.8f), FLinearColor(0.6f, 0.7f, 0.4f) };
		return Recruitment;
	}

	/** The test catalog plus recruitment (the M2.8 shape of the real catalog). */
	UCRHubCatalog* MakeDeathTestCatalog()
	{
		UCRHubCatalog* Catalog = MakeMetaTestCatalog();
		Catalog->Recruitment = MakeMetaTestRecruitment();
		return Catalog;
	}

	/** A current profile with the three test hamsters (Tank selected) and saved recruitment candidates. */
	UCRProfileSaveGame* MakeDeathTestProfile(const UCRHubCatalog* Catalog)
	{
		UCRProfileSaveGame* Profile = MakeMetaTestProfile();
		Profile->ProfileId = TEXT("P_DEATH_TEST");
		Profile->SaveVersion = CRMeta::CurrentProfileSaveVersion;
		Profile->bHamsterRosterInitialized = CRMeta::InitializeHamsterRoster(*Profile, Catalog);
		Profile->SelectedHamsterId = TEXT("Tank");
		CRMeta::RefillRecruitCandidates(*Profile, Catalog->Recruitment);
		return Profile;
	}

	/** A run as the run subsystem reports it at its end (for rules that only read the ended state). */
	FCRRunState MakeEndedRun(ECRRunEndReason Reason, FName HamsterId, const TCHAR* RunId, int32 Silver, int32 Food, int32 Wood)
	{
		FCRRunState Run;
		Run.RunId = RunId;
		Run.RunSeed = 4242;
		Run.ProfileId = TEXT("P_DEATH_TEST");
		Run.Hamster.HamsterId = HamsterId;
		Run.Hamster.Name = HamsterId.ToString();
		Run.EndReason = Reason;
		Run.bEndReported = true;
		Run.Status = Reason == ECRRunEndReason::Failed ? ECRRunStatus::Failed : (Reason == ECRRunEndReason::Completed ? ECRRunStatus::Completed : ECRRunStatus::Active);
		Run.DeathCause = Reason == ECRRunEndReason::Failed ? ECRHamsterDeathCause::Combat : ECRHamsterDeathCause::None;
		Run.Carried.Silver = Silver;
		Run.Carried.Food = Food;
		Run.Carried.Wood = Wood;
		Run.CurrentNodeId = TEXT("L2_0");
		Run.VisitedNodeIds = { TEXT("Start"), TEXT("L1_0"), TEXT("L2_0") };
		return Run;
	}

	/** The real run subsystem inside a throwaway game instance (its required outer), not initialized as a subsystem. */
	UCRRunSubsystem* MakeTestRunSubsystem()
	{
		return NewObject<UCRRunSubsystem>(NewObject<UGameInstance>(GetTransientPackage()));
	}

	/**
	 * The real run subsystem (see MakeTestRunSubsystem), with a profile run started for Hamster and the hamster
	 * placed in the first room of RoomType (unresolved). Every reported run end is captured in OutEnds.
	 */
	UCRRunSubsystem* StartTestProfileRun(const UCRHubCatalog& Catalog, const UCRProfileSaveGame& Profile, FName HamsterId, ECRRoomType RoomType,
		TArray<FCRRunState>& OutEnds)
	{
		UCRRunSubsystem* Run = MakeTestRunSubsystem();
		Run->OnRunEnded.AddLambda([&OutEnds](const FCRRunState& Ended) { OutEnds.Add(Ended); });
		Run->StartProfileRun(CRMeta::BuildRunStartConfig(Catalog, Profile.BuildingLevels, *CRMeta::FindHamster(Profile, HamsterId), Profile.ProfileId));
		FCRRunState& State = Run->GetMutableRunStateForTests();
		if (const FCRRunNodeData* Room = State.Nodes.FindByPredicate([RoomType](const FCRRunNodeData& N) { return N.RoomType == RoomType; }))
		{
			State.CurrentNodeId = Room->NodeId;
			State.VisitedNodeIds.Add(Room->NodeId);
			State.bCurrentRoomResolved = false;
		}
		return Run;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRosterNewProfileTest, "CardsRoguelike.Meta.Hamsters.NewProfileRoster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRosterNewProfileTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeMetaTestCatalog();
	UCRProfileSaveGame* Profile = MakeMetaTestProfile();
	TestTrue(TEXT("A: roster initialized"), CRMeta::InitializeHamsterRoster(*Profile, Catalog));
	CRMeta::EnsureValidHamsterSelection(*Profile);
	TestEqual(TEXT("A: every default hamster added"), Profile->Hamsters.Num(), 3);

	TSet<FName> Ids;
	for (const FCRHamsterPersistentState& Hamster : Profile->Hamsters)
	{
		TestFalse(TEXT("B: id is set"), Hamster.HamsterId.IsNone());
		TestFalse(TEXT("B: id is unique"), Ids.Contains(Hamster.HamsterId));
		Ids.Add(Hamster.HamsterId);
		TestTrue(TEXT("new hamsters are alive"), Hamster.bAlive);
		TestFalse(TEXT("epitaph authored from creation"), Hamster.EpitaphText.IsEmpty());
	}
	const FCRHamsterPersistentState* Selected = CRMeta::FindSelectedHamster(*Profile);
	TestTrue(TEXT("C: a living hamster is selected"), Selected && Selected->bAlive);

	TestFalse(TEXT("roster is only added once"), CRMeta::InitializeHamsterRoster(*Profile, Catalog));
	TestEqual(TEXT("no duplicates on a second call"), Profile->Hamsters.Num(), 3);

	// An invalid (or dead) selection falls back to the first living hamster.
	Profile->Hamsters[0].bAlive = false;
	Profile->SelectedHamsterId = Profile->Hamsters[0].HamsterId;
	TestTrue(TEXT("dead selection is replaced"), CRMeta::EnsureValidHamsterSelection(*Profile));
	TestEqual(TEXT("first living hamster selected"), Profile->SelectedHamsterId, Profile->Hamsters[1].HamsterId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRosterMigrationTest, "CardsRoguelike.Meta.Hamsters.MigrationAndRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRosterMigrationTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeMetaTestCatalog();

	// A version-1 profile as M2.7 wrote it: progress but no roster.
	UCRProfileSaveGame* Old = MakeMetaTestProfile();
	Old->ProfileId = TEXT("AUTOMATION_MIGRATION");
	Old->Resources = MetaTestRes(15, 8, 1);
	Old->BuildingLevels.Add(TEXT("Heart"), 2);
	Old->BuildingLevels.Add(TEXT("Workshop"), 1);
	Old->Stats.RunsStarted = 3;
	const FString Slot = CRMeta::ProfileSlot(Old->ProfileId);
	TestTrue(TEXT("old profile saved"), UGameplayStatics::SaveGameToSlot(Old, Slot, CRMeta::SaveUserIndex));

	UCRProfileSaveGame* Loaded = Cast<UCRProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, CRMeta::SaveUserIndex));
	if (!TestNotNull(TEXT("old profile loads"), Loaded))
	{
		return false;
	}
	TestFalse(TEXT("old save has no roster flag"), Loaded->bHamsterRosterInitialized);
	TestTrue(TEXT("E: migration changes the profile"), CRMeta::MigrateProfile(*Loaded, Catalog));
	TestEqual(TEXT("E: roster added"), Loaded->Hamsters.Num(), 3);
	TestEqual(TEXT("E: silver preserved"), Loaded->Resources.Silver, 15);
	TestEqual(TEXT("E: food preserved"), Loaded->Resources.Food, 8);
	TestEqual(TEXT("E: wood preserved"), Loaded->Resources.Wood, 1);
	TestEqual(TEXT("E: heart level preserved"), Loaded->BuildingLevels.FindRef(TEXT("Heart")), 2);
	TestEqual(TEXT("E: workshop level preserved"), Loaded->BuildingLevels.FindRef(TEXT("Workshop")), 1);
	TestEqual(TEXT("stats preserved"), Loaded->Stats.RunsStarted, 3);
	TestEqual(TEXT("version bumped"), Loaded->SaveVersion, CRMeta::CurrentProfileSaveVersion);
	TestNotNull(TEXT("migration selects a hamster"), CRMeta::FindSelectedHamster(*Loaded));
	TestFalse(TEXT("migration runs exactly once"), CRMeta::MigrateProfile(*Loaded, Catalog));

	// D: the selection survives a save/load round trip.
	Loaded->SelectedHamsterId = TEXT("Caster");
	TestTrue(TEXT("migrated profile saved"), UGameplayStatics::SaveGameToSlot(Loaded, Slot, CRMeta::SaveUserIndex));
	const UCRProfileSaveGame* Again = Cast<UCRProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, CRMeta::SaveUserIndex));
	if (TestNotNull(TEXT("reloaded"), Again))
	{
		TestEqual(TEXT("D: selection persisted"), Again->SelectedHamsterId, FName(TEXT("Caster")));
		TestTrue(TEXT("roster flag persisted"), Again->bHamsterRosterInitialized);
		TestEqual(TEXT("roster persisted"), Again->Hamsters.Num(), 3);
		const FCRHamsterPersistentState* Caster = CRMeta::FindHamster(*Again, TEXT("Caster"));
		TestTrue(TEXT("hamster stats persisted"), Caster && Caster->BaseMaxHP == 24 && Caster->BaseManaPerTurn == 4);
		TestTrue(TEXT("epitaph persisted"), Caster && !Caster->EpitaphText.IsEmpty());
	}
	UGameplayStatics::DeleteGameInSlot(Slot, CRMeta::SaveUserIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaEffectiveStatsTest, "CardsRoguelike.Meta.Hamsters.EffectiveRunStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaEffectiveStatsTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeMetaTestCatalog();
	UCRProfileSaveGame* Profile = MakeMetaTestProfile();
	CRMeta::InitializeHamsterRoster(*Profile, Catalog);
	TMap<FName, int32> Levels;
	Levels.Add(TEXT("Heart"), 2);    // +3 max HP
	Levels.Add(TEXT("Workshop"), 1); // +1 Guard card

	const FCRHamsterPersistentState& Tank = *CRMeta::FindHamster(*Profile, TEXT("Tank"));
	const FCRHamsterPersistentState& Caster = *CRMeta::FindHamster(*Profile, TEXT("Caster"));

	FCRRunState TankRun;
	CRRun::ApplyStartConfig(TankRun, CRMeta::BuildRunStartConfig(*Catalog, Levels, Tank, TEXT("P_TEST")));
	FCRRunState CasterRun;
	CRRun::ApplyStartConfig(CasterRun, CRMeta::BuildRunStartConfig(*Catalog, Levels, Caster, TEXT("P_TEST")));

	TestEqual(TEXT("F: effective HP = base + heart bonus"), TankRun.Hamster.MaxHP, 39);
	TestEqual(TEXT("F: starts at full HP"), TankRun.Hamster.CurrentHP, 39);
	TestEqual(TEXT("G: mana = hamster base mana"), TankRun.Hamster.ManaPerTurn, 2);
	TestEqual(TEXT("G: caster mana"), CasterRun.Hamster.ManaPerTurn, 4);
	TestEqual(TEXT("caster HP = 24 + 3"), CasterRun.Hamster.MaxHP, 27);
	TestTrue(TEXT("H: different hamsters, different run stats"),
		TankRun.Hamster.MaxHP != CasterRun.Hamster.MaxHP && TankRun.Hamster.ManaPerTurn != CasterRun.Hamster.ManaPerTurn);
	TestEqual(TEXT("base HP never overwritten by bonuses"), Tank.BaseMaxHP, 36);

	TestEqual(TEXT("I: run carries the hamster id"), TankRun.Hamster.HamsterId, FName(TEXT("Tank")));
	TestEqual(TEXT("I: run carries the hamster name"), TankRun.Hamster.Name, FString(TEXT("Валун")));
	TestEqual(TEXT("I: run carries the profile"), TankRun.ProfileId, FString(TEXT("P_TEST")));
	TestEqual(TEXT("I: starter deck + workshop card"), TankRun.DeckCardIds.Num(), CRRun::StarterDeck().Num() + 1);

	FCRRunState DevRun;
	CRRun::ApplyStartConfig(DevRun, CRRun::MakeDeveloperStartConfig());
	TestTrue(TEXT("developer runs have no profile or hamster id"), DevRun.ProfileId.IsEmpty() && DevRun.Hamster.HamsterId.IsNone());
	TestEqual(TEXT("developer run uses the base stats"), DevRun.Hamster.MaxHP, CRRun::BaseHamsterMaxHP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRosterAssetTest, "CardsRoguelike.Meta.Hamsters.DefaultRosterAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRosterAssetTest::RunTest(const FString& Parameters)
{
	const UCRHubCatalog* Catalog = LoadObject<UCRHubCatalog>(nullptr, CRMeta::HubCatalogPath());
	if (!TestNotNull(TEXT("catalog"), Catalog) || !TestNotNull(TEXT("catalog references a default roster"), Catalog->DefaultRoster.Get()))
	{
		return false;
	}
	const TArray<FCRHamsterDefinition>& Hamsters = Catalog->DefaultRoster->Hamsters;
	TestTrue(TEXT("about six prototype hamsters"), Hamsters.Num() >= 6);
	TSet<FName> Ids;
	TSet<int32> HPs;
	TSet<int32> Manas;
	for (const FCRHamsterDefinition& Def : Hamsters)
	{
		TestFalse(TEXT("unique id"), Def.HamsterId.IsNone() || Ids.Contains(Def.HamsterId));
		Ids.Add(Def.HamsterId);
		TestFalse(TEXT("has a name"), Def.DisplayName.IsEmpty());
		TestFalse(TEXT("has an epitaph"), Def.EpitaphText.IsEmpty());
		HPs.Add(Def.BaseMaxHP);
		Manas.Add(Def.BaseManaPerTurn);
	}
	TestTrue(TEXT("HP varies across the roster"), HPs.Num() > 2);
	TestTrue(TEXT("mana varies across the roster"), Manas.Num() > 1);
	return true;
}

// ---------------------------------------------------------------------------------------------------------
// M2.8: permanent death, graveyard, recruitment
// ---------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaDeathMigrationV3Test, "CardsRoguelike.Meta.Death.MigrationV2ToV3",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaDeathMigrationV3Test::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();

	// A version-2 profile as M2.7 wrote it: progress, a roster with edited hamster data, a selection, a last run.
	UCRProfileSaveGame* Old = MakeMetaTestProfile();
	Old->SaveVersion = 2;
	Old->ProfileId = TEXT("AUTOMATION_MIGRATION_V3");
	Old->DisplayName = TEXT("Старый хранитель");
	Old->CreatedAt = FDateTime(2026, 9, 1, 10, 0, 0);
	Old->LastPlayedAt = FDateTime(2026, 9, 29, 21, 30, 0);
	Old->Resources = MetaTestRes(41, 7, 9);
	Old->BuildingLevels.Add(TEXT("Heart"), 2);
	Old->BuildingLevels.Add(TEXT("Workshop"), 1);
	Old->UnlockedFlags.Add(TEXT("OldFlag"));
	Old->Counters.Add(TEXT("FutureCounter"), 5);
	Old->Stats.RunsStarted = 6;
	Old->Stats.RunsCompleted = 3;
	Old->Stats.RunsFailed = 2;
	Old->Stats.RunsAbandoned = 1;
	Old->LastRun.bValid = true;
	Old->LastRun.Reason = ECRRunEndReason::Completed;
	Old->LastRun.RunSeed = 777;
	Old->LastRun.Delivered = MetaTestRes(5, 1, 0);
	Old->bHamsterRosterInitialized = CRMeta::InitializeHamsterRoster(*Old, Catalog);
	Old->SelectedHamsterId = TEXT("Caster");
	FCRHamsterPersistentState& OldCaster = *CRMeta::FindHamster(*Old, TEXT("Caster"));
	OldCaster.AvatarTint = FLinearColor(0.1f, 0.2f, 0.9f);
	OldCaster.AvatarId = TEXT("Avatar_Caster");
	OldCaster.StartingDeckCardIds = { TEXT("Blast"), TEXT("Blast") };
	OldCaster.TraitIds = { TEXT("FutureTrait") };
	OldCaster.Stats.Add(TEXT("RunsStarted"), 4);
	const FString Slot = CRMeta::ProfileSlot(Old->ProfileId);
	TestTrue(TEXT("v2 profile saved"), UGameplayStatics::SaveGameToSlot(Old, Slot, CRMeta::SaveUserIndex));

	UCRProfileSaveGame* Loaded = Cast<UCRProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, CRMeta::SaveUserIndex));
	if (!TestNotNull(TEXT("v2 profile loads"), Loaded))
	{
		return false;
	}
	TestEqual(TEXT("loaded as version 2"), Loaded->SaveVersion, 2);
	TestTrue(TEXT("migration changes the profile"), CRMeta::MigrateProfile(*Loaded, Catalog));
	TestEqual(TEXT("version 3"), Loaded->SaveVersion, 3);
	TestEqual(TEXT("profile id"), Loaded->ProfileId, FString(TEXT("AUTOMATION_MIGRATION_V3")));
	TestEqual(TEXT("display name"), Loaded->DisplayName, FString(TEXT("Старый хранитель")));
	TestTrue(TEXT("created at"), Loaded->CreatedAt == FDateTime(2026, 9, 1, 10, 0, 0));
	TestTrue(TEXT("last played at"), Loaded->LastPlayedAt == FDateTime(2026, 9, 29, 21, 30, 0));
	TestTrue(TEXT("resources"), Loaded->Resources.Silver == 41 && Loaded->Resources.Food == 7 && Loaded->Resources.Wood == 9);
	TestTrue(TEXT("building levels"), Loaded->BuildingLevels.FindRef(TEXT("Heart")) == 2 && Loaded->BuildingLevels.FindRef(TEXT("Workshop")) == 1);
	TestTrue(TEXT("flags and counters"), Loaded->UnlockedFlags.Contains(TEXT("OldFlag")) && Loaded->Counters.FindRef(TEXT("FutureCounter")) == 5);
	TestTrue(TEXT("profile stats"), Loaded->Stats.RunsStarted == 6 && Loaded->Stats.RunsCompleted == 3 && Loaded->Stats.RunsFailed == 2 && Loaded->Stats.RunsAbandoned == 1);
	TestTrue(TEXT("last run"), Loaded->LastRun.bValid && Loaded->LastRun.Reason == ECRRunEndReason::Completed && Loaded->LastRun.RunSeed == 777 && Loaded->LastRun.Delivered.Silver == 5);
	TestEqual(TEXT("roster size"), Loaded->Hamsters.Num(), 3);
	TestEqual(TEXT("selection kept"), Loaded->SelectedHamsterId, FName(TEXT("Caster")));
	for (const FCRHamsterPersistentState& Hamster : Loaded->Hamsters)
	{
		TestTrue(*FString::Printf(TEXT("%s stays alive"), *Hamster.HamsterId.ToString()), Hamster.bAlive);
		TestFalse(*FString::Printf(TEXT("%s has no death record"), *Hamster.HamsterId.ToString()), Hamster.Death.bValid);
		TestFalse(*FString::Printf(TEXT("%s keeps the epitaph"), *Hamster.HamsterId.ToString()), Hamster.EpitaphText.IsEmpty());
	}
	const FCRHamsterPersistentState* Caster = CRMeta::FindHamster(*Loaded, TEXT("Caster"));
	if (TestNotNull(TEXT("caster kept"), Caster))
	{
		TestEqual(TEXT("name"), Caster->DisplayName, FString(TEXT("Искра")));
		TestTrue(TEXT("HP and mana"), Caster->BaseMaxHP == 24 && Caster->BaseManaPerTurn == 4);
		TestEqual(TEXT("epitaph text"), Caster->EpitaphText, FString(TEXT("Здесь лежит Искра.")));
		TestTrue(TEXT("avatar data"), Caster->AvatarTint.Equals(FLinearColor(0.1f, 0.2f, 0.9f)) && Caster->AvatarId == FName(TEXT("Avatar_Caster")));
		TestEqual(TEXT("own deck"), Caster->StartingDeckCardIds.Num(), 2);
		TestTrue(TEXT("future-schema data"), Caster->TraitIds.Contains(TEXT("FutureTrait")) && Caster->Stats.FindRef(TEXT("RunsStarted")) == 4);
	}
	TestEqual(TEXT("gains saved recruitment candidates"), Loaded->RecruitCandidates.Num(), 3);
	TestFalse(TEXT("migration runs exactly once"), CRMeta::MigrateProfile(*Loaded, Catalog));
	UGameplayStatics::DeleteGameInSlot(Slot, CRMeta::SaveUserIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaDeathCombatTest, "CardsRoguelike.Meta.Death.CombatDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaDeathCombatTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);
	Profile->Resources = MetaTestRes(10, 10, 10);

	TArray<FCRRunState> Ends;
	UCRRunSubsystem* Run = StartTestProfileRun(*Catalog, *Profile, TEXT("Tank"), ECRRoomType::Combat, Ends);
	const FName DeathNode = Run->GetRunState().CurrentNodeId;
	Run->AddCarriedResources(12, 3, 2);

	// What ACRCombatGameMode does on a defeat, then repeated signals.
	Run->SetHamsterHP(0);
	Run->FailCurrentRun(ECRHamsterDeathCause::Combat);
	Run->FailCurrentRun(ECRHamsterDeathCause::Combat);
	Run->FailCurrentRun(ECRHamsterDeathCause::Event);
	TestEqual(TEXT("the run end is reported once"), Ends.Num(), 1);
	if (Ends.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("reported as failed"), Ends[0].EndReason, ECRRunEndReason::Failed);
	TestEqual(TEXT("death cause combat"), Ends[0].DeathCause, ECRHamsterDeathCause::Combat);

	const FDateTime Now(2026, 9, 30, 12, 0, 0);
	TestTrue(TEXT("applied to the profile"), CRMeta::ApplyRunEnd(*Profile, Catalog, Ends[0], Now));
	const FCRHamsterPersistentState* Tank = CRMeta::FindHamster(*Profile, TEXT("Tank"));
	if (!TestNotNull(TEXT("dead hamster stays in the profile"), Tank))
	{
		return false;
	}
	TestFalse(TEXT("exactly the run hamster died"), Tank->bAlive);
	TestTrue(TEXT("the others live"), CRMeta::FindHamster(*Profile, TEXT("Caster"))->bAlive && CRMeta::FindHamster(*Profile, TEXT("Average"))->bAlive);
	TestTrue(TEXT("death record stored"), Tank->Death.bValid);
	TestEqual(TEXT("record: cause"), Tank->Death.DeathCause, ECRHamsterDeathCause::Combat);
	TestTrue(TEXT("record: timestamp"), Tank->Death.DeathTimestamp == Now);
	TestEqual(TEXT("record: seed"), Tank->Death.RunSeed, Ends[0].RunSeed);
	TestEqual(TEXT("record: node"), Tank->Death.NodeId, DeathNode);
	TestEqual(TEXT("record: room type"), Tank->Death.RoomType, ECRRoomType::Combat);
	TestTrue(TEXT("record: lost loot"), Tank->Death.LostLoot.Silver == 12 && Tank->Death.LostLoot.Food == 3 && Tank->Death.LostLoot.Wood == 2);
	TestEqual(TEXT("record: rooms visited"), Tank->Death.RoomsVisited, 1);

	TestTrue(TEXT("0 delivered"), Profile->LastRun.Delivered.IsZero());
	TestTrue(TEXT("profile resources unchanged"), Profile->Resources.Silver == 10 && Profile->Resources.Food == 10 && Profile->Resources.Wood == 10);
	TestEqual(TEXT("failed stat +1"), Profile->Stats.RunsFailed, 1);
	TestTrue(TEXT("last run failed, with the hamster"), Profile->LastRun.Reason == ECRRunEndReason::Failed && Profile->LastRun.HamsterId == FName(TEXT("Tank"))
		&& Profile->LastRun.HamsterName == FString(TEXT("Валун")) && Profile->LastRun.DeathCause == ECRHamsterDeathCause::Combat);
	TestEqual(TEXT("selection moved off the dead hamster"), Profile->SelectedHamsterId, FName(TEXT("Caster")));

	// A developer run (no profile) has no persistent hamster: nothing could be killed.
	TArray<FCRRunState> DevEnds;
	UCRRunSubsystem* Dev = MakeTestRunSubsystem();
	Dev->OnRunEnded.AddLambda([&DevEnds](const FCRRunState& Ended) { DevEnds.Add(Ended); });
	Dev->StartFreshRunWithSeed(99);
	Dev->FailCurrentRun(ECRHamsterDeathCause::Combat);
	TestTrue(TEXT("developer run has no profile or hamster"), DevEnds.Num() == 1 && DevEnds[0].ProfileId.IsEmpty() && DevEnds[0].Hamster.HamsterId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaDeathIdempotentTest, "CardsRoguelike.Meta.Death.ExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaDeathIdempotentTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);
	const FCRRunState Failed = MakeEndedRun(ECRRunEndReason::Failed, TEXT("Tank"), TEXT("RUN_A"), 5, 0, 0);
	const FDateTime FirstDeath(2026, 9, 30, 12, 0, 0);

	TestTrue(TEXT("first application"), CRMeta::ApplyRunEnd(*Profile, Catalog, Failed, FirstDeath));
	TestFalse(TEXT("second application ignored"), CRMeta::ApplyRunEnd(*Profile, Catalog, Failed, FDateTime(2026, 9, 30, 13, 0, 0)));
	TestEqual(TEXT("one failed-run stat"), Profile->Stats.RunsFailed, 1);
	TestTrue(TEXT("one death: timestamp of the first"), CRMeta::FindHamster(*Profile, TEXT("Tank"))->Death.DeathTimestamp == FirstDeath);
	TestEqual(TEXT("one grave"), CRMeta::GetGraveyard(*Profile).Num(), 1);

	// A second death record for the same hamster is refused (hub/run map reloads never kill again).
	FCRHamsterDeathRecord Again;
	Again.DeathCause = ECRHamsterDeathCause::Event;
	TestFalse(TEXT("an already dead hamster cannot die again"), CRMeta::KillHamster(*Profile, TEXT("Tank"), Again));
	TestEqual(TEXT("record unchanged"), CRMeta::FindHamster(*Profile, TEXT("Tank"))->Death.DeathCause, ECRHamsterDeathCause::Combat);

	// Reloading the profile (migration on select) neither revives nor re-applies.
	TestFalse(TEXT("migration on reload changes nothing"), CRMeta::MigrateProfile(*Profile, Catalog));
	TestFalse(TEXT("still dead after reload"), CRMeta::FindHamster(*Profile, TEXT("Tank"))->bAlive);

	// The next run is a new RunId and is applied normally.
	TestTrue(TEXT("a different run applies"), CRMeta::ApplyRunEnd(*Profile, Catalog, MakeEndedRun(ECRRunEndReason::Completed, TEXT("Caster"), TEXT("RUN_B"), 1, 0, 0), FirstDeath));
	TestEqual(TEXT("completed stat"), Profile->Stats.RunsCompleted, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaDeathSelectionTest, "CardsRoguelike.Meta.Death.SelectionAndLivingRoster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaDeathSelectionTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);
	const FCRHamsterDeathRecord Record;

	// Selected hamster dies -> another living one is selected.
	TestTrue(TEXT("kill the selected hamster"), CRMeta::KillHamster(*Profile, TEXT("Tank"), Record));
	TestEqual(TEXT("falls back to a living hamster"), Profile->SelectedHamsterId, FName(TEXT("Caster")));

	// The dead cannot be selected.
	TestFalse(TEXT("dead hamster cannot be selected"), CRMeta::SelectHamster(*Profile, TEXT("Tank")));
	TestEqual(TEXT("selection unchanged"), Profile->SelectedHamsterId, FName(TEXT("Caster")));
	Profile->SelectedHamsterId = TEXT("Tank"); // a stale save pointing at the dead
	TestNull(TEXT("a dead selection does not count"), CRMeta::FindSelectedHamster(*Profile));
	TestTrue(TEXT("repaired"), CRMeta::EnsureValidHamsterSelection(*Profile) && Profile->SelectedHamsterId == FName(TEXT("Caster")));
	TestTrue(TEXT("living hamsters can be selected"), CRMeta::SelectHamster(*Profile, TEXT("Average")));

	// Living roster excludes the dead (but the dead stay stored).
	TArray<const FCRHamsterPersistentState*> Living = CRMeta::GetLivingHamsters(*Profile);
	TestEqual(TEXT("two living"), Living.Num(), 2);
	TestFalse(TEXT("the dead are not in the living roster"), Living.ContainsByPredicate([](const FCRHamsterPersistentState* H) { return H->HamsterId == FName(TEXT("Tank")); }));
	TestEqual(TEXT("living count"), CRMeta::CountLivingHamsters(*Profile), 2);
	TestEqual(TEXT("the dead stay stored"), Profile->Hamsters.Num(), 3);

	// The last living hamster dies -> the selection is empty, safely.
	CRMeta::KillHamster(*Profile, TEXT("Average"), Record);
	CRMeta::KillHamster(*Profile, TEXT("Caster"), Record);
	TestTrue(TEXT("nobody selected"), Profile->SelectedHamsterId.IsNone());
	TestNull(TEXT("no selected hamster"), CRMeta::FindSelectedHamster(*Profile));
	TestEqual(TEXT("empty living roster"), CRMeta::GetLivingHamsters(*Profile).Num(), 0);
	TestFalse(TEXT("nothing to repair"), CRMeta::EnsureValidHamsterSelection(*Profile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaDeathGraveyardTest, "CardsRoguelike.Meta.Death.GraveyardAndEpitaph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaDeathGraveyardTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);

	TestEqual(TEXT("empty graveyard"), CRMeta::GetGraveyard(*Profile).Num(), 0);
	for (const FCRHamsterPersistentState& Hamster : Profile->Hamsters)
	{
		TestFalse(TEXT("every hamster has an epitaph from creation"), Hamster.EpitaphText.IsEmpty());
		TestTrue(TEXT("the epitaph is hidden while alive"), CRMeta::GetRevealedEpitaph(Hamster).IsEmpty());
	}

	CRMeta::ApplyRunEnd(*Profile, Catalog, MakeEndedRun(ECRRunEndReason::Failed, TEXT("Average"), TEXT("RUN_1"), 0, 0, 0), FDateTime(2026, 9, 30, 10, 0, 0));
	CRMeta::ApplyRunEnd(*Profile, Catalog, MakeEndedRun(ECRRunEndReason::Failed, TEXT("Tank"), TEXT("RUN_2"), 0, 0, 0), FDateTime(2026, 9, 30, 11, 0, 0));
	const TArray<const FCRHamsterPersistentState*> Graves = CRMeta::GetGraveyard(*Profile);
	TestEqual(TEXT("two graves"), Graves.Num(), 2);
	if (Graves.Num() == 2)
	{
		TestEqual(TEXT("newest death first"), Graves[0]->HamsterId, FName(TEXT("Tank")));
		TestEqual(TEXT("then the older one"), Graves[1]->HamsterId, FName(TEXT("Average")));
		TestEqual(TEXT("death reveals the stored epitaph"), CRMeta::GetRevealedEpitaph(*Graves[0]), FString(TEXT("Здесь лежит Валун.")));
		TestTrue(TEXT("graves carry their death record"), Graves[0]->Death.bValid && Graves[1]->Death.bValid);
	}
	TestTrue(TEXT("the living epitaph stays hidden"), CRMeta::GetRevealedEpitaph(*CRMeta::FindHamster(*Profile, TEXT("Caster"))).IsEmpty());
	TestEqual(TEXT("both deaths are new"), CRMeta::CountUnseenGraves(*Profile), 2);
	Profile->GraveyardSeenCount = 2;
	TestEqual(TEXT("seen after opening the graveyard"), CRMeta::CountUnseenGraves(*Profile), 0);
	TestEqual(TEXT("player-facing combat cause"), CRRun::DeathCauseDisplayText(ECRHamsterDeathCause::Combat), FString(TEXT("Погиб в бою")));
	TestEqual(TEXT("player-facing event cause"), CRRun::DeathCauseDisplayText(ECRHamsterDeathCause::Event), FString(TEXT("Погиб во время события")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaDeathSurvivalTest, "CardsRoguelike.Meta.Death.CompletedAndAbandonedSurvive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaDeathSurvivalTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	Catalog->CompletedRunKeepPercent = 100;
	Catalog->FailedRunKeepPercent = 0;
	Catalog->AbandonedRunKeepPercent = 0;
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);
	const FDateTime Now = FDateTime(2026, 9, 30, 12, 0, 0);

	// Completed: 100% delivered, the hamster lives and is not hurt.
	TestTrue(TEXT("completed applied"), CRMeta::ApplyRunEnd(*Profile, Catalog, MakeEndedRun(ECRRunEndReason::Completed, TEXT("Tank"), TEXT("RUN_C"), 8, 2, 1), Now));
	const FCRHamsterPersistentState* Tank = CRMeta::FindHamster(*Profile, TEXT("Tank"));
	TestTrue(TEXT("completed run does not kill"), Tank->bAlive && !Tank->Death.bValid);
	TestEqual(TEXT("base HP untouched"), Tank->BaseMaxHP, 36);
	TestTrue(TEXT("completed delivers 100%"), Profile->Resources.Silver == 8 && Profile->Resources.Food == 2 && Profile->Resources.Wood == 1);

	// Abandoned through the real run subsystem: the hamster lives, 0% delivered.
	TArray<FCRRunState> Ends;
	UCRRunSubsystem* Run = StartTestProfileRun(*Catalog, *Profile, TEXT("Caster"), ECRRoomType::Combat, Ends);
	Run->AddCarriedResources(6, 0, 0);
	Run->AbandonRun();
	TestEqual(TEXT("abandon reported once"), Ends.Num(), 1);
	if (Ends.Num() == 1)
	{
		TestEqual(TEXT("abandoned reason"), Ends[0].EndReason, ECRRunEndReason::Abandoned);
		TestTrue(TEXT("abandon applied"), CRMeta::ApplyRunEnd(*Profile, Catalog, Ends[0], Now));
	}
	TestTrue(TEXT("abandon does not kill"), CRMeta::FindHamster(*Profile, TEXT("Caster"))->bAlive);
	TestEqual(TEXT("abandon delivers 0%"), Profile->Resources.Silver, 8);
	TestEqual(TEXT("three living"), CRMeta::CountLivingHamsters(*Profile), 3);

	// Keep percentages: Completed 100, Failed 0, Abandoned 0.
	TestEqual(TEXT("completed keep"), CRMeta::GetKeepPercent(Catalog, ECRRunEndReason::Completed), 100);
	TestEqual(TEXT("failed keep"), CRMeta::GetKeepPercent(Catalog, ECRRunEndReason::Failed), 0);
	TestEqual(TEXT("abandoned keep"), CRMeta::GetKeepPercent(Catalog, ECRRunEndReason::Abandoned), 0);
	TestEqual(TEXT("failed keep without a catalog"), CRMeta::GetKeepPercent(nullptr, ECRRunEndReason::Failed), 0);
	TestEqual(TEXT("class default failed keep is 0"), GetDefault<UCRHubCatalog>()->FailedRunKeepPercent, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaDeathLethalEventTest, "CardsRoguelike.Meta.Death.LethalEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaDeathLethalEventTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);

	TArray<FCRRunState> Ends;
	UCRRunSubsystem* Run = StartTestProfileRun(*Catalog, *Profile, TEXT("Average"), ECRRoomType::Event, Ends);
	const FName EventNode = Run->GetRunState().CurrentNodeId;
	if (!TestTrue(TEXT("hamster stands in an event room"), Run->IsInEventRoom()))
	{
		return false;
	}
	Run->AddCarriedResources(9, 1, 0);

	// A transient event whose only choice costs more HP than the hamster has.
	UCREventDefinition* Event = NewObject<UCREventDefinition>(GetTransientPackage());
	Event->EventId = TEXT("LethalTest");
	FCREventChoice& Choice = Event->Choices.AddDefaulted_GetRef();
	FCREventEffect& Hurt = Choice.Effects.AddDefaulted_GetRef();
	Hurt.Type = ECREventEffectType::ModifyHP;
	Hurt.Amount = -5;
	FCREventNodeState& State = Run->GetMutableRunStateForTests().EventStates.FindOrAdd(EventNode);
	State.bInitialized = true;
	State.SelectedEvent = Event;
	Run->SetHamsterHP(3);

	TestTrue(TEXT("a lethal choice is allowed"), Run->GetEventChoiceBlockReason(Choice).IsEmpty());
	TestTrue(TEXT("the choice commits"), Run->CommitEventChoice(EventNode, Event, 0, INDEX_NONE));
	TestFalse(TEXT("it commits only once"), Run->CommitEventChoice(EventNode, Event, 0, INDEX_NONE));
	const FCREventNodeState* After = Run->FindEventState(EventNode);
	TestTrue(TEXT("the narrative result is kept for the result screen"), After && After->bEffectsCommitted && After->ResultLines.Num() > 0);
	TestEqual(TEXT("HP reached 0"), Run->GetRunState().Hamster.CurrentHP, 0);
	TestTrue(TEXT("run failed"), Run->IsRunFailed());
	TestFalse(TEXT("no further route progression"), Run->ContinueFromEvent(EventNode));
	TestEqual(TEXT("reported once"), Ends.Num(), 1);
	if (Ends.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("failed reason"), Ends[0].EndReason, ECRRunEndReason::Failed);
	TestEqual(TEXT("event death cause"), Ends[0].DeathCause, ECRHamsterDeathCause::Event);

	TestTrue(TEXT("applied"), CRMeta::ApplyRunEnd(*Profile, Catalog, Ends[0], FDateTime(2026, 9, 30, 12, 0, 0)));
	const FCRHamsterPersistentState* Average = CRMeta::FindHamster(*Profile, TEXT("Average"));
	TestFalse(TEXT("profile hamster dead"), Average->bAlive);
	TestEqual(TEXT("record: event cause"), Average->Death.DeathCause, ECRHamsterDeathCause::Event);
	TestEqual(TEXT("record: event room"), Average->Death.RoomType, ECRRoomType::Event);
	TestEqual(TEXT("record: lost loot"), Average->Death.LostLoot.Silver, 9);
	TestTrue(TEXT("0 delivered"), Profile->LastRun.Delivered.IsZero());
	TestEqual(TEXT("in the graveyard"), CRMeta::GetGraveyard(*Profile).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRecruitCandidatesTest, "CardsRoguelike.Meta.Recruitment.Candidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRecruitCandidatesTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	const UCRRecruitmentDefinition* Recruitment = Catalog->Recruitment;
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);

	TestEqual(TEXT("three candidates"), Profile->RecruitCandidates.Num(), 3);
	TestFalse(TEXT("already full: nothing to add"), CRMeta::RefillRecruitCandidates(*Profile, Recruitment));
	TSet<FName> Ids;
	TSet<FString> Names;
	for (const FCRHamsterPersistentState& Hamster : Profile->Hamsters)
	{
		Ids.Add(Hamster.HamsterId);
		Names.Add(Hamster.DisplayName);
	}
	for (const FCRHamsterPersistentState& Candidate : Profile->RecruitCandidates)
	{
		TestTrue(TEXT("generated id"), Candidate.HamsterId.ToString().StartsWith(TEXT("Recruit_")));
		TestFalse(TEXT("unique id"), Ids.Contains(Candidate.HamsterId));
		Ids.Add(Candidate.HamsterId);
		TestFalse(TEXT("name set"), Candidate.DisplayName.IsEmpty());
		TestFalse(TEXT("name not used by any hamster or candidate"), Names.Contains(Candidate.DisplayName));
		Names.Add(Candidate.DisplayName);
		TestTrue(TEXT("stats from a template"), Recruitment->StatTemplates.ContainsByPredicate([&Candidate](const FCRRecruitStatTemplate& T)
		{
			return T.BaseMaxHP == Candidate.BaseMaxHP && T.BaseManaPerTurn == Candidate.BaseManaPerTurn;
		}));
		TestFalse(TEXT("epitaph written at creation"), Candidate.EpitaphText.IsEmpty() || Candidate.EpitaphText.Contains(TEXT("{Name}")));
		TestTrue(TEXT("epitaph names the hamster"), Candidate.EpitaphText.Contains(Candidate.DisplayName));
		TestTrue(TEXT("placeholder tint from the options"), Recruitment->TintOptions.Contains(Candidate.AvatarTint));
		TestFalse(TEXT("avatar id"), Candidate.AvatarId.IsNone());
		TestTrue(TEXT("alive"), Candidate.bAlive);
	}
	TestFalse(TEXT("the roster name Валун is never offered"), Profile->RecruitCandidates.ContainsByPredicate(
		[](const FCRHamsterPersistentState& C) { return C.DisplayName == TEXT("Валун"); }));

	// An exhausted pool falls back to unique numbered names.
	UCRRecruitmentDefinition* Small = MakeMetaTestRecruitment();
	Small->NamePool = { TEXT("Валун") };
	UCRProfileSaveGame* Other = MakeMetaTestProfile();
	CRMeta::InitializeHamsterRoster(*Other, Catalog);
	CRMeta::RefillRecruitCandidates(*Other, Small);
	TSet<FString> FallbackNames;
	for (const FCRHamsterPersistentState& Candidate : Other->RecruitCandidates)
	{
		TestTrue(TEXT("fallback name"), Candidate.DisplayName.StartsWith(TEXT("Хомяк ")));
		FallbackNames.Add(Candidate.DisplayName);
	}
	TestEqual(TEXT("fallback names are unique"), FallbackNames.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRecruitTest, "CardsRoguelike.Meta.Recruitment.RecruitAndRosterCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRecruitTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	const UCRRecruitmentDefinition* Recruitment = Catalog->Recruitment;
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);

	const FCRHamsterPersistentState Offered = Profile->RecruitCandidates[1];
	TestTrue(TEXT("can recruit below the target"), CRMeta::CanRecruit(*Profile, Recruitment));
	TestTrue(TEXT("recruited"), CRMeta::RecruitCandidate(*Profile, Recruitment, Offered.HamsterId));
	const FCRHamsterPersistentState* Recruit = CRMeta::FindHamster(*Profile, Offered.HamsterId);
	if (!TestNotNull(TEXT("new roster member"), Recruit))
	{
		return false;
	}
	TestTrue(TEXT("alive"), Recruit->bAlive);
	TestTrue(TEXT("in the living roster"), CRMeta::GetLivingHamsters(*Profile).Contains(Recruit));
	TestTrue(TEXT("same data as offered"), Recruit->DisplayName == Offered.DisplayName && Recruit->BaseMaxHP == Offered.BaseMaxHP
		&& Recruit->BaseManaPerTurn == Offered.BaseManaPerTurn && Recruit->EpitaphText == Offered.EpitaphText && Recruit->AvatarTint.Equals(Offered.AvatarTint));
	TestFalse(TEXT("candidate removed"), Profile->RecruitCandidates.ContainsByPredicate([&Offered](const FCRHamsterPersistentState& C) { return C.HamsterId == Offered.HamsterId; }));
	TestEqual(TEXT("pool refilled to 3"), Profile->RecruitCandidates.Num(), 3);
	TestEqual(TEXT("the selection is kept"), Profile->SelectedHamsterId, FName(TEXT("Tank")));
	TestFalse(TEXT("unknown candidate refused"), CRMeta::RecruitCandidate(*Profile, Recruitment, TEXT("Recruit_Nobody")));

	// Up to the target (6 living), then no more.
	CRMeta::RecruitCandidate(*Profile, Recruitment, Profile->RecruitCandidates[0].HamsterId);
	CRMeta::RecruitCandidate(*Profile, Recruitment, Profile->RecruitCandidates[0].HamsterId);
	TestEqual(TEXT("six living"), CRMeta::CountLivingHamsters(*Profile), 6);
	TestFalse(TEXT("cannot recruit at the target"), CRMeta::CanRecruit(*Profile, Recruitment));
	TestFalse(TEXT("recruit above the target refused"), CRMeta::RecruitCandidate(*Profile, Recruitment, Profile->RecruitCandidates[0].HamsterId));
	TestEqual(TEXT("still six"), Profile->Hamsters.Num(), 6);

	// A death opens a place again.
	CRMeta::KillHamster(*Profile, TEXT("Tank"), FCRHamsterDeathRecord());
	TestTrue(TEXT("a death frees a place"), CRMeta::CanRecruit(*Profile, Recruitment));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRecruitEmptyRosterTest, "CardsRoguelike.Meta.Recruitment.EmptyRosterRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRecruitEmptyRosterTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);
	for (const FName Id : { FName(TEXT("Tank")), FName(TEXT("Caster")), FName(TEXT("Average")) })
	{
		CRMeta::KillHamster(*Profile, Id, FCRHamsterDeathRecord());
	}
	TestEqual(TEXT("nobody alive"), CRMeta::CountLivingHamsters(*Profile), 0);
	TestTrue(TEXT("empty selection"), Profile->SelectedHamsterId.IsNone());
	TestTrue(TEXT("recruitment stays available"), CRMeta::CanRecruit(*Profile, Catalog->Recruitment) && Profile->RecruitCandidates.Num() == 3);

	const FName RecruitId = Profile->RecruitCandidates[0].HamsterId;
	TestTrue(TEXT("recruited"), CRMeta::RecruitCandidate(*Profile, Catalog->Recruitment, RecruitId));
	TestEqual(TEXT("the recruit is selected automatically"), Profile->SelectedHamsterId, RecruitId);
	const FCRHamsterPersistentState* Selected = CRMeta::FindSelectedHamster(*Profile);
	TestTrue(TEXT("a valid selection: an expedition can start"), Selected && Selected->bAlive);
	if (Selected)
	{
		FCRRunState Next;
		CRRun::ApplyStartConfig(Next, CRMeta::BuildRunStartConfig(*Catalog, Profile->BuildingLevels, *Selected, Profile->ProfileId));
		TestEqual(TEXT("the run goes with the recruit"), Next.Hamster.HamsterId, RecruitId);
		TestEqual(TEXT("with the recruit's HP"), Next.Hamster.MaxHP, Selected->BaseMaxHP);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRecruitPersistenceTest, "CardsRoguelike.Meta.Recruitment.SaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRecruitPersistenceTest::RunTest(const FString& Parameters)
{
	UCRHubCatalog* Catalog = MakeDeathTestCatalog();
	UCRProfileSaveGame* Profile = MakeDeathTestProfile(Catalog);
	Profile->ProfileId = TEXT("AUTOMATION_RECRUIT");
	CRMeta::ApplyRunEnd(*Profile, Catalog, MakeEndedRun(ECRRunEndReason::Failed, TEXT("Tank"), TEXT("RUN_P"), 4, 0, 1), FDateTime(2026, 9, 30, 12, 0, 0));
	const FName RecruitId = Profile->RecruitCandidates[0].HamsterId;
	CRMeta::RecruitCandidate(*Profile, Catalog->Recruitment, RecruitId);
	const TArray<FCRHamsterPersistentState> Before = Profile->RecruitCandidates;
	const FString Slot = CRMeta::ProfileSlot(Profile->ProfileId);
	TestTrue(TEXT("saved"), UGameplayStatics::SaveGameToSlot(Profile, Slot, CRMeta::SaveUserIndex));

	UCRProfileSaveGame* Loaded = Cast<UCRProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, CRMeta::SaveUserIndex));
	if (!TestNotNull(TEXT("loaded"), Loaded))
	{
		return false;
	}
	TestFalse(TEXT("loading does not reroll"), CRMeta::MigrateProfile(*Loaded, Catalog));
	TestEqual(TEXT("same candidate count"), Loaded->RecruitCandidates.Num(), Before.Num());
	for (int32 i = 0; i < FMath::Min(Before.Num(), Loaded->RecruitCandidates.Num()); ++i)
	{
		const FCRHamsterPersistentState& A = Before[i];
		const FCRHamsterPersistentState& B = Loaded->RecruitCandidates[i];
		TestTrue(*FString::Printf(TEXT("candidate %d identical"), i), A.HamsterId == B.HamsterId && A.DisplayName == B.DisplayName && A.BaseMaxHP == B.BaseMaxHP
			&& A.BaseManaPerTurn == B.BaseManaPerTurn && A.EpitaphText == B.EpitaphText && A.AvatarTint.Equals(B.AvatarTint));
	}
	const FCRHamsterPersistentState* Recruit = CRMeta::FindHamster(*Loaded, RecruitId);
	TestTrue(TEXT("recruit persisted alive"), Recruit && Recruit->bAlive && !Recruit->EpitaphText.IsEmpty());
	const FCRHamsterPersistentState* Tank = CRMeta::FindHamster(*Loaded, TEXT("Tank"));
	TestTrue(TEXT("death persisted"), Tank && !Tank->bAlive && Tank->Death.bValid && Tank->Death.DeathCause == ECRHamsterDeathCause::Combat
		&& Tank->Death.LostLoot.Silver == 4 && Tank->Death.NodeId == FName(TEXT("L2_0")));
	TestTrue(TEXT("epitaph persisted"), Tank && CRMeta::GetRevealedEpitaph(*Tank) == TEXT("Здесь лежит Валун."));
	TestEqual(TEXT("applied run id persisted"), Loaded->LastAppliedRunId, FString(TEXT("RUN_P")));
	TestEqual(TEXT("last run hamster name persisted"), Loaded->LastRun.HamsterName, FString(TEXT("Tank")));
	UGameplayStatics::DeleteGameInSlot(Slot, CRMeta::SaveUserIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRMetaRecruitAssetTest, "CardsRoguelike.Meta.Recruitment.RecruitmentAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRMetaRecruitAssetTest::RunTest(const FString& Parameters)
{
	const UCRHubCatalog* Catalog = LoadObject<UCRHubCatalog>(nullptr, CRMeta::HubCatalogPath());
	if (!TestNotNull(TEXT("catalog"), Catalog))
	{
		return false;
	}
	TestEqual(TEXT("the real catalog keeps 0% on death"), Catalog->FailedRunKeepPercent, 0);
	TestEqual(TEXT("the real catalog keeps 100% on return"), Catalog->CompletedRunKeepPercent, 100);
	TestEqual(TEXT("the real catalog keeps 0% on abandon"), Catalog->AbandonedRunKeepPercent, 0);
	const UCRRecruitmentDefinition* Recruitment = Catalog->Recruitment.Get();
	if (!TestNotNull(TEXT("catalog references the recruitment asset"), Recruitment))
	{
		return false;
	}
	TestEqual(TEXT("3 candidates"), Recruitment->CandidateCount, 3);
	TestEqual(TEXT("roster target 6"), Recruitment->TargetLivingRosterSize, 6);
	TestTrue(TEXT("a name pool"), Recruitment->NamePool.Num() >= 12);
	TestTrue(TEXT("an epitaph pool"), Recruitment->EpitaphPool.Num() >= 6);
	TestTrue(TEXT("tints"), Recruitment->TintOptions.Num() >= 3);
	for (const FIntPoint& Stats : { FIntPoint(36, 2), FIntPoint(34, 2), FIntPoint(32, 3), FIntPoint(30, 3), FIntPoint(27, 4), FIntPoint(24, 4) })
	{
		TestTrue(*FString::Printf(TEXT("template %d HP / %d mana"), Stats.X, Stats.Y), Recruitment->StatTemplates.ContainsByPredicate(
			[&Stats](const FCRRecruitStatTemplate& T) { return T.BaseMaxHP == Stats.X && T.BaseManaPerTurn == Stats.Y; }));
	}
	for (const FString& Name : Recruitment->NamePool)
	{
		TestFalse(TEXT("pool names are not default-roster names"), Catalog->DefaultRoster && Catalog->DefaultRoster->Hamsters.ContainsByPredicate(
			[&Name](const FCRHamsterDefinition& D) { return D.DisplayName.Equals(Name, ESearchCase::IgnoreCase); }));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS