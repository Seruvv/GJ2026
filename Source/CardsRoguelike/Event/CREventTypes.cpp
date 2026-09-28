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

	FString SignedAmount(int32 Amount)
	{
		return Amount >= 0 ? FString::Printf(TEXT("+%d"), Amount) : FString::Printf(TEXT("%d"), Amount);
	}
}
