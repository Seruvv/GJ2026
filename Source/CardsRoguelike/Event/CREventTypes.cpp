#include "CREventTypes.h"

namespace CREvent
{
	FString ResourceName(ECREventResource Resource)
	{
		switch (Resource)
		{
		case ECREventResource::Silver: return TEXT("Silver");
		case ECREventResource::Food:   return TEXT("Food");
		case ECREventResource::Wood:   return TEXT("Wood");
		}
		return TEXT("?");
	}

	FString ResourceDisplayName(ECREventResource Resource)
	{
		switch (Resource)
		{
		case ECREventResource::Silver: return TEXT("Серебро");
		case ECREventResource::Food:   return TEXT("Еда");
		case ECREventResource::Wood:   return TEXT("Дерево");
		}
		return TEXT("?");
	}

	FString NeedResourceText(ECREventResource Resource, int32 Amount)
	{
		switch (Resource)
		{
		case ECREventResource::Silver: return FString::Printf(TEXT("Нужно серебра: %d"), Amount);
		case ECREventResource::Food:   return FString::Printf(TEXT("Нужна еда: %d"), Amount);
		case ECREventResource::Wood:   return FString::Printf(TEXT("Нужно дерева: %d"), Amount);
		}
		return FString::Printf(TEXT("Не хватает ресурса: %d"), Amount);
	}

	FString SignedAmount(int32 Amount)
	{
		return Amount >= 0 ? FString::Printf(TEXT("+%d"), Amount) : FString::Printf(TEXT("%d"), Amount);
	}
}
