#include "CRMainMenuGameMode.h"

#include "../Hub/CRHubRoomActor.h"
#include "../Meta/CRProfileSubsystem.h"
#include "../Run/CRRunSubsystem.h"
#include "../UI/CRUIPlayerController.h"
#include "CRMainMenuHUD.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

ACRMainMenuGameMode::ACRMainMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ACRUIPlayerController::StaticClass();
	HUDClass = ACRMainMenuHUD::StaticClass();
}

UCRProfileSubsystem* ACRMainMenuGameMode::GetProfiles() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCRProfileSubsystem>() : nullptr;
}

void ACRMainMenuGameMode::StartPlay()
{
	Super::StartPlay();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Backdrop = GetWorld()->SpawnActor<ACRHubRoomActor>(ACRHubRoomActor::StaticClass(), FTransform::Identity, Params);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTarget(Backdrop);
	}
}

void ACRMainMenuGameMode::RestartPlayer(AController* NewPlayer)
{
	// No pawn.
}

bool ACRMainMenuGameMode::ContinueWithProfile(const FString& ProfileId)
{
	UCRProfileSubsystem* Profiles = GetProfiles();
	if (!Profiles || !Profiles->SelectProfile(ProfileId))
	{
		return false;
	}
	OpenHub();
	return true;
}

bool ACRMainMenuGameMode::CreateProfileAndEnter(const FString& DisplayName)
{
	UCRProfileSubsystem* Profiles = GetProfiles();
	if (!Profiles || Profiles->CreateProfile(DisplayName).IsEmpty())
	{
		return false;
	}
	OpenHub();
	return true;
}

void ACRMainMenuGameMode::OpenHub()
{
	// A run left over from another profile (e.g. the player went back to the menu mid-run) never leaks
	// into the next session: it ends here as abandoned for its own profile.
	if (UCRRunSubsystem* Run = GetGameInstance()->GetSubsystem<UCRRunSubsystem>())
	{
		Run->AbandonRun();
	}
	UGameplayStatics::OpenLevel(this, FName(CRMeta::HubMapPath()));
}

void ACRMainMenuGameMode::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}
