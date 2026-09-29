#include "CRRunGenerator.h"

#include "../Event/CREventPool.h"
#include "Math/RandomStream.h"

namespace
{
	/** Visual height per layer (pillars on the diorama); fixed, not random, so maps stay readable. */
	float RunGenLayerHeight(int32 Layer)
	{
		static const float Heights[] = { 0.f, 60.f, 110.f, 140.f, 100.f, 180.f, 120.f };
		return Heights[FMath::Clamp(Layer, 0, int32(UE_ARRAY_COUNT(Heights)) - 1)];
	}

	/** Lane offset along Y, centred on 0: lower Y is farther from the camera (higher on screen). */
	float RunGenLaneY(int32 Lane, int32 LaneCount, float Spacing)
	{
		return (Lane - (LaneCount - 1) * 0.5f) * Spacing;
	}

	/**
	 * Non-crossing "staircase" between two adjacent layers: walk from (0,0) to (Last,Last), each step
	 * advancing the source, the target or both, and connect every visited (source, target) pair.
	 * Every node gets at least one edge, lines never cross, and each node keeps 1-2 edges on the
	 * transition when the layer sizes allow it. Returns false if the walk got stuck (candidate rejected).
	 */
	bool RunGenConnectLayers(FRandomStream& Rng, int32 SourceCount, int32 TargetCount, TArray<TPair<int32, int32>>& OutEdges)
	{
		OutEdges.Reset();
		TArray<int32> OutDegree;
		TArray<int32> InDegree;
		OutDegree.Init(0, SourceCount);
		InDegree.Init(0, TargetCount);

		int32 Source = 0;
		int32 Target = 0;
		OutEdges.Add({ 0, 0 });
		OutDegree[0] = InDegree[0] = 1;

		while (Source < SourceCount - 1 || Target < TargetCount - 1)
		{
			TArray<int32, TInlineAllocator<3>> Options; // 0 = next source, 1 = next target, 2 = both
			if (Source + 1 < SourceCount && InDegree[Target] < 2)
			{
				Options.Add(0);
			}
			if (Target + 1 < TargetCount && OutDegree[Source] < 2)
			{
				Options.Add(1);
			}
			if (Source + 1 < SourceCount && Target + 1 < TargetCount)
			{
				Options.Add(2);
			}
			if (Options.Num() == 0)
			{
				return false;
			}

			const int32 Step = Options[Rng.RandRange(0, Options.Num() - 1)];
			Source += (Step == 0 || Step == 2) ? 1 : 0;
			Target += (Step == 1 || Step == 2) ? 1 : 0;
			OutEdges.Add({ Source, Target });
			++OutDegree[Source];
			++InDegree[Target];
		}
		return true;
	}

	ECRRoomType RunGenPickNormalRoom(FRandomStream& Rng, const FCRRunGenerationConfig& Config, bool bAllowShop)
	{
		const float ShopWeight = bAllowShop ? Config.ShopWeight : 0.f;
		const float Total = Config.CombatWeight + Config.EventWeight + ShopWeight;
		const float Roll = Rng.FRandRange(0.f, Total);
		if (Roll < Config.CombatWeight)
		{
			return ECRRoomType::Combat;
		}
		return Roll < Config.CombatWeight + Config.EventWeight ? ECRRoomType::Event : ECRRoomType::Shop;
	}

	/** Index lookups for one candidate graph (edges only go to the next layer). */
	struct FRunGenGraphIndex
	{
		TMap<FName, int32> IndexOf;
		TArray<TArray<int32>> Children;
		TArray<TArray<int32>> Parents;

		explicit FRunGenGraphIndex(const TArray<FCRRunNodeData>& Nodes)
		{
			Children.SetNum(Nodes.Num());
			Parents.SetNum(Nodes.Num());
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				IndexOf.Add(Nodes[i].NodeId, i);
			}
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				for (const FName Next : Nodes[i].ConnectedNodeIds)
				{
					if (const int32* Child = IndexOf.Find(Next))
					{
						Children[i].Add(*Child);
						Parents[*Child].Add(i);
					}
				}
			}
		}
	};

	/**
	 * Start -> Boss path with the fewest Combat rooms (ties broken by the lowest lane, so it is
	 * deterministic). Returns the node indices on that path and the number of combats in OutCombats.
	 */
	TArray<int32> RunGenWeakestPath(const TArray<FCRRunNodeData>& Nodes, const FRunGenGraphIndex& Index, int32& OutCombats)
	{
		TArray<int32> Best;
		TArray<int32> Next;
		Best.Init(MAX_int32, Nodes.Num());
		Next.Init(INDEX_NONE, Nodes.Num());
		const int32* BossIndex = Index.IndexOf.Find(CRRunGen::BossNodeId());
		const int32* StartIndex = Index.IndexOf.Find(CRRunGen::StartNodeId());
		if (!BossIndex || !StartIndex)
		{
			OutCombats = 0;
			return {};
		}

		// Nodes are stored layer by layer, so walking backwards visits children before parents.
		Best[*BossIndex] = 0;
		for (int32 i = Nodes.Num() - 1; i >= 0; --i)
		{
			if (i == *BossIndex)
			{
				continue;
			}
			for (const int32 Child : Index.Children[i])
			{
				if (Best[Child] != MAX_int32 && Best[Child] < Best[i])
				{
					Best[i] = Best[Child];
					Next[i] = Child;
				}
			}
			if (Best[i] != MAX_int32 && Nodes[i].RoomType == ECRRoomType::Combat)
			{
				++Best[i];
			}
		}

		TArray<int32> Path;
		for (int32 i = *StartIndex; i != INDEX_NONE && i != *BossIndex; i = Next[i])
		{
			Path.Add(i);
		}
		OutCombats = Best[*StartIndex] == MAX_int32 ? 0 : Best[*StartIndex];
		return Path;
	}

	bool RunGenIsNormalLayer(const FCRRunGenerationConfig& Config, int32 Layer)
	{
		return Layer >= 1 && Layer < Config.LayerSizes.Num() - 2;
	}

	/** True if node I may become a Shop: no Shop parent/child, and every path still fights often enough. */
	bool RunGenCanBecome(TArray<FCRRunNodeData>& Nodes, const FRunGenGraphIndex& Index, const FCRRunGenerationConfig& Config, int32 I, ECRRoomType NewType)
	{
		if (NewType == ECRRoomType::Shop)
		{
			for (const int32 Other : Index.Children[I])
			{
				if (Nodes[Other].RoomType == ECRRoomType::Shop)
				{
					return false;
				}
			}
			for (const int32 Other : Index.Parents[I])
			{
				if (Nodes[Other].RoomType == ECRRoomType::Shop)
				{
					return false;
				}
			}
		}
		const ECRRoomType Old = Nodes[I].RoomType;
		Nodes[I].RoomType = NewType;
		int32 Combats = 0;
		RunGenWeakestPath(Nodes, Index, Combats);
		Nodes[I].RoomType = Old;
		return Combats >= Config.MinCombatPerPath;
	}

	int32 RunGenCountType(const TArray<FCRRunNodeData>& Nodes, ECRRoomType Type)
	{
		int32 Count = 0;
		for (const FCRRunNodeData& Node : Nodes)
		{
			Count += Node.RoomType == Type ? 1 : 0;
		}
		return Count;
	}

	/**
	 * Seeded repair after the weighted roll, so almost every candidate is usable instead of being rejected:
	 * 1) rooms on the weakest path become Combat until every path fights MinCombatPerPath times;
	 * 2) missing Shops / Events are converted from rooms whose change keeps every rule intact.
	 * Layer 1 (the fixed Combat + Event opening) is never changed.
	 */
	void RunGenRepairRooms(FRandomStream& Rng, TArray<FCRRunNodeData>& Nodes, const FCRRunGenerationConfig& Config)
	{
		const FRunGenGraphIndex Index(Nodes);

		for (int32 Guard = 0; Guard < Nodes.Num(); ++Guard)
		{
			int32 Combats = 0;
			const TArray<int32> Path = RunGenWeakestPath(Nodes, Index, Combats);
			if (Combats >= Config.MinCombatPerPath)
			{
				break;
			}
			TArray<int32> Candidates;
			for (const int32 I : Path)
			{
				if (Nodes[I].Layer >= 2 && RunGenIsNormalLayer(Config, Nodes[I].Layer) && Nodes[I].RoomType != ECRRoomType::Combat)
				{
					Candidates.Add(I);
				}
			}
			if (Candidates.Num() == 0)
			{
				break; // validation will reject this candidate
			}
			Nodes[Candidates[Rng.RandRange(0, Candidates.Num() - 1)]].RoomType = ECRRoomType::Combat;
		}

		auto TopUp = [&](ECRRoomType Wanted, int32 Minimum)
		{
			for (int32 Guard = 0; Guard < Nodes.Num() && RunGenCountType(Nodes, Wanted) < Minimum; ++Guard)
			{
				TArray<int32> Candidates;
				for (int32 I = 0; I < Nodes.Num(); ++I)
				{
					const ECRRoomType Type = Nodes[I].RoomType;
					const bool bSpare = Type == ECRRoomType::Combat
						? RunGenCountType(Nodes, ECRRoomType::Combat) > Config.MinCombatRooms
						: (Type == ECRRoomType::Event && Wanted == ECRRoomType::Shop && RunGenCountType(Nodes, ECRRoomType::Event) > Config.MinEventRooms);
					if (Nodes[I].Layer >= 2 && RunGenIsNormalLayer(Config, Nodes[I].Layer) && Type != Wanted && bSpare
						&& RunGenCanBecome(Nodes, Index, Config, I, Wanted))
					{
						Candidates.Add(I);
					}
				}
				if (Candidates.Num() == 0)
				{
					break;
				}
				Nodes[Candidates[Rng.RandRange(0, Candidates.Num() - 1)]].RoomType = Wanted;
			}
		};
		TopUp(ECRRoomType::Shop, Config.MinShopRooms);
		TopUp(ECRRoomType::Event, Config.MinEventRooms);
	}

	/** Builds the node list skeleton (ids, layers, positions, Boss->Return) for the configured layers. */
	TArray<FCRRunNodeData> RunGenSkeleton(const FCRRunGenerationConfig& Config)
	{
		TArray<FCRRunNodeData> Nodes;
		const int32 LayerCount = Config.LayerSizes.Num();
		for (int32 Layer = 0; Layer < LayerCount; ++Layer)
		{
			const int32 Lanes = Config.LayerSizes[Layer];
			for (int32 Lane = 0; Lane < Lanes; ++Lane)
			{
				FCRRunNodeData Node;
				Node.Layer = Layer;
				if (Layer == 0)
				{
					Node.NodeId = CRRunGen::StartNodeId();
					Node.RoomType = ECRRoomType::Start;
				}
				else if (Layer == LayerCount - 2)
				{
					Node.NodeId = CRRunGen::BossNodeId();
					Node.RoomType = ECRRoomType::Boss;
				}
				else if (Layer == LayerCount - 1)
				{
					Node.NodeId = CRRunGen::ReturnNodeId();
					Node.RoomType = ECRRoomType::Return;
				}
				else
				{
					Node.NodeId = CRRunGen::MakeNodeId(Layer, Lane);
				}
				Node.Position = FVector(Layer * Config.LayerSpacingX, RunGenLaneY(Lane, Lanes, Config.LaneSpacingY), RunGenLayerHeight(Layer));
				Nodes.Add(Node);
			}
		}
		return Nodes;
	}

	/** Index of the node at (Layer, Lane) in a skeleton built by RunGenSkeleton. */
	int32 RunGenIndexOf(const FCRRunGenerationConfig& Config, int32 Layer, int32 Lane)
	{
		int32 Index = 0;
		for (int32 L = 0; L < Layer; ++L)
		{
			Index += Config.LayerSizes[L];
		}
		return Index + Lane;
	}

	bool RunGenConfigIsSupported(const FCRRunGenerationConfig& Config, FString& OutReason)
	{
		const int32 LayerCount = Config.LayerSizes.Num();
		if (LayerCount < 4 || Config.LayerSizes[0] != 1 || Config.LayerSizes[LayerCount - 2] != 1 || Config.LayerSizes[LayerCount - 1] != 1)
		{
			OutReason = TEXT("layer sizes must be Start(1), normal layers..., Boss(1), Return(1)");
			return false;
		}
		if (Config.LayerSizes[1] != 2)
		{
			OutReason = TEXT("layer 1 must have exactly 2 rooms (one Combat, one Event)");
			return false;
		}
		for (int32 Layer = 1; Layer < LayerCount - 2; ++Layer)
		{
			if (Config.LayerSizes[Layer] < 1)
			{
				OutReason = TEXT("normal layers need at least one room");
				return false;
			}
		}
		return true;
	}
}

namespace CRRunGen
{
	FName MakeNodeId(int32 Layer, int32 Lane)
	{
		return FName(*FString::Printf(TEXT("L%d_%c"), Layer, TCHAR('A' + Lane)));
	}

	int32 CountEdges(const TArray<FCRRunNodeData>& Nodes)
	{
		int32 Count = 0;
		for (const FCRRunNodeData& Node : Nodes)
		{
			Count += Node.ConnectedNodeIds.Num();
		}
		return Count;
	}

	FCRRunGenerationResult Generate(int32 Seed, const FCRRunGenerationConfig& Config)
	{
		FCRRunGenerationResult Result;
		Result.Seed = Seed;

		FString ConfigReason;
		if (!RunGenConfigIsSupported(Config, ConfigReason))
		{
			Result.LastRejectReason = FString::Printf(TEXT("unsupported config: %s"), *ConfigReason);
			Result.Nodes = BuildFallbackGraph(FCRRunGenerationConfig());
			Result.bUsedFallback = true;
			return Result;
		}

		// One stream for the whole generation, retries included: the same seed replays the same attempts.
		FRandomStream Rng(Seed);
		const int32 LayerCount = Config.LayerSizes.Num();
		const int32 BossLayer = LayerCount - 2;

		for (int32 Attempt = 1; Attempt <= FMath::Max(1, Config.MaxAttempts); ++Attempt)
		{
			Result.Attempts = Attempt;
			TArray<FCRRunNodeData> Nodes = RunGenSkeleton(Config);
			bool bTopologyOk = true;

			// Topology. Start fans out to all of layer 1; the last normal layer all leads to Boss.
			for (int32 Layer = 0; Layer < LayerCount - 1 && bTopologyOk; ++Layer)
			{
				const int32 Sources = Config.LayerSizes[Layer];
				const int32 Targets = Config.LayerSizes[Layer + 1];
				TArray<TPair<int32, int32>> Edges;
				if (Sources == 1 || Targets == 1)
				{
					for (int32 S = 0; S < Sources; ++S)
					{
						for (int32 T = 0; T < Targets; ++T)
						{
							Edges.Add({ S, T });
						}
					}
				}
				else if (!RunGenConnectLayers(Rng, Sources, Targets, Edges))
				{
					bTopologyOk = false;
					Result.LastRejectReason = FString::Printf(TEXT("could not connect layer %d to %d"), Layer, Layer + 1);
					break;
				}
				for (const TPair<int32, int32>& Edge : Edges)
				{
					FCRRunNodeData& From = Nodes[RunGenIndexOf(Config, Layer, Edge.Key)];
					From.ConnectedNodeIds.AddUnique(Nodes[RunGenIndexOf(Config, Layer + 1, Edge.Value)].NodeId);
				}
			}
			if (!bTopologyOk)
			{
				continue;
			}

			// Room types. Layer 1 is always the prototype's opening choice: one fight, one event.
			const bool bCombatOnTop = Rng.RandRange(0, 1) == 0;
			Nodes[RunGenIndexOf(Config, 1, 0)].RoomType = bCombatOnTop ? ECRRoomType::Combat : ECRRoomType::Event;
			Nodes[RunGenIndexOf(Config, 1, 1)].RoomType = bCombatOnTop ? ECRRoomType::Event : ECRRoomType::Combat;
			// Weighted roll, layer by layer; a room right after a Shop is never another Shop.
			const FRunGenGraphIndex RollIndex(Nodes);
			for (int32 Layer = 2; Layer < BossLayer; ++Layer)
			{
				for (int32 Lane = 0; Lane < Config.LayerSizes[Layer]; ++Lane)
				{
					const int32 I = RunGenIndexOf(Config, Layer, Lane);
					bool bShopParent = false;
					for (const int32 Parent : RollIndex.Parents[I])
					{
						bShopParent |= Nodes[Parent].RoomType == ECRRoomType::Shop;
					}
					Nodes[I].RoomType = RunGenPickNormalRoom(Rng, Config, !bShopParent);
				}
			}
			RunGenRepairRooms(Rng, Nodes, Config);
			for (FCRRunNodeData& Node : Nodes)
			{
				if (Node.RoomType == ECRRoomType::Event)
				{
					Node.EventPool = TSoftObjectPtr<UCREventPool>(FSoftObjectPath(Config.EventPoolPath));
				}
			}

			TArray<FString> Errors;
			if (ValidateGraph(Nodes, Config, Errors))
			{
				Result.Nodes = MoveTemp(Nodes);
				return Result;
			}
			Result.LastRejectReason = Errors.Num() > 0 ? Errors[0] : FString(TEXT("invalid"));
		}

		// Should never happen with the default rules; keep the run playable with the same structural ids.
		Result.Nodes = BuildFallbackGraph(Config);
		Result.bUsedFallback = true;
		return Result;
	}

	TArray<FCRRunNodeData> BuildFallbackGraph(const FCRRunGenerationConfig& InConfig)
	{
		// Fixed graph for the default 1/2/3/3/2/1/1 layout (other layouts fall back to the defaults).
		FCRRunGenerationConfig Config = InConfig;
		Config.LayerSizes = { 1, 2, 3, 3, 2, 1, 1 };
		TArray<FCRRunNodeData> Nodes = RunGenSkeleton(Config);

		auto Find = [&Nodes](const TCHAR* Id) -> FCRRunNodeData& { return *Nodes.FindByPredicate([Id](const FCRRunNodeData& N) { return N.NodeId == FName(Id); }); };
		auto Connect = [&Find](const TCHAR* From, std::initializer_list<const TCHAR*> To)
		{
			for (const TCHAR* Id : To)
			{
				Find(From).ConnectedNodeIds.Add(FName(Id));
			}
		};
		auto SetType = [&Find](const TCHAR* Id, ECRRoomType Type) { Find(Id).RoomType = Type; };

		Connect(TEXT("Start"), { TEXT("L1_A"), TEXT("L1_B") });
		Connect(TEXT("L1_A"), { TEXT("L2_A"), TEXT("L2_B") });
		Connect(TEXT("L1_B"), { TEXT("L2_B"), TEXT("L2_C") });
		Connect(TEXT("L2_A"), { TEXT("L3_A") });
		Connect(TEXT("L2_B"), { TEXT("L3_B") });
		Connect(TEXT("L2_C"), { TEXT("L3_C") });
		Connect(TEXT("L3_A"), { TEXT("L4_A") });
		Connect(TEXT("L3_B"), { TEXT("L4_A"), TEXT("L4_B") });
		Connect(TEXT("L3_C"), { TEXT("L4_B") });
		Connect(TEXT("L4_A"), { TEXT("Boss") });
		Connect(TEXT("L4_B"), { TEXT("Boss") });
		Connect(TEXT("Boss"), { TEXT("Return") });

		// Every path fights at least twice; 6 Combat / 3 Event / 1 Shop, no Shop -> Shop.
		SetType(TEXT("L1_A"), ECRRoomType::Combat);
		SetType(TEXT("L1_B"), ECRRoomType::Event);
		SetType(TEXT("L2_A"), ECRRoomType::Event);
		SetType(TEXT("L2_B"), ECRRoomType::Combat);
		SetType(TEXT("L2_C"), ECRRoomType::Shop);
		SetType(TEXT("L3_A"), ECRRoomType::Combat);
		SetType(TEXT("L3_B"), ECRRoomType::Event);
		SetType(TEXT("L3_C"), ECRRoomType::Combat);
		SetType(TEXT("L4_A"), ECRRoomType::Combat);
		SetType(TEXT("L4_B"), ECRRoomType::Combat);

		for (FCRRunNodeData& Node : Nodes)
		{
			if (Node.RoomType == ECRRoomType::Event)
			{
				Node.EventPool = TSoftObjectPtr<UCREventPool>(FSoftObjectPath(Config.EventPoolPath));
			}
		}
		return Nodes;
	}

	bool ValidateGraph(const TArray<FCRRunNodeData>& Nodes, const FCRRunGenerationConfig& Config, TArray<FString>& OutErrors)
	{
		OutErrors.Reset();
		auto Fail = [&OutErrors](const FString& Error) { OutErrors.Add(Error); };

		// Node count and ids.
		int32 Expected = 0;
		for (const int32 Size : Config.LayerSizes)
		{
			Expected += Size;
		}
		if (Nodes.Num() != Expected)
		{
			Fail(FString::Printf(TEXT("expected %d nodes, got %d"), Expected, Nodes.Num()));
		}

		TMap<FName, const FCRRunNodeData*> ById;
		int32 StartCount = 0;
		int32 BossCount = 0;
		int32 ReturnCount = 0;
		for (const FCRRunNodeData& Node : Nodes)
		{
			if (ById.Contains(Node.NodeId))
			{
				Fail(FString::Printf(TEXT("duplicate node id %s"), *Node.NodeId.ToString()));
			}
			ById.Add(Node.NodeId, &Node);
			StartCount += Node.RoomType == ECRRoomType::Start ? 1 : 0;
			BossCount += Node.RoomType == ECRRoomType::Boss ? 1 : 0;
			ReturnCount += Node.RoomType == ECRRoomType::Return ? 1 : 0;
		}
		if (StartCount != 1 || BossCount != 1 || ReturnCount != 1)
		{
			Fail(FString::Printf(TEXT("need exactly one Start/Boss/Return (got %d/%d/%d)"), StartCount, BossCount, ReturnCount));
		}
		const FCRRunNodeData* const* StartPtr = ById.Find(CRRunGen::StartNodeId());
		const FCRRunNodeData* const* BossPtr = ById.Find(CRRunGen::BossNodeId());
		const FCRRunNodeData* const* ReturnPtr = ById.Find(CRRunGen::ReturnNodeId());
		if (!StartPtr || !BossPtr || !ReturnPtr)
		{
			Fail(TEXT("missing Start, Boss or Return node id"));
			return false;
		}
		if ((*StartPtr)->RoomType != ECRRoomType::Start || (*BossPtr)->RoomType != ECRRoomType::Boss || (*ReturnPtr)->RoomType != ECRRoomType::Return)
		{
			Fail(TEXT("Start/Boss/Return ids do not carry their room types"));
		}

		// Edges.
		TMap<FName, int32> InDegree;
		for (const FCRRunNodeData& Node : Nodes)
		{
			TSet<FName> Seen;
			for (const FName Next : Node.ConnectedNodeIds)
			{
				const FCRRunNodeData* const* Target = ById.Find(Next);
				if (!Target)
				{
					Fail(FString::Printf(TEXT("%s links to missing node %s"), *Node.NodeId.ToString(), *Next.ToString()));
					continue;
				}
				if (Next == Node.NodeId)
				{
					Fail(FString::Printf(TEXT("self edge on %s"), *Node.NodeId.ToString()));
				}
				if (Seen.Contains(Next))
				{
					Fail(FString::Printf(TEXT("duplicate edge %s -> %s"), *Node.NodeId.ToString(), *Next.ToString()));
				}
				Seen.Add(Next);
				if ((*Target)->Layer != Node.Layer + 1)
				{
					Fail(FString::Printf(TEXT("edge %s (L%d) -> %s (L%d) does not advance exactly one layer"),
						*Node.NodeId.ToString(), Node.Layer, *Next.ToString(), (*Target)->Layer));
				}
				if (Node.RoomType == ECRRoomType::Shop && (*Target)->RoomType == ECRRoomType::Shop)
				{
					Fail(FString::Printf(TEXT("Shop -> Shop edge %s -> %s"), *Node.NodeId.ToString(), *Next.ToString()));
				}
				InDegree.FindOrAdd(Next)++;
			}
			if (Node.RoomType != ECRRoomType::Return && Node.ConnectedNodeIds.Num() == 0)
			{
				Fail(FString::Printf(TEXT("%s has no outgoing edge"), *Node.NodeId.ToString()));
			}
			if (Node.RoomType != ECRRoomType::Start && InDegree.FindRef(Node.NodeId) == 0)
			{
				// Checked again below once all edges are counted.
			}
			if (Node.RoomType == ECRRoomType::Event && Node.EventPool.IsNull())
			{
				Fail(FString::Printf(TEXT("event node %s has no EventPool"), *Node.NodeId.ToString()));
			}
		}
		for (const FCRRunNodeData& Node : Nodes)
		{
			if (Node.RoomType != ECRRoomType::Start && InDegree.FindRef(Node.NodeId) == 0)
			{
				Fail(FString::Printf(TEXT("%s has no incoming edge"), *Node.NodeId.ToString()));
			}
		}
		if ((*BossPtr)->ConnectedNodeIds.Num() != 1 || (*BossPtr)->ConnectedNodeIds[0] != CRRunGen::ReturnNodeId())
		{
			Fail(TEXT("Boss must lead only to Return"));
		}
		if ((*ReturnPtr)->ConnectedNodeIds.Num() != 0)
		{
			Fail(TEXT("Return must be the end of the route"));
		}
		if ((*StartPtr)->ConnectedNodeIds.Num() != 2)
		{
			Fail(FString::Printf(TEXT("Start must offer 2 choices (has %d)"), (*StartPtr)->ConnectedNodeIds.Num()));
		}

		// Reachability from Start (also rules out cycles: every edge advances a layer).
		TSet<FName> Reached;
		TArray<FName> Stack = { CRRunGen::StartNodeId() };
		while (Stack.Num() > 0)
		{
			const FName Id = Stack.Pop();
			if (Reached.Contains(Id))
			{
				continue;
			}
			Reached.Add(Id);
			if (const FCRRunNodeData* const* Node = ById.Find(Id))
			{
				for (const FName Next : (*Node)->ConnectedNodeIds)
				{
					if (ById.Contains(Next))
					{
						Stack.Push(Next);
					}
				}
			}
		}
		for (const FCRRunNodeData& Node : Nodes)
		{
			if (!Reached.Contains(Node.NodeId))
			{
				Fail(FString::Printf(TEXT("%s is not reachable from Start"), *Node.NodeId.ToString()));
			}
		}

		// Everything reaches Return, and cycle check by explicit DFS colouring.
		TMap<FName, int32> State; // 0 unvisited, 1 on stack, 2 done
		TMap<FName, bool> ReachesReturn;
		TFunction<bool(FName)> Visit = [&](FName Id) -> bool
		{
			int32& S = State.FindOrAdd(Id);
			if (S == 1)
			{
				Fail(FString::Printf(TEXT("cycle through %s"), *Id.ToString()));
				return false;
			}
			if (S == 2)
			{
				return ReachesReturn.FindRef(Id);
			}
			S = 1;
			bool bReaches = Id == CRRunGen::ReturnNodeId();
			if (const FCRRunNodeData* const* Node = ById.Find(Id))
			{
				for (const FName Next : (*Node)->ConnectedNodeIds)
				{
					if (ById.Contains(Next))
					{
						bReaches |= Visit(Next);
					}
				}
			}
			State.FindOrAdd(Id) = 2;
			ReachesReturn.Add(Id, bReaches);
			return bReaches;
		};
		for (const FCRRunNodeData& Node : Nodes)
		{
			if (!Visit(Node.NodeId))
			{
				Fail(FString::Printf(TEXT("%s cannot reach Return"), *Node.NodeId.ToString()));
			}
		}

		// Room rules.
		int32 Combats = 0;
		int32 Events = 0;
		int32 Shops = 0;
		int32 Layer1Combat = 0;
		int32 Layer1Event = 0;
		for (const FCRRunNodeData& Node : Nodes)
		{
			Combats += Node.RoomType == ECRRoomType::Combat ? 1 : 0;
			Events += Node.RoomType == ECRRoomType::Event ? 1 : 0;
			Shops += Node.RoomType == ECRRoomType::Shop ? 1 : 0;
			if (Node.Layer == 1)
			{
				Layer1Combat += Node.RoomType == ECRRoomType::Combat ? 1 : 0;
				Layer1Event += Node.RoomType == ECRRoomType::Event ? 1 : 0;
				if (Node.RoomType == ECRRoomType::Shop)
				{
					Fail(FString::Printf(TEXT("Shop %s directly after Start"), *Node.NodeId.ToString()));
				}
			}
			const bool bNormalLayer = Node.Layer >= 1 && Node.Layer < Config.LayerSizes.Num() - 2;
			const bool bNormalType = Node.RoomType == ECRRoomType::Combat || Node.RoomType == ECRRoomType::Event || Node.RoomType == ECRRoomType::Shop;
			if (bNormalLayer != bNormalType)
			{
				Fail(FString::Printf(TEXT("%s has room type %s on layer %d"), *Node.NodeId.ToString(), *CRRun::RoomTypeName(Node.RoomType), Node.Layer));
			}
		}
		if (Layer1Combat != 1 || Layer1Event != 1)
		{
			Fail(FString::Printf(TEXT("layer 1 must be one Combat + one Event (got %d Combat, %d Event)"), Layer1Combat, Layer1Event));
		}
		if (Combats < Config.MinCombatRooms || Events < Config.MinEventRooms || Shops < Config.MinShopRooms)
		{
			Fail(FString::Printf(TEXT("room minimums not met: Combat %d/%d, Event %d/%d, Shop %d/%d"),
				Combats, Config.MinCombatRooms, Events, Config.MinEventRooms, Shops, Config.MinShopRooms));
		}

		// Fewest combats on any Start -> Boss path (DP over the layered DAG).
		TMap<FName, int32> MinCombat;
		TFunction<int32(FName)> Fewest = [&](FName Id) -> int32
		{
			if (const int32* Known = MinCombat.Find(Id))
			{
				return *Known;
			}
			const FCRRunNodeData* const* Node = ById.Find(Id);
			if (!Node || Id == CRRunGen::BossNodeId())
			{
				return 0;
			}
			int32 Best = MAX_int32;
			for (const FName Next : (*Node)->ConnectedNodeIds)
			{
				if (ById.Contains(Next) && (*ById.Find(Next))->Layer > (*Node)->Layer)
				{
					Best = FMath::Min(Best, Fewest(Next));
				}
			}
			const int32 Value = (Best == MAX_int32 ? 0 : Best) + ((*Node)->RoomType == ECRRoomType::Combat ? 1 : 0);
			MinCombat.Add(Id, Value);
			return Value;
		};
		const int32 FewestCombats = Fewest(CRRunGen::StartNodeId());
		if (FewestCombats < Config.MinCombatPerPath)
		{
			Fail(FString::Printf(TEXT("a Start -> Boss path has only %d Combat room(s), need %d"), FewestCombats, Config.MinCombatPerPath));
		}

		return OutErrors.Num() == 0;
	}

	TArray<TArray<FName>> EnumeratePathsToBoss(const TArray<FCRRunNodeData>& Nodes)
	{
		TMap<FName, const FCRRunNodeData*> ById;
		for (const FCRRunNodeData& Node : Nodes)
		{
			ById.Add(Node.NodeId, &Node);
		}

		TArray<TArray<FName>> Paths;
		TArray<FName> Current;
		TFunction<void(FName)> Walk = [&](FName Id)
		{
			Current.Add(Id);
			if (Id == CRRunGen::BossNodeId())
			{
				Paths.Add(Current);
			}
			else if (const FCRRunNodeData* const* Node = ById.Find(Id); Node && Current.Num() <= Nodes.Num())
			{
				for (const FName Next : (*Node)->ConnectedNodeIds)
				{
					Walk(Next);
				}
			}
			Current.Pop();
		};
		Walk(CRRunGen::StartNodeId());
		return Paths;
	}

	FString GraphSignature(const TArray<FCRRunNodeData>& Nodes)
	{
		FString Signature;
		for (const FCRRunNodeData& Node : Nodes)
		{
			Signature += Node.NodeId.ToString() + TEXT(":") + CRRun::RoomTypeName(Node.RoomType).Left(2) + TEXT(">");
			for (const FName Next : Node.ConnectedNodeIds)
			{
				Signature += Next.ToString() + TEXT(",");
			}
			Signature += TEXT(";");
		}
		return Signature;
	}
}
