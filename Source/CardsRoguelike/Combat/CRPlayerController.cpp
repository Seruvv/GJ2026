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
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACRPlayerController::SelectCard1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ACRPlayerController::SelectCard2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACRPlayerController::SelectCard3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ACRPlayerController::SelectCard4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ACRPlayerController::SelectCard5);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACRPlayerController::OnLeftPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ACRPlayerController::OnLeftReleased);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ACRPlayerController::OnCancel);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ACRPlayerController::OnCancel);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACRPlayerController::OnEndTurn);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ACRPlayerController::CycleEdge1);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ACRPlayerController::CycleEdge2);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ACRPlayerController::CycleEdge3);
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

void ACRPlayerController::CycleEdge1()
{
	if (ACRCombatGameMode* GM = GetCombatMode()) { GM->CycleBoundaryType(0); }
}

void ACRPlayerController::CycleEdge2()
{
	if (ACRCombatGameMode* GM = GetCombatMode()) { GM->CycleBoundaryType(1); }
}

void ACRPlayerController::CycleEdge3()
{
	if (ACRCombatGameMode* GM = GetCombatMode()) { GM->CycleBoundaryType(2); }
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
			GM->ShowMessage(TEXT("Not enough mana"));
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
