#include "CRRunSubsystem.h"

#include "../Combat/CRCardLibrary.h"
#include "../Event/CREventDefinition.h"
#include "../Event/CREventPool.h"

DEFINE_LOG_CATEGORY_STATIC(LogCRRun, Log, All);

void UCRRunSubsystem::StartPrototypeRun()
{
	RunState = FCRRunState();
	RunState.Status = ECRRunStatus::Active;

	RunState.Hamster.Name = TEXT("Test Hamster");
	RunState.Hamster.MaxHP = 30;
	RunState.Hamster.CurrentHP = 30;
	RunState.Hamster.ManaPerTurn = 3;

	RunState.DeckCardIds = { TEXT("Push"), TEXT("Blast"), TEXT("Pull"), TEXT("Guard"), TEXT("Mend") };
	BuildPrototypeGraph();
	// Start needs no resolving: its first rooms are open immediately.
	RunState.CurrentNodeId = TEXT("Start");
	RunState.VisitedNodeIds = { RunState.CurrentNodeId };
	RunState.bCurrentRoomResolved = true;
	RefreshNodeStates();

	UE_LOG(LogCRRun, Log, TEXT("Prototype run started (%d nodes)"), RunState.Nodes.Num());
	OnRunStateChanged.Broadcast();
}

void UCRRunSubsystem::AbandonRun()
{
	RunState = FCRRunState();
	UE_LOG(LogCRRun, Log, TEXT("Run abandoned"));
	OnRunStateChanged.Broadcast();
}

void UCRRunSubsystem::BuildPrototypeGraph()
{
	// Fixed test layout. X is route progress, Y the branch, Z small height variation.
	//
	//              CombatA ------> CombatC
	//             /               ^       \
	//   Start ---+               /         Boss ---> Return
	//             \             /         /
	//              EventA -----+         /
	//                    \              /
	//                     ShopA --> CombatB
	auto AddNode = [this](const TCHAR* Id, ECRRoomType Type, const FVector& Position, std::initializer_list<const TCHAR*> Next)
	{
		FCRRunNodeData Node;
		Node.NodeId = Id;
		Node.RoomType = Type;
		Node.Position = Position;
		for (const TCHAR* NextId : Next)
		{
			Node.ConnectedNodeIds.Add(NextId);
		}
		RunState.Nodes.Add(Node);
	};

	AddNode(TEXT("Start"),   ECRRoomType::Start,  FVector(0.f,    0.f,    0.f),   { TEXT("CombatA"), TEXT("EventA") });
	// CombatA -> ShopA (M2.4) opens the economy test route: Combat -> Reward -> Shop -> Combat.
	AddNode(TEXT("CombatA"), ECRRoomType::Combat, FVector(750.f,  -550.f, 80.f),  { TEXT("CombatC"), TEXT("ShopA") });
	AddNode(TEXT("EventA"),  ECRRoomType::Event,  FVector(750.f,  450.f,  0.f),   { TEXT("CombatC"), TEXT("ShopA") });
	// Event nodes name the pool they draw from; a generated map can assign regional or rare pools here.
	RunState.Nodes.Last().EventPool = TSoftObjectPtr<UCREventPool>(FSoftObjectPath(CRRun::DefaultEventPoolPath()));
	AddNode(TEXT("CombatC"), ECRRoomType::Combat, FVector(1600.f, -350.f, 140.f), { TEXT("Boss") });
	AddNode(TEXT("ShopA"),   ECRRoomType::Shop,   FVector(1350.f, 950.f,  20.f),  { TEXT("CombatB") });
	AddNode(TEXT("CombatB"), ECRRoomType::Combat, FVector(2050.f, 700.f,  60.f),  { TEXT("Boss") });
	AddNode(TEXT("Boss"),    ECRRoomType::Boss,   FVector(2650.f, 100.f,  180.f), { TEXT("Return") });
	AddNode(TEXT("Return"),  ECRRoomType::Return, FVector(3300.f, 100.f,  120.f), {});
}

void UCRRunSubsystem::RefreshNodeStates()
{
	const FCRRunNodeData* Current = FindNode(RunState.CurrentNodeId);
	const bool bCanMoveOn = RunState.Status == ECRRunStatus::Active && RunState.bCurrentRoomResolved;
	for (FCRRunNodeData& Node : RunState.Nodes)
	{
		if (Node.NodeId == RunState.CurrentNodeId)
		{
			// A resolved room reads as Completed; Start keeps its M2.1 "Current" look.
			const bool bShowCompleted = RunState.bCurrentRoomResolved && Node.RoomType != ECRRoomType::Start;
			Node.State = bShowCompleted ? ECRRunNodeState::Completed : ECRRunNodeState::Current;
		}
		else if (RunState.VisitedNodeIds.Contains(Node.NodeId))
		{
			Node.State = ECRRunNodeState::Completed;
		}
		else if (bCanMoveOn && Current && Current->ConnectedNodeIds.Contains(Node.NodeId))
		{
			Node.State = ECRRunNodeState::Available;
		}
		else
		{
			Node.State = ECRRunNodeState::Locked;
		}
	}
}

const FCRRunNodeData* UCRRunSubsystem::FindNode(FName NodeId) const
{
	return RunState.Nodes.FindByPredicate([NodeId](const FCRRunNodeData& Node) { return Node.NodeId == NodeId; });
}

bool UCRRunSubsystem::IsInCombatRoom() const
{
	const FCRRunNodeData* Current = GetCurrentNode();
	return IsRunActive() && !RunState.bCurrentRoomResolved && Current && Current->RoomType == ECRRoomType::Combat;
}

bool UCRRunSubsystem::CanTravelTo(FName NodeId) const
{
	const FCRRunNodeData* Node = FindNode(NodeId);
	return IsRunActive() && Node && Node->State == ECRRunNodeState::Available;
}

bool UCRRunSubsystem::EnterNode(FName NodeId)
{
	if (!CanTravelTo(NodeId))
	{
		return false;
	}

	RunState.CurrentNodeId = NodeId;
	RunState.VisitedNodeIds.Add(NodeId);
	RunState.bCurrentRoomResolved = false;
	RefreshNodeStates();

	const FCRRunNodeData* Node = FindNode(NodeId);
	UE_LOG(LogCRRun, Log, TEXT("Entered %s (%s)"), *NodeId.ToString(), Node ? *CRRun::RoomTypeName(Node->RoomType) : TEXT("?"));
	OnRunStateChanged.Broadcast();

	// Combat, Shop and Event rooms resolve through their own maps. Rooms without gameplay yet
	// (Boss, Return) are placeholders that resolve on arrival so the graph stays traversable.
	if (Node && Node->RoomType != ECRRoomType::Combat && Node->RoomType != ECRRoomType::Shop && Node->RoomType != ECRRoomType::Event)
	{
		CompleteCurrentRoom();
	}
	return true;
}

bool UCRRunSubsystem::IsInShopRoom() const
{
	const FCRRunNodeData* Current = GetCurrentNode();
	return IsRunActive() && !RunState.bCurrentRoomResolved && Current && Current->RoomType == ECRRoomType::Shop;
}

const FCRShopState* UCRRunSubsystem::FindShopState(FName ShopNodeId) const
{
	return RunState.ShopStates.Find(ShopNodeId);
}

const FCRShopState* UCRRunSubsystem::EnsureShopState(FName ShopNodeId, int32 OfferCount)
{
	const FCRRunNodeData* Node = FindNode(ShopNodeId);
	if (!IsRunActive() || !Node || Node->RoomType != ECRRoomType::Shop)
	{
		return nullptr;
	}

	FCRShopState& Shop = RunState.ShopStates.FindOrAdd(ShopNodeId);
	if (!Shop.bInitialized)
	{
		// Distinct offers from the shared prototype catalog; duplicates of deck cards are allowed.
		TArray<FName> Pool = CRCardLibrary::GetAllCardIds();
		while (Shop.OfferCardIds.Num() < OfferCount && Pool.Num() > 0)
		{
			const int32 Pick = FMath::RandRange(0, Pool.Num() - 1);
			Shop.OfferCardIds.Add(Pool[Pick]);
			Pool.RemoveAt(Pick);
		}
		Shop.OfferPurchased.Init(false, Shop.OfferCardIds.Num());

		const TArray<FString>& Lines = CRRun::MerchantLines();
		Shop.MerchantLine = Lines.Num() > 0 ? Lines[FMath::RandRange(0, Lines.Num() - 1)] : FString();
		Shop.bInitialized = true;

		FString Offers;
		for (const FName Offer : Shop.OfferCardIds)
		{
			Offers += (Offers.IsEmpty() ? TEXT("") : TEXT(", ")) + Offer.ToString();
		}
		UE_LOG(LogCRRun, Log, TEXT("Shop %s created: offers [%s], merchant \"%s\""), *ShopNodeId.ToString(), *Offers, *Shop.MerchantLine);
		OnRunStateChanged.Broadcast();
	}
	return &Shop;
}

FCRShopState* UCRRunSubsystem::GetActiveShopState(FName ShopNodeId)
{
	// Purchases only happen inside the shop the hamster is currently standing in.
	if (!IsInShopRoom() || RunState.CurrentNodeId != ShopNodeId)
	{
		return nullptr;
	}
	FCRShopState* Shop = RunState.ShopStates.Find(ShopNodeId);
	return Shop && Shop->bInitialized ? Shop : nullptr;
}

bool UCRRunSubsystem::BuyShopCard(FName ShopNodeId, int32 OfferIndex, int32 SilverPrice)
{
	FCRShopState* Shop = GetActiveShopState(ShopNodeId);
	if (!Shop || !Shop->OfferCardIds.IsValidIndex(OfferIndex) || !Shop->OfferPurchased.IsValidIndex(OfferIndex)
		|| Shop->OfferPurchased[OfferIndex] || SilverPrice < 0 || RunState.Carried.Silver < SilverPrice)
	{
		return false;
	}

	const FName CardId = Shop->OfferCardIds[OfferIndex];
	Shop->OfferPurchased[OfferIndex] = true;
	RunState.Carried.Silver -= SilverPrice;
	RunState.DeckCardIds.Add(CardId);
	UE_LOG(LogCRRun, Log, TEXT("Shop %s: bought %s for %d Silver (Silver %d, deck %d)"), *ShopNodeId.ToString(),
		*CardId.ToString(), SilverPrice, RunState.Carried.Silver, RunState.DeckCardIds.Num());
	OnRunStateChanged.Broadcast();
	return true;
}

bool UCRRunSubsystem::BuyShopHeal(FName ShopNodeId, int32 SilverPrice, int32 HealAmount)
{
	FCRShopState* Shop = GetActiveShopState(ShopNodeId);
	FCRHamsterRunData& Hamster = RunState.Hamster;
	if (!Shop || Shop->bHealPurchased || SilverPrice < 0 || RunState.Carried.Silver < SilverPrice
		|| HealAmount <= 0 || Hamster.CurrentHP >= Hamster.MaxHP)
	{
		return false;
	}

	const int32 Before = Hamster.CurrentHP;
	Shop->bHealPurchased = true;
	RunState.Carried.Silver -= SilverPrice;
	Hamster.CurrentHP = FMath::Min(Hamster.MaxHP, Hamster.CurrentHP + HealAmount);
	UE_LOG(LogCRRun, Log, TEXT("Shop %s: healed %d -> %d for %d Silver (Silver %d)"), *ShopNodeId.ToString(),
		Before, Hamster.CurrentHP, SilverPrice, RunState.Carried.Silver);
	OnRunStateChanged.Broadcast();
	return true;
}

namespace
{
	FString EventCardName(FName CardId)
	{
		const FCRCardDef* Card = CRCardLibrary::FindCard(CardId);
		return Card ? Card->Name : CardId.ToString();
	}

	int32& CarriedResource(FCRCarriedLoot& Carried, ECREventResource Resource)
	{
		switch (Resource)
		{
		case ECREventResource::Food: return Carried.Food;
		case ECREventResource::Wood: return Carried.Wood;
		default:                     return Carried.Silver;
		}
	}
}

bool UCRRunSubsystem::IsInEventRoom() const
{
	const FCRRunNodeData* Current = GetCurrentNode();
	return IsRunActive() && !RunState.bCurrentRoomResolved && Current && Current->RoomType == ECRRoomType::Event;
}

const FCREventNodeState* UCRRunSubsystem::FindEventState(FName EventNodeId) const
{
	return RunState.EventStates.Find(EventNodeId);
}

const FCREventNodeState* UCRRunSubsystem::EnsureEventState(FName EventNodeId)
{
	const FCRRunNodeData* Node = FindNode(EventNodeId);
	if (!IsRunActive() || !Node || Node->RoomType != ECRRoomType::Event)
	{
		return nullptr;
	}

	if (const FCREventNodeState* Existing = RunState.EventStates.Find(EventNodeId); Existing && Existing->bInitialized)
	{
		return Existing;
	}

	// First visit: draw one event from the node's pool. The pick is stored and never rerolled.
	const TSoftObjectPtr<UCREventPool> PoolRef = Node->EventPool.IsNull()
		? TSoftObjectPtr<UCREventPool>(FSoftObjectPath(CRRun::DefaultEventPoolPath())) : Node->EventPool;
	const UCREventPool* Pool = PoolRef.LoadSynchronous();
	const TSoftObjectPtr<UCREventDefinition> Picked = Pool ? Pool->PickEvent() : nullptr;
	if (Picked.IsNull())
	{
		UE_LOG(LogCRRun, Warning, TEXT("Event %s: no event could be drawn from pool %s"), *EventNodeId.ToString(), *PoolRef.ToString());
		return nullptr;
	}

	FCREventNodeState& State = RunState.EventStates.FindOrAdd(EventNodeId);
	State = FCREventNodeState();
	State.SelectedEvent = Picked;
	State.bInitialized = true;
	UE_LOG(LogCRRun, Log, TEXT("Event %s: drew %s from pool %s"), *EventNodeId.ToString(), *Picked.ToString(), *PoolRef.ToString());
	OnRunStateChanged.Broadcast();
	return &State;
}

FCREventNodeState* UCRRunSubsystem::GetActiveEventState(FName EventNodeId)
{
	// Choices only happen inside the event room the hamster is currently standing in.
	if (!IsInEventRoom() || RunState.CurrentNodeId != EventNodeId)
	{
		return nullptr;
	}
	FCREventNodeState* State = RunState.EventStates.Find(EventNodeId);
	return State && State->bInitialized ? State : nullptr;
}

bool UCRRunSubsystem::WouldEventChoiceBeLethal(int32 CurrentHP, int32 HPDelta)
{
	return HPDelta < 0 && CurrentHP + HPDelta <= 0;
}

bool UCRRunSubsystem::ChoiceNeedsCardSelection(const FCREventChoice& Choice)
{
	return Choice.Effects.ContainsByPredicate([](const FCREventEffect& Effect) { return Effect.Type == ECREventEffectType::RemoveSelectedCard; });
}

FString UCRRunSubsystem::GetEventChoiceBlockReason(const FCREventChoice& Choice) const
{
	// Validate the whole choice up front so it either commits completely or not at all.
	int32 HPDelta = 0;
	int32 ResourceDelta[3] = { 0, 0, 0 };
	int32 Sacrifices = 0;
	for (const FCREventEffect& Effect : Choice.Effects)
	{
		switch (Effect.Type)
		{
		case ECREventEffectType::ModifyHP:
			HPDelta += Effect.Amount;
			break;
		case ECREventEffectType::ModifyResource:
			ResourceDelta[static_cast<int32>(Effect.Resource)] += Effect.Amount;
			break;
		case ECREventEffectType::AddRandomCard:
			if (CRCardLibrary::GetAllCardIds().Num() == 0)
			{
				return TEXT("No cards available");
			}
			break;
		case ECREventEffectType::AddSpecificCard:
			if (!CRCardLibrary::FindCard(Effect.CardId))
			{
				return FString::Printf(TEXT("Unknown card '%s'"), *Effect.CardId.ToString());
			}
			break;
		case ECREventEffectType::RemoveSelectedCard:
			++Sacrifices;
			break;
		}
	}

	FCRCarriedLoot Carried = RunState.Carried;
	for (const ECREventResource Resource : { ECREventResource::Silver, ECREventResource::Food, ECREventResource::Wood })
	{
		const int32 Delta = ResourceDelta[static_cast<int32>(Resource)];
		if (Delta < 0 && CarriedResource(Carried, Resource) + Delta < 0)
		{
			return FString::Printf(TEXT("Need %d %s"), -Delta, *CREvent::ResourceName(Resource));
		}
	}
	if (Sacrifices > 1)
	{
		return TEXT("Only one card can be sacrificed");
	}
	if (Sacrifices == 1 && RunState.DeckCardIds.Num() == 0)
	{
		return TEXT("No card to sacrifice");
	}
	if (WouldEventChoiceBeLethal(RunState.Hamster.CurrentHP, HPDelta))
	{
		return TEXT("Would be lethal");
	}
	return FString();
}

bool UCRRunSubsystem::CommitEventChoice(FName EventNodeId, const UCREventDefinition* Event, int32 ChoiceIndex, int32 SacrificeDeckIndex)
{
	FCREventNodeState* State = GetActiveEventState(EventNodeId);
	if (!State || State->bEffectsCommitted || !Event || FSoftObjectPath(Event) != State->SelectedEvent.ToSoftObjectPath()
		|| !Event->Choices.IsValidIndex(ChoiceIndex))
	{
		return false;
	}

	const FCREventChoice& Choice = Event->Choices[ChoiceIndex];
	const bool bNeedsCard = ChoiceNeedsCardSelection(Choice);
	if (!GetEventChoiceBlockReason(Choice).IsEmpty() || (bNeedsCard && !RunState.DeckCardIds.IsValidIndex(SacrificeDeckIndex)))
	{
		return false;
	}

	TArray<FString> Lines;
	FCRHamsterRunData& Hamster = RunState.Hamster;
	FCRCarriedLoot& Carried = RunState.Carried;

	// The sacrifice is the one effect that depends on player input; take exactly that deck entry.
	if (bNeedsCard)
	{
		State->SacrificedCardId = RunState.DeckCardIds[SacrificeDeckIndex];
		RunState.DeckCardIds.RemoveAt(SacrificeDeckIndex);
		Lines.Add(FString::Printf(TEXT("Card sacrificed: %s"), *EventCardName(State->SacrificedCardId)));
	}

	for (const FCREventEffect& Effect : Choice.Effects)
	{
		switch (Effect.Type)
		{
		case ECREventEffectType::ModifyHP:
		{
			// Healing caps at MaxHP. Damage is not clamped to 1: lethal outcomes are blocked by the
			// temporary guard in validation instead (see WouldEventChoiceBeLethal).
			const int32 Before = Hamster.CurrentHP;
			Hamster.CurrentHP = FMath::Max(0, FMath::Min(Hamster.MaxHP, Hamster.CurrentHP + Effect.Amount));
			Lines.Add(FString::Printf(TEXT("HP %s"), *CREvent::SignedAmount(Hamster.CurrentHP - Before)));
			break;
		}
		case ECREventEffectType::ModifyResource:
			CarriedResource(Carried, Effect.Resource) += Effect.Amount;
			Lines.Add(FString::Printf(TEXT("%s %s"), *CREvent::ResourceName(Effect.Resource), *CREvent::SignedAmount(Effect.Amount)));
			break;
		case ECREventEffectType::AddRandomCard:
		{
			const TArray<FName> CardIds = CRCardLibrary::GetAllCardIds();
			const FName CardId = CardIds[FMath::RandRange(0, CardIds.Num() - 1)];
			RunState.DeckCardIds.Add(CardId);
			State->GainedCardIds.Add(CardId);
			Lines.Add(FString::Printf(TEXT("Card gained: %s"), *EventCardName(CardId)));
			break;
		}
		case ECREventEffectType::AddSpecificCard:
			RunState.DeckCardIds.Add(Effect.CardId);
			State->GainedCardIds.Add(Effect.CardId);
			Lines.Add(FString::Printf(TEXT("Card gained: %s"), *EventCardName(Effect.CardId)));
			break;
		case ECREventEffectType::RemoveSelectedCard:
			break;
		}
	}

	State->SelectedChoiceIndex = ChoiceIndex;
	State->ResultLines = Lines;
	State->bEffectsCommitted = true;
	UE_LOG(LogCRRun, Log, TEXT("Event %s (%s): choice %d committed [%s] -> HP %d/%d, Silver %d, Food %d, Wood %d, deck %d"),
		*EventNodeId.ToString(), *Event->EventId.ToString(), ChoiceIndex + 1, *FString::Join(Lines, TEXT("; ")),
		Hamster.CurrentHP, Hamster.MaxHP, Carried.Silver, Carried.Food, Carried.Wood, RunState.DeckCardIds.Num());
	OnRunStateChanged.Broadcast();
	return true;
}

bool UCRRunSubsystem::ContinueFromEvent(FName EventNodeId)
{
	const FCREventNodeState* State = GetActiveEventState(EventNodeId);
	if (!State || !State->bEffectsCommitted)
	{
		return false;
	}
	return CompleteCurrentRoom();
}

bool UCRRunSubsystem::CompleteCurrentRoom()
{
	if (!IsRunActive() || RunState.bCurrentRoomResolved)
	{
		return false;
	}

	RunState.bCurrentRoomResolved = true;
	RefreshNodeStates();
	UE_LOG(LogCRRun, Log, TEXT("Room %s resolved (HP %d/%d)"), *RunState.CurrentNodeId.ToString(), RunState.Hamster.CurrentHP, RunState.Hamster.MaxHP);
	OnRunStateChanged.Broadcast();
	return true;
}

void UCRRunSubsystem::FailCurrentRun()
{
	if (!IsRunActive())
	{
		return;
	}

	RunState.Status = ECRRunStatus::Failed;
	RefreshNodeStates();
	UE_LOG(LogCRRun, Log, TEXT("Run failed in room %s"), *RunState.CurrentNodeId.ToString());
	OnRunStateChanged.Broadcast();
}

void UCRRunSubsystem::SetHamsterHP(int32 CurrentHP)
{
	RunState.Hamster.CurrentHP = FMath::Clamp(CurrentHP, 0, RunState.Hamster.MaxHP);
}

void UCRRunSubsystem::AddCardToDeck(FName CardId)
{
	if (!IsRunActive() || CardId.IsNone())
	{
		return;
	}
	RunState.DeckCardIds.Add(CardId);
	UE_LOG(LogCRRun, Log, TEXT("Card %s added to deck (%d cards)"), *CardId.ToString(), RunState.DeckCardIds.Num());
	OnRunStateChanged.Broadcast();
}

void UCRRunSubsystem::AddCarriedResources(int32 Silver, int32 Food, int32 Wood)
{
	if (!IsRunActive())
	{
		return;
	}
	FCRCarriedLoot& Carried = RunState.Carried;
	Carried.Silver = FMath::Max(0, Carried.Silver + Silver);
	Carried.Food = FMath::Max(0, Carried.Food + Food);
	Carried.Wood = FMath::Max(0, Carried.Wood + Wood);
	UE_LOG(LogCRRun, Log, TEXT("Resources now Silver %d, Food %d, Wood %d"), Carried.Silver, Carried.Food, Carried.Wood);
	OnRunStateChanged.Broadcast();
}
