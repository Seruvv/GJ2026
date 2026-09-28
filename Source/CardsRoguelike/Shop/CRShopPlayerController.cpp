#include "CRShopPlayerController.h"

#include "Components/InputComponent.h"
#include "CRShopGameMode.h"
#include "CRShopHUD.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

ACRShopPlayerController::ACRShopPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ACRShopPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ACRShopPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Prototype bindings, same approach as combat and the run map.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACRShopPlayerController::OnLeftClick);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACRShopPlayerController::BuyOffer1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ACRShopPlayerController::BuyOffer2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACRShopPlayerController::BuyOffer3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ACRShopPlayerController::BuyHeal);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACRShopPlayerController::Leave);
}

ACRShopGameMode* ACRShopPlayerController::GetShopMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACRShopGameMode>() : nullptr;
}

void ACRShopPlayerController::OnLeftClick()
{
	ACRShopGameMode* GM = GetShopMode();
	const ACRShopHUD* HUD = Cast<ACRShopHUD>(GetHUD());
	float X = 0.f;
	float Y = 0.f;
	if (!GM || !HUD || !GetMousePosition(X, Y))
	{
		return;
	}

	switch (HUD->HitTest(FVector2D(X, Y)))
	{
	case ECRShopHitTarget::Offer0: GM->BuyCard(0); break;
	case ECRShopHitTarget::Offer1: GM->BuyCard(1); break;
	case ECRShopHitTarget::Offer2: GM->BuyCard(2); break;
	case ECRShopHitTarget::Heal:   GM->BuyHeal(); break;
	case ECRShopHitTarget::Leave:  GM->LeaveShop(); break;
	default: break;
	}
}

void ACRShopPlayerController::BuyOffer1() { if (ACRShopGameMode* GM = GetShopMode()) { GM->BuyCard(0); } }
void ACRShopPlayerController::BuyOffer2() { if (ACRShopGameMode* GM = GetShopMode()) { GM->BuyCard(1); } }
void ACRShopPlayerController::BuyOffer3() { if (ACRShopGameMode* GM = GetShopMode()) { GM->BuyCard(2); } }
void ACRShopPlayerController::BuyHeal() { if (ACRShopGameMode* GM = GetShopMode()) { GM->BuyHeal(); } }
void ACRShopPlayerController::Leave() { if (ACRShopGameMode* GM = GetShopMode()) { GM->LeaveShop(); } }
