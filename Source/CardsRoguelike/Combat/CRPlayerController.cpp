#include "CRPlayerController.h"

#include "CRCombatGameMode.h"
#include "CRDebugHUD.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

ACRPlayerController::ACRPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void ACRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ACRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Prototype bindings. Real card input will move to Enhanced Input assets.
	// F1-F9 are avoided: the engine's debug exec bindings claim several of them in PIE.
	// Number keys 1-9 (card slots; Shift+7/8/9 = debug edge cycling, see OnNumberKey).
	const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 Digit = 1; Digit <= 9; ++Digit)
	{
		FInputKeyBinding Binding(FInputChord(NumberKeys[Digit - 1]), IE_Pressed);
		Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Digit]() { OnNumberKey(Digit); });
		InputComponent->KeyBindings.Add(Binding);
	}
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACRPlayerController::OnLeftPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ACRPlayerController::OnLeftReleased);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ACRPlayerController::OnCancel);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ACRPlayerController::OnCancel);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACRPlayerController::OnEndTurn);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACRPlayerController::OnRestart);
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ACRPlayerController::OnToggleDebugView);
}

ACRCombatGameMode* ACRPlayerController::GetCombatMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACRCombatGameMode>() : nullptr;
}

ACRDebugHUD* ACRPlayerController::GetCombatHUD() const
{
	return Cast<ACRDebugHUD>(GetHUD());
}

bool ACRPlayerController::GetCursorWorldHit(FHitResult& OutHit) const
{
	return GetHitResultUnderCursor(ECC_Visibility, false, OutHit);
}

bool ACRPlayerController::IsCursorOverHand() const
{
	float X = 0.f;
	float Y = 0.f;
	const ACRDebugHUD* HUD = GetCombatHUD();
	// The End Turn button counts as UI too: a card released on it returns to the hand.
	return HUD && GetMousePosition(X, Y) && (HUD->IsOverHand(FVector2D(X, Y)) || HUD->GetEndTurnRect().IsInside(FVector2D(X, Y)));
}

void ACRPlayerController::SelectCard(int32 Index)
{
	if (ACRCombatGameMode* GM = GetCombatMode())
	{
		DraggedCard = INDEX_NONE;
		// During the reward phase 1-3 pick an offer instead of a hand card.
		if (GM->GetTurnState() == ECRTurnState::Reward)
		{
			GM->ChooseReward(Index);
			return;
		}
		GM->SelectCard(Index);
	}
}

bool ACRPlayerController::IsShiftDown() const
{
	return IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
}

void ACRPlayerController::OnNumberKey(int32 Digit)
{
	ACRCombatGameMode* GM = GetCombatMode();
	if (!GM)
	{
		return;
	}
	if (Digit >= 7 && GM->IsDebugView() && IsShiftDown())
	{
		GM->CycleBoundaryType(Digit - 7);
		return;
	}
	SelectCard(Digit - 1);
}

void ACRPlayerController::OnLeftPressed()
{
	ACRCombatGameMode* GM = GetCombatMode();
	const ACRDebugHUD* HUD = GetCombatHUD();
	if (!GM)
	{
		return;
	}

	float X = 0.f;
	float Y = 0.f;
	GetMousePosition(X, Y);

	if (GM->GetTurnState() == ECRTurnState::Reward)
	{
		// The GameMode ignores anything after the first commit.
		if (HUD && HUD->IsOverRewardSkip(FVector2D(X, Y)))
		{
			GM->SkipReward();
		}
		else if (HUD)
		{
			GM->ChooseReward(HUD->GetRewardIndexAt(FVector2D(X, Y)));
		}
		return;
	}

	if (GM->GetTurnState() != ECRTurnState::PlayerTurn)
	{
		return;
	}

	if (HUD && HUD->GetEndTurnRect().IsInside(FVector2D(X, Y)))
	{
		// Same path as Space; the button is inactive outside the player turn.
		OnEndTurn();
		return;
	}

	const int32 CardUnderCursor = HUD ? HUD->GetCardIndexAt(FVector2D(X, Y)) : INDEX_NONE;

	if (CardUnderCursor != INDEX_NONE)
	{
		// Picking up a card abandons any unfinished targeting (nothing was spent).
		GM->CancelTargeting();
		if (GM->CanAffordCard(CardUnderCursor))
		{
			DraggedCard = CardUnderCursor;
		}
		else
		{
			GM->ShowMessage(TEXT("Недостаточно маны"), TEXT("Not enough mana"));
		}
		return;
	}

	// PUSH stage 2 or a keyboard-selected card: the click targets the world.
	FHitResult Hit;
	if (GetCursorWorldHit(Hit))
	{
		GM->HandleClick(Hit.GetActor(), Hit.ImpactPoint);
	}
}

void ACRPlayerController::OnLeftReleased()
{
	const int32 Card = DraggedCard;
	DraggedCard = INDEX_NONE;

	ACRCombatGameMode* GM = GetCombatMode();
	if (Card == INDEX_NONE || !GM)
	{
		return;
	}

	if (IsCursorOverHand())
	{
		// Released back over the hand: the card simply returns.
		return;
	}

	FHitResult Hit;
	const bool bHitWorld = GetCursorWorldHit(Hit);
	GM->TryDropCard(Card, Hit.GetActor(), Hit.ImpactPoint, bHitWorld);
}

void ACRPlayerController::OnCancel()
{
	if (DraggedCard != INDEX_NONE)
	{
		DraggedCard = INDEX_NONE;
		return;
	}
	if (ACRCombatGameMode* GM = GetCombatMode())
	{
		GM->CancelTargeting();
	}
}

void ACRPlayerController::OnEndTurn()
{
	DraggedCard = INDEX_NONE;
	if (ACRCombatGameMode* GM = GetCombatMode())
	{
		// Space doubles as Skip on the reward panel.
		if (GM->GetTurnState() == ECRTurnState::Reward)
		{
			GM->SkipReward();
			return;
		}
		GM->RequestEndTurn();
	}
}

void ACRPlayerController::OnRestart()
{
	// A run room only ends through Victory/Defeat and the automatic return to the run map.
	// Restarting would reload the fight with the HP the hamster had when entering the room.
	const ACRCombatGameMode* GM = GetCombatMode();
	if (GM && GM->IsRunIntegrated())
	{
		return;
	}
	RestartLevel();
}

void ACRPlayerController::OnToggleDebugView()
{
	if (ACRCombatGameMode* GM = GetCombatMode())
	{
		GM->ToggleDebugView();
	}
}
