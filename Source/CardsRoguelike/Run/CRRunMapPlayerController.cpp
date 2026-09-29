#include "CRRunMapPlayerController.h"

#include "Components/InputComponent.h"
#include "CRRunMapActor.h"
#include "CRRunMapGameMode.h"
#include "CRRunNodeActor.h"
#include "CRRunSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

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
	// Debug convenience: throw away the test run and start it again.
	if (UCRRunSubsystem* Run = GetRunSubsystem())
	{
		Run->AbandonRun();
		Run->StartFreshRun();
	}
}

void ACRRunMapPlayerController::OnToggleDebugView()
{
	bDebugView = !bDebugView;
}
