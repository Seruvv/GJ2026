#include "CRProfileSubsystem.h"

#include "../Run/CRRunSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"

DEFINE_LOG_CATEGORY_STATIC(LogCRProfile, Log, All);

void UCRProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadIndex();

	// Runs report their end here, so resources reach the profile however the run ended.
	if (UCRRunSubsystem* Run = Collection.InitializeDependency<UCRRunSubsystem>())
	{
		RunEndedHandle = Run->OnRunEnded.AddUObject(this, &UCRProfileSubsystem::HandleRunEnded);
	}
}

void UCRProfileSubsystem::Deinitialize()
{
	if (UCRRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCRRunSubsystem>() : nullptr)
	{
		Run->OnRunEnded.Remove(RunEndedHandle);
	}
	Super::Deinitialize();
}

void UCRProfileSubsystem::LoadIndex()
{
	Index = Cast<UCRProfileIndexSaveGame>(UGameplayStatics::LoadGameFromSlot(CRMeta::ProfileIndexSlot(), CRMeta::SaveUserIndex));
	if (!Index)
	{
		Index = Cast<UCRProfileIndexSaveGame>(UGameplayStatics::CreateSaveGameObject(UCRProfileIndexSaveGame::StaticClass()));
	}

	// Drop index entries whose profile save is gone (e.g. deleted by hand), so the menu never lists a dead profile.
	const int32 Before = Index->Profiles.Num();
	Index->Profiles.RemoveAll([](const FCRProfileSummary& Summary)
	{
		return !UGameplayStatics::DoesSaveGameExist(CRMeta::ProfileSlot(Summary.ProfileId), CRMeta::SaveUserIndex);
	});
	if (Index->Profiles.Num() != Before)
	{
		UE_LOG(LogCRProfile, Warning, TEXT("Profile index: removed %d entries without a save file"), Before - Index->Profiles.Num());
		SaveIndex();
	}
	UE_LOG(LogCRProfile, Log, TEXT("Profile index loaded: %d profiles, last selected '%s'"), Index->Profiles.Num(), *Index->LastSelectedProfileId);
}

bool UCRProfileSubsystem::SaveIndex()
{
	const bool bSaved = Index && UGameplayStatics::SaveGameToSlot(Index, CRMeta::ProfileIndexSlot(), CRMeta::SaveUserIndex);
	if (!bSaved)
	{
		UE_LOG(LogCRProfile, Error, TEXT("Failed to save the profile index"));
	}
	return bSaved;
}

bool UCRProfileSubsystem::SaveActiveProfile()
{
	if (!ActiveProfile)
	{
		return false;
	}
	ActiveProfile->LastPlayedAt = FDateTime::Now();
	if (FCRProfileSummary* Summary = FindSummary(ActiveProfile->ProfileId))
	{
		Summary->LastPlayedAt = ActiveProfile->LastPlayedAt;
		Summary->DisplayName = ActiveProfile->DisplayName;
	}
	const bool bSaved = UGameplayStatics::SaveGameToSlot(ActiveProfile, CRMeta::ProfileSlot(ActiveProfile->ProfileId), CRMeta::SaveUserIndex);
	if (!bSaved)
	{
		UE_LOG(LogCRProfile, Error, TEXT("Failed to save profile %s"), *ActiveProfile->ProfileId);
	}
	SaveIndex();
	return bSaved;
}

FCRProfileSummary* UCRProfileSubsystem::FindSummary(const FString& ProfileId)
{
	return Index ? Index->Profiles.FindByPredicate([&ProfileId](const FCRProfileSummary& S) { return S.ProfileId == ProfileId; }) : nullptr;
}

const TArray<FCRProfileSummary>& UCRProfileSubsystem::GetProfiles() const
{
	static const TArray<FCRProfileSummary> Empty;
	return Index ? Index->Profiles : Empty;
}

FString UCRProfileSubsystem::GetLastSelectedProfileId() const
{
	return Index ? Index->LastSelectedProfileId : FString();
}

FString UCRProfileSubsystem::MakeDefaultProfileName() const
{
	return FString::Printf(TEXT("Хранитель %d"), Index ? Index->NextProfileNumber : 1);
}

FString UCRProfileSubsystem::CreateProfile(const FString& DisplayName)
{
	if (!Index)
	{
		return FString();
	}

	const int32 Number = Index->NextProfileNumber++;
	// Filename-safe, unique even if numbers are reused by a reset index.
	const FString ProfileId = FString::Printf(TEXT("P%03d_%s"), Number, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));

	UCRProfileSaveGame* Profile = Cast<UCRProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UCRProfileSaveGame::StaticClass()));
	Profile->ProfileId = ProfileId;
	Profile->DisplayName = DisplayName.TrimStartAndEnd().IsEmpty() ? FString::Printf(TEXT("Хранитель %d"), Number) : DisplayName.TrimStartAndEnd().Left(24);
	Profile->CreatedAt = FDateTime::Now();
	Profile->LastPlayedAt = Profile->CreatedAt;
	if (const UCRHubCatalog* Catalog = GetCatalog())
	{
		Profile->Resources = Catalog->StartingResources;
		for (const UCRHubBuildingDefinition* Building : Catalog->Buildings)
		{
			if (Building)
			{
				Profile->BuildingLevels.Add(Building->BuildingId, Building->StartLevel);
			}
		}
	}

	// Hamsters: the default roster, with the first one selected, and the saved recruitment candidates.
	Profile->SaveVersion = CRMeta::CurrentProfileSaveVersion;
	Profile->bHamsterRosterInitialized = CRMeta::InitializeHamsterRoster(*Profile, GetCatalog());
	CRMeta::EnsureValidHamsterSelection(*Profile);
	CRMeta::RefillRecruitCandidates(*Profile, GetRecruitment());

	FCRProfileSummary Summary;
	Summary.ProfileId = ProfileId;
	Summary.DisplayName = Profile->DisplayName;
	Summary.CreatedAt = Profile->CreatedAt;
	Summary.LastPlayedAt = Profile->LastPlayedAt;
	Index->Profiles.Add(Summary);
	Index->LastSelectedProfileId = ProfileId;

	ActiveProfile = Profile;
	RefreshUnlockedFlags();
	if (!SaveActiveProfile())
	{
		return FString();
	}
	UE_LOG(LogCRProfile, Log, TEXT("Profile created: %s '%s' (Silver %d, Food %d, Wood %d, %d hamsters, selected %s)"), *ProfileId, *Profile->DisplayName,
		Profile->Resources.Silver, Profile->Resources.Food, Profile->Resources.Wood, Profile->Hamsters.Num(), *Profile->SelectedHamsterId.ToString());
	OnProfileChanged.Broadcast();
	return ProfileId;
}

bool UCRProfileSubsystem::SelectProfile(const FString& ProfileId)
{
	if (!FindSummary(ProfileId))
	{
		return false;
	}
	UCRProfileSaveGame* Profile = Cast<UCRProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(CRMeta::ProfileSlot(ProfileId), CRMeta::SaveUserIndex));
	if (!Profile)
	{
		UE_LOG(LogCRProfile, Error, TEXT("Profile %s could not be loaded"), *ProfileId);
		return false;
	}
	const int32 VersionBefore = Profile->SaveVersion;
	const bool bHadRoster = Profile->bHamsterRosterInitialized;
	if (CRMeta::MigrateProfile(*Profile, GetCatalog()))
	{
		UE_LOG(LogCRProfile, Log, TEXT("Profile %s migrated: version %d -> %d, roster %s (%d hamsters, %d alive), %d recruit candidates, selected %s"),
			*ProfileId, VersionBefore, Profile->SaveVersion, bHadRoster ? TEXT("kept") : TEXT("added"), Profile->Hamsters.Num(),
			CRMeta::CountLivingHamsters(*Profile), Profile->RecruitCandidates.Num(), *Profile->SelectedHamsterId.ToString());
	}
	ActiveProfile = Profile;
	Index->LastSelectedProfileId = ProfileId;
	RefreshUnlockedFlags();
	SaveActiveProfile();
	UE_LOG(LogCRProfile, Log, TEXT("Profile selected: %s '%s' (Silver %d, Food %d, Wood %d, runs %d)"), *ProfileId, *Profile->DisplayName,
		Profile->Resources.Silver, Profile->Resources.Food, Profile->Resources.Wood, Profile->Stats.RunsStarted);
	OnProfileChanged.Broadcast();
	return true;
}

bool UCRProfileSubsystem::DeleteProfile(const FString& ProfileId)
{
	if (!Index || !FindSummary(ProfileId))
	{
		return false;
	}
	UGameplayStatics::DeleteGameInSlot(CRMeta::ProfileSlot(ProfileId), CRMeta::SaveUserIndex);
	Index->Profiles.RemoveAll([&ProfileId](const FCRProfileSummary& S) { return S.ProfileId == ProfileId; });
	if (Index->LastSelectedProfileId == ProfileId)
	{
		Index->LastSelectedProfileId = Index->Profiles.Num() > 0 ? Index->Profiles.Last().ProfileId : FString();
	}
	if (ActiveProfile && ActiveProfile->ProfileId == ProfileId)
	{
		ActiveProfile = nullptr;
	}
	SaveIndex();
	UE_LOG(LogCRProfile, Log, TEXT("Profile deleted: %s (%d left)"), *ProfileId, Index->Profiles.Num());
	OnProfileChanged.Broadcast();
	return true;
}

bool UCRProfileSubsystem::EnsureActiveProfile()
{
	return ActiveProfile || (!GetLastSelectedProfileId().IsEmpty() && SelectProfile(GetLastSelectedProfileId()));
}

void UCRProfileSubsystem::RefreshUnlockedFlags()
{
	const UCRHubCatalog* Catalog = GetCatalog();
	if (!ActiveProfile || !Catalog)
	{
		return;
	}
	for (const FName Flag : CRMeta::ComputeUnlockedFlags(*Catalog, ActiveProfile->BuildingLevels))
	{
		ActiveProfile->UnlockedFlags.AddUnique(Flag);
	}
}

const UCRHubCatalog* UCRProfileSubsystem::GetCatalog() const
{
	if (!CachedCatalog)
	{
		CachedCatalog = LoadObject<UCRHubCatalog>(nullptr, CRMeta::HubCatalogPath());
		if (!CachedCatalog)
		{
			UE_LOG(LogCRProfile, Error, TEXT("Hub catalog %s is missing"), CRMeta::HubCatalogPath());
		}
	}
	return CachedCatalog;
}

int32 UCRProfileSubsystem::GetBuildingLevel(const UCRHubBuildingDefinition& Building) const
{
	return ActiveProfile ? CRMeta::GetLevel(ActiveProfile->BuildingLevels, Building) : Building.StartLevel;
}

ECRUpgradeStatus UCRProfileSubsystem::GetUpgradeStatus(const UCRHubBuildingDefinition& Building, const FCRHubBuildingLevel** OutNextLevel) const
{
	const UCRHubCatalog* Catalog = GetCatalog();
	if (!ActiveProfile || !Catalog)
	{
		return ECRUpgradeStatus::Invalid;
	}
	return CRMeta::GetUpgradeStatus(*Catalog, ActiveProfile->BuildingLevels, ActiveProfile->Resources, Building, OutNextLevel);
}

bool UCRProfileSubsystem::TryUpgrade(FName BuildingId, FString& OutMessage)
{
	const UCRHubCatalog* Catalog = GetCatalog();
	const UCRHubBuildingDefinition* Building = Catalog ? Catalog->FindBuilding(BuildingId) : nullptr;
	if (!ActiveProfile || !Building)
	{
		OutMessage = TEXT("Улучшение недоступно");
		return false;
	}

	const FCRHubBuildingLevel* Next = nullptr;
	switch (GetUpgradeStatus(*Building, &Next))
	{
	case ECRUpgradeStatus::Available:
		break;
	case ECRUpgradeStatus::MaxLevel:
		OutMessage = FString::Printf(TEXT("%s: достигнут максимальный уровень"), *Building->DisplayName);
		return false;
	case ECRUpgradeStatus::Locked:
	{
		const UCRHubBuildingDefinition* Required = Catalog->FindBuilding(Next->RequiredBuildingId);
		OutMessage = FString::Printf(TEXT("Сначала улучшите «%s» до уровня %d"), Required ? *Required->DisplayName : TEXT("?"), Next->RequiredLevel);
		return false;
	}
	case ECRUpgradeStatus::NotEnoughResources:
		OutMessage = TEXT("Недостаточно ресурсов");
		return false;
	default:
		OutMessage = TEXT("Улучшение недоступно");
		return false;
	}

	const int32 NewLevel = GetBuildingLevel(*Building) + 1;
	ActiveProfile->Resources.Subtract(Next->Cost);
	ActiveProfile->BuildingLevels.Add(BuildingId, NewLevel);
	RefreshUnlockedFlags();
	SaveActiveProfile();
	UE_LOG(LogCRProfile, Log, TEXT("Upgraded %s to level %d for %s (paid Silver %d, Food %d, Wood %d)"), *BuildingId.ToString(), NewLevel,
		*ActiveProfile->ProfileId, Next->Cost.Silver, Next->Cost.Food, Next->Cost.Wood);
	OutMessage = FString::Printf(TEXT("%s: уровень %d"), *Building->DisplayName, NewLevel);
	OnProfileChanged.Broadcast();
	return true;
}

bool UCRProfileSubsystem::TryConvert(FName BuildingId, FName ConversionId, FString& OutMessage)
{
	const UCRHubCatalog* Catalog = GetCatalog();
	const UCRHubBuildingDefinition* Building = Catalog ? Catalog->FindBuilding(BuildingId) : nullptr;
	const TArray<FCRHubConversion>* Conversions = (ActiveProfile && Building) ? CRMeta::GetConversions(ActiveProfile->BuildingLevels, *Building) : nullptr;
	const FCRHubConversion* Conversion = Conversions ? Conversions->FindByPredicate([ConversionId](const FCRHubConversion& C) { return C.ConversionId == ConversionId; }) : nullptr;
	if (!Conversion)
	{
		OutMessage = TEXT("Обмен недоступен");
		return false;
	}
	if (!ActiveProfile->Resources.CanAfford(Conversion->Input))
	{
		OutMessage = TEXT("Недостаточно ресурсов для обмена");
		return false;
	}

	ActiveProfile->Resources.Subtract(Conversion->Input);
	ActiveProfile->Resources.Add(Conversion->Output);
	SaveActiveProfile();
	UE_LOG(LogCRProfile, Log, TEXT("Conversion %s/%s for %s: now Silver %d, Food %d, Wood %d"), *BuildingId.ToString(), *ConversionId.ToString(),
		*ActiveProfile->ProfileId, ActiveProfile->Resources.Silver, ActiveProfile->Resources.Food, ActiveProfile->Resources.Wood);
	OutMessage = FString::Printf(TEXT("Отдано: %s. Получено: %s"), *CRMeta::FormatResources(Conversion->Input), *CRMeta::FormatResources(Conversion->Output));
	OnProfileChanged.Broadcast();
	return true;
}

FCRRunStartBonuses UCRProfileSubsystem::GetRunStartBonuses() const
{
	const UCRHubCatalog* Catalog = GetCatalog();
	return (ActiveProfile && Catalog) ? CRMeta::ComputeRunStartBonuses(*Catalog, ActiveProfile->BuildingLevels) : FCRRunStartBonuses();
}

const FCRHamsterPersistentState* UCRProfileSubsystem::GetSelectedHamster() const
{
	return ActiveProfile ? CRMeta::FindSelectedHamster(*ActiveProfile) : nullptr;
}

bool UCRProfileSubsystem::SelectHamster(FName HamsterId)
{
	const FName Before = ActiveProfile ? ActiveProfile->SelectedHamsterId : NAME_None;
	if (!ActiveProfile || !CRMeta::SelectHamster(*ActiveProfile, HamsterId))
	{
		return false; // unknown or dead: the dead are never selected
	}
	if (Before != HamsterId)
	{
		const FCRHamsterPersistentState* Hamster = CRMeta::FindHamster(*ActiveProfile, HamsterId);
		SaveActiveProfile();
		UE_LOG(LogCRProfile, Log, TEXT("Hamster selected for %s: %s (HP %d, mana %d)"), *ActiveProfile->ProfileId, *HamsterId.ToString(),
			Hamster->BaseMaxHP, Hamster->BaseManaPerTurn);
		OnProfileChanged.Broadcast();
	}
	return true;
}

bool UCRProfileSubsystem::GetNextRunStartConfig(FCRRunStartConfig& OutConfig) const
{
	const UCRHubCatalog* Catalog = GetCatalog();
	const FCRHamsterPersistentState* Hamster = GetSelectedHamster();
	if (!ActiveProfile || !Catalog || !Hamster)
	{
		return false;
	}
	OutConfig = CRMeta::BuildRunStartConfig(*Catalog, ActiveProfile->BuildingLevels, *Hamster, ActiveProfile->ProfileId);
	return true;
}

int32 UCRProfileSubsystem::GetLivingHamsterCount() const
{
	return ActiveProfile ? CRMeta::CountLivingHamsters(*ActiveProfile) : 0;
}

int32 UCRProfileSubsystem::GetUnseenGraveCount() const
{
	return ActiveProfile ? CRMeta::CountUnseenGraves(*ActiveProfile) : 0;
}

void UCRProfileSubsystem::MarkGraveyardSeen()
{
	if (!ActiveProfile || CRMeta::CountUnseenGraves(*ActiveProfile) == 0)
	{
		return;
	}
	ActiveProfile->GraveyardSeenCount = ActiveProfile->Hamsters.Num() - CRMeta::CountLivingHamsters(*ActiveProfile);
	SaveActiveProfile();
	OnProfileChanged.Broadcast();
}

const UCRRecruitmentDefinition* UCRProfileSubsystem::GetRecruitment() const
{
	const UCRHubCatalog* Catalog = GetCatalog();
	return Catalog ? Catalog->Recruitment.Get() : nullptr;
}

bool UCRProfileSubsystem::CanRecruit() const
{
	return ActiveProfile && CRMeta::CanRecruit(*ActiveProfile, GetRecruitment());
}

bool UCRProfileSubsystem::RecruitCandidate(FName CandidateId, FString& OutMessage)
{
	const UCRRecruitmentDefinition* Recruitment = GetRecruitment();
	if (!ActiveProfile || !Recruitment)
	{
		OutMessage = TEXT("Пополнение недоступно");
		return false;
	}
	if (!CRMeta::CanRecruit(*ActiveProfile, Recruitment))
	{
		OutMessage = TEXT("В убежище достаточно хомяков");
		return false;
	}
	const FCRHamsterPersistentState* Candidate = ActiveProfile->RecruitCandidates.FindByPredicate(
		[CandidateId](const FCRHamsterPersistentState& C) { return C.HamsterId == CandidateId; });
	const FString Name = Candidate ? Candidate->DisplayName : FString();
	if (!CRMeta::RecruitCandidate(*ActiveProfile, Recruitment, CandidateId))
	{
		OutMessage = TEXT("Этого хомяка нельзя принять");
		return false;
	}
	SaveActiveProfile();
	const FCRHamsterPersistentState* Recruit = CRMeta::FindHamster(*ActiveProfile, CandidateId);
	UE_LOG(LogCRProfile, Log, TEXT("Recruited %s '%s' (HP %d, mana %d) into %s: %d alive, selected %s, %d candidates"), *CandidateId.ToString(), *Name,
		Recruit ? Recruit->BaseMaxHP : 0, Recruit ? Recruit->BaseManaPerTurn : 0, *ActiveProfile->ProfileId, CRMeta::CountLivingHamsters(*ActiveProfile),
		*ActiveProfile->SelectedHamsterId.ToString(), ActiveProfile->RecruitCandidates.Num());
	OutMessage = FString::Printf(TEXT("%s теперь живёт в убежище"), *Name);
	OnProfileChanged.Broadcast();
	return true;
}

bool UCRProfileSubsystem::StartRunFromHub()
{
	UCRRunSubsystem* Run = GetGameInstance()->GetSubsystem<UCRRunSubsystem>();
	if (!ActiveProfile || !Run)
	{
		return false;
	}
	// Only a living hamster can go: repair an invalid or dead selection first, refuse if nobody is alive.
	if (CRMeta::EnsureValidHamsterSelection(*ActiveProfile))
	{
		SaveActiveProfile();
		OnProfileChanged.Broadcast();
	}
	FCRRunStartConfig Config;
	if (!GetNextRunStartConfig(Config))
	{
		UE_LOG(LogCRProfile, Warning, TEXT("Run not started for %s: no living hamster"), *ActiveProfile->ProfileId);
		return false;
	}
	// Any leftover run (a finished one, or an unfinished one from before) is cleared first; an unfinished
	// run is reported as abandoned so its end is still accounted for.
	Run->AbandonRun();
	ActiveProfile->Stats.RunsStarted++;
	CRMeta::RecordHamsterRunStart(*ActiveProfile, Config.HamsterId);
	SaveActiveProfile();
	Run->StartProfileRun(Config);
	UE_LOG(LogCRProfile, Log, TEXT("Run %d started from the hub for %s with %s"), ActiveProfile->Stats.RunsStarted, *ActiveProfile->ProfileId,
		*Config.HamsterId.ToString());
	return true;
}

void UCRProfileSubsystem::HandleRunEnded(const FCRRunState& EndedRun)
{
	if (EndedRun.ProfileId.IsEmpty())
	{
		return; // developer run opened straight on the run map: no persistent hamster, no persistent death
	}
	if ((!ActiveProfile || ActiveProfile->ProfileId != EndedRun.ProfileId) && !SelectProfile(EndedRun.ProfileId))
	{
		UE_LOG(LogCRProfile, Error, TEXT("Run ended for unknown profile %s; its resources are lost"), *EndedRun.ProfileId);
		return;
	}

	const UCRHubCatalog* Catalog = GetCatalog();
	if (!CRMeta::ApplyRunEnd(*ActiveProfile, Catalog, EndedRun, FDateTime::Now()))
	{
		UE_LOG(LogCRProfile, Warning, TEXT("Run %s end was already applied to %s; ignored"), *EndedRun.RunId, *ActiveProfile->ProfileId);
		return;
	}
	SaveActiveProfile();
	const FCRRunEndSummary& Summary = ActiveProfile->LastRun;
	UE_LOG(LogCRProfile, Log, TEXT("Run end (%s) delivered to %s at %d%%: carried S%d F%d W%d -> +S%d F%d W%d, profile now Silver %d, Food %d, Wood %d"),
		*UEnum::GetValueAsString(EndedRun.EndReason), *ActiveProfile->ProfileId, CRMeta::GetKeepPercent(Catalog, EndedRun.EndReason),
		Summary.Carried.Silver, Summary.Carried.Food, Summary.Carried.Wood, Summary.Delivered.Silver, Summary.Delivered.Food, Summary.Delivered.Wood,
		ActiveProfile->Resources.Silver, ActiveProfile->Resources.Food, ActiveProfile->Resources.Wood);
	if (EndedRun.EndReason == ECRRunEndReason::Failed)
	{
		UE_LOG(LogCRProfile, Log, TEXT("Hamster %s '%s' died permanently (%s in %s); %d alive, selected now %s"), *EndedRun.Hamster.HamsterId.ToString(),
			*EndedRun.Hamster.Name, *UEnum::GetValueAsString(EndedRun.DeathCause), *EndedRun.CurrentNodeId.ToString(),
			CRMeta::CountLivingHamsters(*ActiveProfile), *ActiveProfile->SelectedHamsterId.ToString());
	}
	OnProfileChanged.Broadcast();
}
