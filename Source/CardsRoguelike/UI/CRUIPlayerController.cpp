#include "CRUIPlayerController.h"

#include "CRMenuHUDBase.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

ACRUIPlayerController::ACRUIPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ACRUIPlayerController::BeginPlay()
{
	Super::BeginPlay();
	RestoreDefaultInput();
}

void ACRUIPlayerController::RestoreDefaultInput()
{
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ACRUIPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACRUIPlayerController::OnLeftClick);
}

void ACRUIPlayerController::OnLeftClick()
{
	float X = 0.f;
	float Y = 0.f;
	if (ACRMenuHUDBase* HUD = Cast<ACRMenuHUDBase>(GetHUD()); HUD && GetMousePosition(X, Y))
	{
		HUD->HandleClick(FVector2D(X, Y));
	}
}
