#include "CRRunMapGameMode.h"

#include "CRRunMapActor.h"
#include "CRRunMapHUD.h"
#include "CRRunMapPlayerController.h"
#include "CRRunSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

ACRRunMapGameMode::ACRRunMapGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ACRRunMapPlayerController::StaticClass();
	HUDClass = ACRRunMapHUD::StaticClass();
}

void ACRRunMapGameMode::StartPlay()
{
	Super::StartPlay();

	// The run outlives this map; only start the test run when no run exists at all.
	// A failed run stays on screen until the player restarts it (R).
	if (UCRRunSubsystem* Run = GetGameInstance()->GetSubsystem<UCRRunSubsystem>())
	{
		if (!Run->HasRun())
		{
			Run->StartPrototypeRun();
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MapActor = GetWorld()->SpawnActor<ACRRunMapActor>(ACRRunMapActor::StaticClass(), FTransform::Identity, Params);

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTarget(MapActor);
	}
}

void ACRRunMapGameMode::RestartPlayer(AController* NewPlayer)
{
	// Strategic map: no pawn. The view target is the map actor's camera.
}
