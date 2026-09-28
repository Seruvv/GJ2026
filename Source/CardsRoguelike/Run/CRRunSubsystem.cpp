#include "CRRunSubsystem.h"

#include "../Combat/CRCardLibrary.h"

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

	// Combat and Shop rooms resolve through their own maps. Rooms without gameplay yet
	// (Event, Boss, Return) are placeholders that resolve on arrival so the graph stays traversable.
	if (Node && Node->RoomType != ECRRoomType::Combat && Node->RoomType != ECRRoomType::Shop)
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
