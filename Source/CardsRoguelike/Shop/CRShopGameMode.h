// Shop room GameMode: validates the run, loads the persistent shop state, handles purchases and leaving.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRShopGameMode.generated.h"

class ACRShopMerchant;
class ACRShopRoomActor;
class UCRRunSubsystem;
struct FCRShopState;

UENUM()
enum class ECRShopOfferStatus : uint8
{
	Available,
	Sold,
	NotEnoughSilver,
	FullHP,
	Unavailable
};

UCLASS()
class CARDSROGUELIKE_API ACRShopGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRShopGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// Player actions (ignored after Leave has been committed)
	void BuyCard(int32 OfferIndex);
	void BuyHeal();
	void LeaveShop();

	// Queries for the HUD
	/** True when this map is a real shop room of an active run (not a direct sandbox open). */
	bool IsValidShop() const { return bValidShop; }
	bool IsLeaving() const { return bLeaving; }
	const FCRShopState* GetShopState() const;
	FName GetShopNodeId() const { return ShopNodeId; }
	int32 GetSilver() const;
	int32 GetHP() const;
	int32 GetMaxHP() const;
	ECRShopOfferStatus GetCardStatus(int32 OfferIndex) const;
	ECRShopOfferStatus GetHealStatus() const;
	FString GetActiveMessage() const;

	/** Prototype tuning: shop prices are Silver and unrelated to combat mana costs. */
	UPROPERTY(EditAnywhere, Category = "CR|Shop")
	int32 CardPrice = 3;

	UPROPERTY(EditAnywhere, Category = "CR|Shop")
	int32 HealPrice = 2;

	UPROPERTY(EditAnywhere, Category = "CR|Shop")
	int32 HealAmount = 5;

	UPROPERTY(EditAnywhere, Category = "CR|Shop")
	int32 OfferCount = 3;

	UPROPERTY(EditAnywhere, Category = "CR|Shop")
	float LeaveDelay = 0.5f;

private:
	UCRRunSubsystem* GetRun() const;
	void ShowMessage(const FString& Message);
	void ReturnToRunMap();

	UPROPERTY()
	TObjectPtr<ACRShopRoomActor> Room;

	UPROPERTY()
	TObjectPtr<ACRShopMerchant> Merchant;

	FName ShopNodeId;
	bool bValidShop = false;
	bool bLeaving = false;
	FString Message;
	double MessageTime = -100.0;
	FTimerHandle LeaveTimer;
};
