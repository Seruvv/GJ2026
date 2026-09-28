// Shop input: mouse clicks on offers/heal/leave, keys 1-3 buy offers, 4 heals, Space leaves.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CRShopPlayerController.generated.h"

class ACRShopGameMode;

UCLASS()
class CARDSROGUELIKE_API ACRShopPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACRShopPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	ACRShopGameMode* GetShopMode() const;

	void OnLeftClick();
	void BuyOffer1();
	void BuyOffer2();
	void BuyOffer3();
	void BuyHeal();
	void Leave();
};
