// Shared prototype card definitions: the single catalog used by Combat, Rewards and the Shop.

#pragma once

#include "CoreMinimal.h"
#include "CRTypes.h"

namespace CRCardLibrary
{
	/** Every prototype card definition (Push, Blast, Pull, Guard, Mend). */
	const TArray<FCRCardDef>& GetPrototypeCards();

	/** Definition for a card id ("Push"), or nullptr. Also accepts the display name ("PUSH"). */
	const FCRCardDef* FindCard(FName CardId);

	/** Ids of every prototype card, in catalog order. */
	TArray<FName> GetAllCardIds();
}
