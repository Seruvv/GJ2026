#include "CRRunTypes.h"

namespace CRRun
{
	FString RoomTypeName(ECRRoomType Type)
	{
		switch (Type)
		{
		case ECRRoomType::Start:  return TEXT("START");
		case ECRRoomType::Combat: return TEXT("COMBAT");
		case ECRRoomType::Shop:   return TEXT("SHOP");
		case ECRRoomType::Event:  return TEXT("EVENT");
		case ECRRoomType::Boss:   return TEXT("BOSS");
		case ECRRoomType::Return: return TEXT("RETURN");
		}
		return TEXT("?");
	}

	FString RoomTypeDisplayName(ECRRoomType Type)
	{
		switch (Type)
		{
		case ECRRoomType::Start:  return TEXT("НАЧАЛО");
		case ECRRoomType::Combat: return TEXT("БОЙ");
		case ECRRoomType::Shop:   return TEXT("ЛАВКА");
		case ECRRoomType::Event:  return TEXT("СОБЫТИЕ");
		case ECRRoomType::Boss:   return TEXT("БОСС");
		case ECRRoomType::Return: return TEXT("ВОЗВРАЩЕНИЕ");
		}
		return TEXT("?");
	}

	FCRRunStartConfig MakeDeveloperStartConfig()
	{
		FCRRunStartConfig Config;
		Config.HamsterName = TEXT("Тестовый хомяк");
		Config.BaseMaxHP = BaseHamsterMaxHP;
		Config.BaseManaPerTurn = BaseHamsterManaPerTurn;
		return Config;
	}

	void ApplyStartConfig(FCRRunState& RunState, const FCRRunStartConfig& Config)
	{
		RunState.ProfileId = Config.ProfileId;
		RunState.Hamster.HamsterId = Config.HamsterId;
		RunState.Hamster.Name = Config.HamsterName;
		RunState.Hamster.MaxHP = FMath::Max(1, Config.BaseMaxHP + FMath::Max(0, Config.Bonuses.BonusMaxHP));
		RunState.Hamster.CurrentHP = RunState.Hamster.MaxHP;
		RunState.Hamster.ManaPerTurn = FMath::Max(1, Config.BaseManaPerTurn);
		RunState.DeckCardIds = Config.StartingDeckCardIds.Num() > 0 ? Config.StartingDeckCardIds : StarterDeck();
		RunState.DeckCardIds.Append(Config.Bonuses.ExtraCardIds);
		RunState.Carried.Silver = FMath::Max(0, Config.Bonuses.StartSilver);
		RunState.Carried.Food = FMath::Max(0, Config.Bonuses.StartFood);
	}

	const TArray<FString>& MerchantLines()
	{
		static const TArray<FString> Lines = {
			TEXT("Что-нибудь нужно?"),
			TEXT("Покупаешь или просто смотришь?"),
			TEXT("Монеты вперёд."),
			TEXT("Трогать можно. Бесплатно — нельзя."),
			TEXT("Есть товар. Есть цена."),
		};
		return Lines;
	}

	FLinearColor RoomTypeColor(ECRRoomType Type)
	{
		switch (Type)
		{
		case ECRRoomType::Start:  return FLinearColor(0.8f, 0.8f, 0.75f);
		case ECRRoomType::Combat: return FLinearColor(0.85f, 0.15f, 0.1f);
		case ECRRoomType::Shop:   return FLinearColor(0.95f, 0.72f, 0.1f);
		case ECRRoomType::Event:  return FLinearColor(0.55f, 0.25f, 0.9f);
		case ECRRoomType::Boss:   return FLinearColor(0.45f, 0.02f, 0.08f);
		case ECRRoomType::Return: return FLinearColor(0.15f, 0.8f, 0.35f);
		}
		return FLinearColor::White;
	}
}
