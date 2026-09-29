#include "CREventPlayerController.h"

#include "Components/InputComponent.h"
#include "CREventGameMode.h"
#include "CREventHUD.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

namespace
{
	const FKey EventChoiceKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
}

ACREventPlayerController::ACREventPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ACREventPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ACREventPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Prototype bindings, same approach as the other rooms.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACREventPlayerController::OnLeftClick);
	for (const FKey& Key : EventChoiceKeys)
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &ACREventPlayerController::OnChooseKey);
	}
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACREventPlayerController::OnContinueKey);
	InputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &ACREventPlayerController::OnBackKey);
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ACREventPlayerController::OnScrollUp);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ACREventPlayerController::OnScrollDown);
	InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &ACREventPlayerController::OnPageUp);
	InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &ACREventPlayerController::OnPageDown);
}

ACREventGameMode* ACREventPlayerController::GetEventMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACREventGameMode>() : nullptr;
}

ACREventHUD* ACREventPlayerController::GetEventHUD() const
{
	return Cast<ACREventHUD>(GetHUD());
}

void ACREventPlayerController::OnLeftClick()
{
	ACREventGameMode* GM = GetEventMode();
	const ACREventHUD* HUD = GetEventHUD();
	float X = 0.f;
	float Y = 0.f;
	if (!GM || !HUD || !GetMousePosition(X, Y))
	{
		return;
	}

	const FCREventHit Hit = HUD->HitTest(FVector2D(X, Y));
	switch (Hit.Kind)
	{
	case ECREventHitKind::Choice:   GM->ChooseOption(Hit.Index); break;
	case ECREventHitKind::Card:     GM->SelectSacrifice(Hit.Index); break;
	case ECREventHitKind::Back:     GM->CancelCardSelection(); break;
	case ECREventHitKind::Continue: GM->Continue(); break;
	default: break;
	}
}

void ACREventPlayerController::OnChooseKey(FKey Key)
{
	// Number keys only ever pick a narrative choice (the game mode ignores them on other screens).
	ACREventGameMode* GM = GetEventMode();
	for (int32 i = 0; GM && i < UE_ARRAY_COUNT(EventChoiceKeys); ++i)
	{
		if (EventChoiceKeys[i] == Key)
		{
			GM->ChooseOption(i);
			return;
		}
	}
}

void ACREventPlayerController::OnContinueKey()
{
	// Space only continues from the result screen; it never selects a choice.
	if (ACREventGameMode* GM = GetEventMode())
	{
		GM->Continue();
	}
}

void ACREventPlayerController::OnBackKey()
{
	if (ACREventGameMode* GM = GetEventMode())
	{
		GM->CancelCardSelection();
	}
}

void ACREventPlayerController::OnScrollUp()
{
	if (ACREventHUD* HUD = GetEventHUD()) { HUD->Scroll(-1.f); }
}

void ACREventPlayerController::OnScrollDown()
{
	if (ACREventHUD* HUD = GetEventHUD()) { HUD->Scroll(1.f); }
}

void ACREventPlayerController::OnPageUp()
{
	if (ACREventHUD* HUD = GetEventHUD()) { HUD->Scroll(-6.f); }
}

void ACREventPlayerController::OnPageDown()
{
	if (ACREventHUD* HUD = GetEventHUD()) { HUD->Scroll(6.f); }
}
