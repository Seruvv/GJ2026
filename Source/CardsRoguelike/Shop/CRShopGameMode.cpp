#include "CRShopGameMode.h"

#include "../Combat/CRCardLibrary.h"
#include "../Run/CRRunSubsystem.h"
#include "CRShopHUD.h"
#include "CRShopMerchant.h"
#include "CRShopPlayerController.h"
#include "CRShopRoomActor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCRShop, Log, All);

ACRShopGameMode::ACRShopGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ACRShopPlayerController::StaticClass();
	HUDClass = ACRShopHUD::StaticClass();
}

UCRRunSubsystem* ACRShopGameMode::GetRun() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCRRunSubsystem>() : nullptr;
}

void ACRShopGameMode::StartPlay()
{
	Super::StartPlay();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Room = GetWorld()->SpawnActor<ACRShopRoomActor>(ACRShopRoomActor::StaticClass(), FTransform::Identity, Params);
	// The merchant faces the camera (which looks along +Y).
	Merchant = GetWorld()->SpawnActor<ACRShopMerchant>(ACRShopMerchant::StaticClass(),
		FTransform(FRotator(0.f, -90.f, 0.f), Room->GetMerchantLocation()), Params);

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTarget(Room);
	}

	// Only a real shop room of an active run is interactive. A direct sandbox open never touches run data.
	UCRRunSubsystem* Run = GetRun();
	bValidShop = Run && Run->IsInShopRoom();
	if (!bValidShop)
	{
		UE_LOG(LogCRShop, Log, TEXT("Shop sandbox opened without an active shop room"));
		Merchant->SetDialogueLine(TEXT("Nobody's shopping today."));
		return;
	}

	ShopNodeId = Run->GetRunState().CurrentNodeId;
	const FCRShopState* Shop = Run->EnsureShopState(ShopNodeId, OfferCount);
	if (!Shop)
	{
		bValidShop = false;
		return;
	}
	Merchant->SetDialogueLine(Shop->MerchantLine);
	UE_LOG(LogCRShop, Log, TEXT("Shop room %s opened (Silver %d, HP %d/%d)"), *ShopNodeId.ToString(), GetSilver(), GetHP(), GetMaxHP());
}

void ACRShopGameMode::RestartPlayer(AController* NewPlayer)
{
	// No pawn in the shop: the view is the room's fixed camera.
}

const FCRShopState* ACRShopGameMode::GetShopState() const
{
	const UCRRunSubsystem* Run = GetRun();
	return (Run && bValidShop) ? Run->FindShopState(ShopNodeId) : nullptr;
}

int32 ACRShopGameMode::GetSilver() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Carried.Silver : 0;
}

int32 ACRShopGameMode::GetHP() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Hamster.CurrentHP : 0;
}

int32 ACRShopGameMode::GetMaxHP() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Hamster.MaxHP : 0;
}

ECRShopOfferStatus ACRShopGameMode::GetCardStatus(int32 OfferIndex) const
{
	const FCRShopState* Shop = GetShopState();
	if (!Shop || bLeaving || !Shop->OfferPurchased.IsValidIndex(OfferIndex))
	{
		return ECRShopOfferStatus::Unavailable;
	}
	if (Shop->OfferPurchased[OfferIndex])
	{
		return ECRShopOfferStatus::Sold;
	}
	return GetSilver() >= CardPrice ? ECRShopOfferStatus::Available : ECRShopOfferStatus::NotEnoughSilver;
}

ECRShopOfferStatus ACRShopGameMode::GetHealStatus() const
{
	const FCRShopState* Shop = GetShopState();
	if (!Shop || bLeaving)
	{
		return ECRShopOfferStatus::Unavailable;
	}
	if (Shop->bHealPurchased)
	{
		return ECRShopOfferStatus::Sold;
	}
	if (GetHP() >= GetMaxHP())
	{
		return ECRShopOfferStatus::FullHP;
	}
	return GetSilver() >= HealPrice ? ECRShopOfferStatus::Available : ECRShopOfferStatus::NotEnoughSilver;
}

void ACRShopGameMode::BuyCard(int32 OfferIndex)
{
	const ECRShopOfferStatus Status = GetCardStatus(OfferIndex);
	if (Status == ECRShopOfferStatus::NotEnoughSilver)
	{
		ShowMessage(TEXT("Not enough Silver"));
		return;
	}
	if (Status != ECRShopOfferStatus::Available)
	{
		return;
	}

	UCRRunSubsystem* Run = GetRun();
	if (Run && Run->BuyShopCard(ShopNodeId, OfferIndex, CardPrice))
	{
		const FCRShopState* Shop = GetShopState();
		const FCRCardDef* Card = Shop ? CRCardLibrary::FindCard(Shop->OfferCardIds[OfferIndex]) : nullptr;
		ShowMessage(FString::Printf(TEXT("Bought %s"), Card ? *Card->Name : TEXT("card")));
	}
}

void ACRShopGameMode::BuyHeal()
{
	const ECRShopOfferStatus Status = GetHealStatus();
	if (Status == ECRShopOfferStatus::NotEnoughSilver)
	{
		ShowMessage(TEXT("Not enough Silver"));
		return;
	}
	if (Status == ECRShopOfferStatus::FullHP)
	{
		ShowMessage(TEXT("Already at full HP"));
		return;
	}
	if (Status != ECRShopOfferStatus::Available)
	{
		return;
	}

	UCRRunSubsystem* Run = GetRun();
	const int32 Before = GetHP();
	if (Run && Run->BuyShopHeal(ShopNodeId, HealPrice, HealAmount))
	{
		ShowMessage(FString::Printf(TEXT("Healed %d -> %d"), Before, GetHP()));
	}
}

void ACRShopGameMode::LeaveShop()
{
	// One commit only; purchases are closed from here on.
	if (!bValidShop || bLeaving)
	{
		return;
	}
	bLeaving = true;

	if (UCRRunSubsystem* Run = GetRun())
	{
		Run->CompleteCurrentRoom();
	}
	UE_LOG(LogCRShop, Log, TEXT("Left shop %s"), *ShopNodeId.ToString());
	ShowMessage(TEXT("Leaving the shop..."));
	GetWorldTimerManager().SetTimer(LeaveTimer, this, &ACRShopGameMode::ReturnToRunMap, FMath::Max(LeaveDelay, 0.01f), false);
}

void ACRShopGameMode::ReturnToRunMap()
{
	UGameplayStatics::OpenLevel(this, FName(CRRun::RunMapPath()));
}

void ACRShopGameMode::ShowMessage(const FString& InMessage)
{
	Message = InMessage;
	MessageTime = GetWorld()->GetRealTimeSeconds();
	UE_LOG(LogCRShop, Log, TEXT("%s"), *InMessage);
}

FString ACRShopGameMode::GetActiveMessage() const
{
	return GetWorld()->GetRealTimeSeconds() - MessageTime < 2.5 ? Message : FString();
}
