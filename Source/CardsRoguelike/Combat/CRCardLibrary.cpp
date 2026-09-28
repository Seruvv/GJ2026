#include "CRCardLibrary.h"

namespace
{
	FCRCardDef MakeCard(const TCHAR* Id, const TCHAR* Name, const TCHAR* ShortText, int32 Cost, ECRCardEffect Effect,
		ECRCardTargeting Targeting, float Strength, int32 Damage, float Radius, int32 Amount, bool bPhysical)
	{
		FCRCardDef Card;
		Card.Id = Id;
		Card.Name = Name;
		Card.ShortText = ShortText;
		Card.ManaCost = Cost;
		Card.Effect = Effect;
		Card.Targeting = Targeting;
		Card.Strength = Strength;
		Card.Damage = Damage;
		Card.Radius = Radius;
		Card.Amount = Amount;
		Card.bIsPhysical = bPhysical;
		return Card;
	}
}

namespace CRCardLibrary
{
	const TArray<FCRCardDef>& GetPrototypeCards()
	{
		// Values moved unchanged from the M2.3 CRCombatGameMode constructor.
		static const TArray<FCRCardDef> Cards = {
			MakeCard(TEXT("Push"),  TEXT("PUSH"),  TEXT("Push target"),    1, ECRCardEffect::Push,  ECRCardTargeting::PhysicsTargetThenPoint, 1500.f, 0, 0.f,   0, true),
			MakeCard(TEXT("Blast"), TEXT("BLAST"), TEXT("Area blast"),     2, ECRCardEffect::Blast, ECRCardTargeting::GroundPoint,            1200.f, 3, 400.f, 0, true),
			MakeCard(TEXT("Pull"),  TEXT("PULL"),  TEXT("Pull to center"), 1, ECRCardEffect::Pull,  ECRCardTargeting::PhysicsTarget,          1300.f, 0, 0.f,   0, true),
			MakeCard(TEXT("Guard"), TEXT("GUARD"), TEXT("Gain armor"),     1, ECRCardEffect::Guard, ECRCardTargeting::None,                   0.f,    0, 0.f,   5, false),
			MakeCard(TEXT("Mend"),  TEXT("MEND"),  TEXT("Restore HP"),     2, ECRCardEffect::Mend,  ECRCardTargeting::None,                   0.f,    0, 0.f,   6, false),
		};
		return Cards;
	}

	const FCRCardDef* FindCard(FName CardId)
	{
		return GetPrototypeCards().FindByPredicate([CardId](const FCRCardDef& Card)
		{
			return Card.Id == CardId || Card.Name.Equals(CardId.ToString(), ESearchCase::IgnoreCase);
		});
	}

	TArray<FName> GetAllCardIds()
	{
		TArray<FName> Ids;
		for (const FCRCardDef& Card : GetPrototypeCards())
		{
			Ids.AddUnique(Card.Id);
		}
		return Ids;
	}
}
