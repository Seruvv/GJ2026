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

	const TArray<FString>& MerchantLines()
	{
		static const TArray<FString> Lines = {
			TEXT("Take a look."),
			TEXT("Spend wisely."),
			TEXT("Everything has a price."),
			TEXT("Need something?"),
			TEXT("I've seen worse decks."),
			TEXT("Don't take all day."),
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
