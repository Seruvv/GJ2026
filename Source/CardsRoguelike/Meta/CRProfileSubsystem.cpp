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
	UE_LOG(LogCRProfile, Log, TEXT("Profile created: %s '%s' (Silver %d, Food %d, Wood %d)"), *ProfileId, *Profile->DisplayName,
		Profile->Resources.Silver, Profile->Resources.Food, Profile->Resources.Wood);
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
	UpgradeProfileData(*Profile);
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

void UCRProfileSubsystem::UpgradeProfileData(UCRProfileSaveGame& Profile) const
{
	// Version 1 is current. Buildings added after the profile was created simply use their StartLevel
	// (CRMeta::GetLevel), so no migration is needed for new hub content.
	Profile.SaveVersion = FMath::Max(Profile.SaveVersion, 1);
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

bool UCRProfileSubsystem::StartRunFromHub()
{
	UCRRunSubsystem* Run = GetGameInstance()->GetSubsystem<UCRRunSubsystem>();
	if (!ActiveProfile || !Run)
	{
		return false;
	}
	// Any leftover run (a finished one, or an unfinished one from before) is cleared first; an unfinished
	// run is reported as abandoned so its end is still accounted for.
	Run->AbandonRun();
	ActiveProfile->Stats.RunsStarted++;
	SaveActiveProfile();
	Run->StartProfileRun(ActiveProfile->ProfileId, GetRunStartBonuses());
	UE_LOG(LogCRProfile, Log, TEXT("Run %d started from the hub for %s"), ActiveProfile->Stats.RunsStarted, *ActiveProfile->ProfileId);
	return true;
}

void UCRProfileSubsystem::HandleRunEnded(const FCRRunState& EndedRun)
{
	if (EndedRun.ProfileId.IsEmpty())
	{
		return; // developer run opened straight on the run map
	}
	if ((!ActiveProfile || ActiveProfile->ProfileId != EndedRun.ProfileId) && !SelectProfile(EndedRun.ProfileId))
	{
		UE_LOG(LogCRProfile, Error, TEXT("Run ended for unknown profile %s; its resources are lost"), *EndedRun.ProfileId);
		return;
	}

	const UCRHubCatalog* Catalog = GetCatalog();
	int32 KeepPercent = 100;
	switch (EndedRun.EndReason)
	{
	case ECRRunEndReason::Completed: KeepPercent = Catalog ? Catalog->CompletedRunKeepPercent : 100; ActiveProfile->Stats.RunsCompleted++; break;
	case ECRRunEndReason::Failed:    KeepPercent = Catalog ? Catalog->FailedRunKeepPercent : 50;     ActiveProfile->Stats.RunsFailed++; break;
	case ECRRunEndReason::Abandoned: KeepPercent = Catalog ? Catalog->AbandonedRunKeepPercent : 0;   ActiveProfile->Stats.RunsAbandoned++; break;
	}

	FCRRunEndSummary Summary;
	Summary.bValid = true;
	Summary.Reason = EndedRun.EndReason;
	Summary.RunSeed = EndedRun.RunSeed;
	Summary.RoomsVisited = FMath::Max(0, EndedRun.VisitedNodeIds.Num() - 1);
	Summary.Carried.Silver = EndedRun.Carried.Silver;
	Summary.Carried.Food = EndedRun.Carried.Food;
	Summary.Carried.Wood = EndedRun.Carried.Wood;
	Summary.Delivered.Silver = CRMeta::ApplyKeepPercent(Summary.Carried.Silver, KeepPercent);
	Summary.Delivered.Food = CRMeta::ApplyKeepPercent(Summary.Carried.Food, KeepPercent);
	Summary.Delivered.Wood = CRMeta::ApplyKeepPercent(Summary.Carried.Wood, KeepPercent);

	ActiveProfile->Resources.Add(Summary.Delivered);
	ActiveProfile->LastRun = Summary;
	SaveActiveProfile();
	UE_LOG(LogCRProfile, Log, TEXT("Run end (%s) delivered to %s at %d%%: carried S%d F%d W%d -> +S%d F%d W%d, profile now Silver %d, Food %d, Wood %d"),
		*UEnum::GetValueAsString(EndedRun.EndReason), *ActiveProfile->ProfileId, KeepPercent,
		Summary.Carried.Silver, Summary.Carried.Food, Summary.Carried.Wood, Summary.Delivered.Silver, Summary.Delivered.Food, Summary.Delivered.Wood,
		ActiveProfile->Resources.Silver, ActiveProfile->Resources.Food, ActiveProfile->Resources.Wood);
	OnProfileChanged.Broadcast();
}
