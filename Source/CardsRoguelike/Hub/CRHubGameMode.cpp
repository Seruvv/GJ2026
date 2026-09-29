#include "CRHubGameMode.h"

#include "../Meta/CRProfileSubsystem.h"
#include "../Run/CRRunTypes.h"
#include "../UI/CRUIPlayerController.h"
#include "CRHubHUD.h"
#include "CRHubRoomActor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogCRHub, Log, All);

ACRHubGameMode::ACRHubGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ACRUIPlayerController::StaticClass();
	HUDClass = ACRHubHUD::StaticClass();
}

UCRProfileSubsystem* ACRHubGameMode::GetProfiles() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCRProfileSubsystem>() : nullptr;
}

void ACRHubGameMode::StartPlay()
{
	Super::StartPlay();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Room = GetWorld()->SpawnActor<ACRHubRoomActor>(ACRHubRoomActor::StaticClass(), FTransform::Identity, Params);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTarget(Room);
	}

	if (UCRProfileSubsystem* Profiles = GetProfiles())
	{
		// Opened straight in the editor: fall back to the last selected profile.
		Profiles->EnsureActiveProfile();
		ProfileChangedHandle = Profiles->OnProfileChanged.AddUObject(this, &ACRHubGameMode::RefreshRoom);
		const UCRProfileSaveGame* Profile = Profiles->GetActiveProfile();
		UE_LOG(LogCRHub, Log, TEXT("Hub opened for %s"), Profile ? *FString::Printf(TEXT("%s '%s' (Silver %d, Food %d, Wood %d)"),
			*Profile->ProfileId, *Profile->DisplayName, Profile->Resources.Silver, Profile->Resources.Food, Profile->Resources.Wood) : TEXT("no active profile"));
	}
	RefreshRoom();
}

void ACRHubGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCRProfileSubsystem* Profiles = GetProfiles())
	{
		Profiles->OnProfileChanged.Remove(ProfileChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ACRHubGameMode::RestartPlayer(AController* NewPlayer)
{
	// No pawn: the view is the room's fixed camera.
}

void ACRHubGameMode::RefreshRoom()
{
	const UCRProfileSubsystem* Profiles = GetProfiles();
	const UCRHubCatalog* Catalog = Profiles ? Profiles->GetCatalog() : nullptr;
	if (!Room || !Catalog)
	{
		return;
	}
	for (const UCRHubBuildingDefinition* Building : Catalog->Buildings)
	{
		if (Building)
		{
			Room->SetAnchorLevel(Building->AnchorId, Profiles->GetBuildingLevel(*Building));
		}
	}
}

bool ACRHubGameMode::StartRun()
{
	UCRProfileSubsystem* Profiles = GetProfiles();
	if (!Profiles || !Profiles->StartRunFromHub())
	{
		return false;
	}
	UGameplayStatics::OpenLevel(this, FName(CRRun::RunMapPath()));
	return true;
}

void ACRHubGameMode::OpenMainMenu()
{
	UGameplayStatics::OpenLevel(this, FName(CRMeta::MainMenuMapPath()));
}
