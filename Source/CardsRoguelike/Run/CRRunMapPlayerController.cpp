#include "CRRunMapPlayerController.h"

#include "Components/InputComponent.h"
#include "../Meta/CRMetaTypes.h"
#include "CRRunMapActor.h"
#include "CRRunMapHUD.h"
#include "CRRunMapGameMode.h"
#include "CRRunNodeActor.h"
#include "CRRunSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

ACRRunMapPlayerController::ACRRunMapPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ACRRunMapPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ACRRunMapPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Prototype bindings, same approach as the combat prototype.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACRRunMapPlayerController::OnLeftClick);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACRRunMapPlayerController::OnRestartRun);
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ACRRunMapPlayerController::OnToggleDebugView);
}

ACRRunMapActor* ACRRunMapPlayerController::GetMapActor() const
{
	const ACRRunMapGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRRunMapGameMode>() : nullptr;
	return GM ? GM->GetMapActor() : nullptr;
}

UCRRunSubsystem* ACRRunMapPlayerController::GetRunSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCRRunSubsystem>() : nullptr;
}

ACRRunNodeActor* ACRRunMapPlayerController::GetNodeUnderCursor() const
{
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return Cast<ACRRunNodeActor>(Hit.GetActor());
	}
	return nullptr;
}

void ACRRunMapPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	ACRRunMapActor* Map = GetMapActor();
	const UCRRunSubsystem* Run = GetRunSubsystem();
	if (!Map || !Run)
	{
		return;
	}

	// Only Available rooms react to hover; the map actor ignores hover while the marker moves.
	const ACRRunNodeActor* Node = GetNodeUnderCursor();
	Map->SetHoveredNode(Node && Run->CanTravelTo(Node->GetNodeId()) ? Node->GetNodeId() : NAME_None);
}

void ACRRunMapPlayerController::OnLeftClick()
{
	const ACRRunMapActor* Map = GetMapActor();
	UCRRunSubsystem* Run = GetRunSubsystem();

	// HUD buttons (profile runs only) take the click before the map does.
	float MouseX = 0.f;
	float MouseY = 0.f;
	ACRRunMapHUD* HUD = Cast<ACRRunMapHUD>(GetHUD());
	if (HUD && Run && GetMousePosition(MouseX, MouseY))
	{
		switch (HUD->HitTest(FVector2D(MouseX, MouseY)))
		{
		case ECRRunMapButton::ReturnToHub:
			// The finished run was already delivered to the profile; clearing it reports nothing more.
			Run->AbandonRun();
			UGameplayStatics::OpenLevel(this, FName(CRMeta::HubMapPath()));
			return;
		case ECRRunMapButton::LeaveRun:
			if (HUD->ConfirmLeave())
			{
				// Leaving mid-run is reported as Abandoned (the profile keeps AbandonedRunKeepPercent of the loot).
				Run->AbandonRun();
				UGameplayStatics::OpenLevel(this, FName(CRMeta::HubMapPath()));
			}
			return;
		default:
			break;
		}
	}

	const ACRRunNodeActor* Node = GetNodeUnderCursor();
	if (!Map || !Run || !Node || Map->IsMarkerMoving())
	{
		return;
	}

	// EnterNode rejects Locked, Completed and Current rooms, and anything after the run has ended.
	Run->EnterNode(Node->GetNodeId());
}

void ACRRunMapPlayerController::OnRestartRun()
{
	// Debug convenience for developer runs only: a run from the hub cannot be rerolled for free.
	if (UCRRunSubsystem* Run = GetRunSubsystem(); Run && !Run->IsProfileRun())
	{
		Run->AbandonRun();
		Run->StartFreshRun();
	}
}

void ACRRunMapPlayerController::OnToggleDebugView()
{
	bDebugView = !bDebugView;
}
