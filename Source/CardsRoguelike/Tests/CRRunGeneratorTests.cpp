// Automation tests for the procedural run generator (Session Frontend / Automation:
// "CardsRoguelike.Run.ProceduralGeneration"). Pure data: no World or map needed.

#include "../Run/CRRunGenerator.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 RunGenTestFirstSeed = 1;
	constexpr int32 RunGenTestLastSeed = 2000;

	bool RunGenSameGraph(const TArray<FCRRunNodeData>& A, const TArray<FCRRunNodeData>& B, FString& OutDiff)
	{
		if (A.Num() != B.Num())
		{
			OutDiff = FString::Printf(TEXT("node count %d vs %d"), A.Num(), B.Num());
			return false;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const FCRRunNodeData& X = A[i];
			const FCRRunNodeData& Y = B[i];
			if (X.NodeId != Y.NodeId || X.RoomType != Y.RoomType || X.Layer != Y.Layer || !X.Position.Equals(Y.Position, 0.f)
				|| X.ConnectedNodeIds != Y.ConnectedNodeIds || X.EventPool != Y.EventPool)
			{
				OutDiff = FString::Printf(TEXT("node %d (%s vs %s) differs"), i, *X.NodeId.ToString(), *Y.NodeId.ToString());
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRRunGenValidSeedsTest, "CardsRoguelike.Run.ProceduralGeneration.ValidSeeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRRunGenValidSeedsTest::RunTest(const FString& Parameters)
{
	const FCRRunGenerationConfig Config;
	int32 Failures = 0;
	int32 TotalAttempts = 0;
	int32 MaxAttempts = 0;
	for (int32 Seed = RunGenTestFirstSeed; Seed <= RunGenTestLastSeed; ++Seed)
	{
		const FCRRunGenerationResult Result = CRRunGen::Generate(Seed, Config);
		TArray<FString> Errors;
		const bool bValid = CRRunGen::ValidateGraph(Result.Nodes, Config, Errors);
		TotalAttempts += Result.Attempts;
		MaxAttempts = FMath::Max(MaxAttempts, Result.Attempts);
		if (Result.bUsedFallback || !bValid)
		{
			++Failures;
			AddError(FString::Printf(TEXT("seed %d: fallback=%d valid=%d (%s)"), Seed, Result.bUsedFallback, bValid,
				Errors.Num() > 0 ? *Errors[0] : *Result.LastRejectReason));
		}
	}
	AddInfo(FString::Printf(TEXT("%d seeds generated and validated, %d failures, attempts avg %.2f / max %d"),
		RunGenTestLastSeed - RunGenTestFirstSeed + 1, Failures, float(TotalAttempts) / (RunGenTestLastSeed - RunGenTestFirstSeed + 1), MaxAttempts));
	return Failures == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRRunGenDeterminismTest, "CardsRoguelike.Run.ProceduralGeneration.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRRunGenDeterminismTest::RunTest(const FString& Parameters)
{
	int32 Mismatches = 0;
	for (int32 Seed = RunGenTestFirstSeed; Seed <= RunGenTestLastSeed; ++Seed)
	{
		const FCRRunGenerationResult First = CRRunGen::Generate(Seed);
		const FCRRunGenerationResult Second = CRRunGen::Generate(Seed);
		FString Diff;
		if (!RunGenSameGraph(First.Nodes, Second.Nodes, Diff) || First.Attempts != Second.Attempts)
		{
			++Mismatches;
			AddError(FString::Printf(TEXT("seed %d is not deterministic: %s"), Seed, *Diff));
		}
	}
	AddInfo(FString::Printf(TEXT("%d seeds generated twice, %d mismatches"), RunGenTestLastSeed - RunGenTestFirstSeed + 1, Mismatches));
	return Mismatches == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRRunGenDiversityTest, "CardsRoguelike.Run.ProceduralGeneration.Diversity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRRunGenDiversityTest::RunTest(const FString& Parameters)
{
	TSet<FString> Signatures;
	TSet<FString> Topologies;
	int32 RoomTotals[3] = { 0, 0, 0 };
	int32 TwoEventRouteSeed = 0;
	int32 ShopRouteSeed = 0;
	for (int32 Seed = RunGenTestFirstSeed; Seed <= RunGenTestLastSeed; ++Seed)
	{
		const TArray<FCRRunNodeData> Nodes = CRRunGen::Generate(Seed).Nodes;
		Signatures.Add(CRRunGen::GraphSignature(Nodes));

		FString Topology;
		TMap<FName, ECRRoomType> Types;
		for (const FCRRunNodeData& Node : Nodes)
		{
			Topology += Node.NodeId.ToString() + TEXT(">") + FString::JoinBy(Node.ConnectedNodeIds, TEXT(","), [](FName Id) { return Id.ToString(); }) + TEXT(";");
			Types.Add(Node.NodeId, Node.RoomType);
			RoomTotals[0] += Node.RoomType == ECRRoomType::Combat ? 1 : 0;
			RoomTotals[1] += Node.RoomType == ECRRoomType::Event ? 1 : 0;
			RoomTotals[2] += Node.RoomType == ECRRoomType::Shop ? 1 : 0;
		}
		Topologies.Add(Topology);

		// Example seeds for manual PIE checks (a route with two events, a route through a shop).
		for (const TArray<FName>& Path : CRRunGen::EnumeratePathsToBoss(Nodes))
		{
			int32 PathEvents = 0;
			bool bPathShop = false;
			for (const FName Id : Path)
			{
				PathEvents += Types.FindRef(Id) == ECRRoomType::Event ? 1 : 0;
				bPathShop |= Types.FindRef(Id) == ECRRoomType::Shop;
			}
			// With 4 rooms and at least 2 fights per path, a route holds at most 2 non-combat rooms.
			if (!TwoEventRouteSeed && PathEvents >= 2)
			{
				TwoEventRouteSeed = Seed;
			}
			if (!ShopRouteSeed && bPathShop)
			{
				ShopRouteSeed = Seed;
			}
		}
	}
	const float Normal = float(RoomTotals[0] + RoomTotals[1] + RoomTotals[2]);
	AddInfo(FString::Printf(TEXT("%d seeds: %d unique graphs (types+edges), %d unique topologies; rooms Combat %.1f%% Event %.1f%% Shop %.1f%%"),
		RunGenTestLastSeed - RunGenTestFirstSeed + 1, Signatures.Num(), Topologies.Num(),
		100.f * RoomTotals[0] / Normal, 100.f * RoomTotals[1] / Normal, 100.f * RoomTotals[2] / Normal));
	AddInfo(FString::Printf(TEXT("Example seeds: two-event route %d, shop route %d"), TwoEventRouteSeed, ShopRouteSeed));
	TestTrue(TEXT("more than one distinct graph"), Signatures.Num() > 1);
	TestTrue(TEXT("more than one distinct topology (not just retyped rooms)"), Topologies.Num() > 1);
	return Signatures.Num() > 1 && Topologies.Num() > 1;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRRunGenPathsTest, "CardsRoguelike.Run.ProceduralGeneration.PathEnumeration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRRunGenPathsTest::RunTest(const FString& Parameters)
{
	const FCRRunGenerationConfig Config;
	const int32 NormalLayers = Config.LayerSizes.Num() - 3; // minus Start, Boss, Return
	int32 Failures = 0;
	int32 PathCount = 0;
	int32 MinPaths = MAX_int32;
	int32 MaxPaths = 0;
	for (int32 Seed = RunGenTestFirstSeed; Seed <= RunGenTestLastSeed; ++Seed)
	{
		const TArray<FCRRunNodeData> Nodes = CRRunGen::Generate(Seed, Config).Nodes;
		TMap<FName, ECRRoomType> Types;
		for (const FCRRunNodeData& Node : Nodes)
		{
			Types.Add(Node.NodeId, Node.RoomType);
		}

		const TArray<TArray<FName>> Paths = CRRunGen::EnumeratePathsToBoss(Nodes);
		PathCount += Paths.Num();
		MinPaths = FMath::Min(MinPaths, Paths.Num());
		MaxPaths = FMath::Max(MaxPaths, Paths.Num());
		if (Paths.Num() == 0)
		{
			++Failures;
			AddError(FString::Printf(TEXT("seed %d: Boss unreachable"), Seed));
			continue;
		}
		for (const TArray<FName>& Path : Paths)
		{
			// Path = Start, normal rooms..., Boss.
			const int32 Visits = Path.Num() - 2;
			int32 Combats = 0;
			bool bShopShop = false;
			for (int32 i = 1; i < Path.Num() - 1; ++i)
			{
				Combats += Types.FindRef(Path[i]) == ECRRoomType::Combat ? 1 : 0;
				bShopShop |= i > 1 && Types.FindRef(Path[i]) == ECRRoomType::Shop && Types.FindRef(Path[i - 1]) == ECRRoomType::Shop;
			}
			if (Visits != NormalLayers || Combats < Config.MinCombatPerPath || bShopShop || Path.Last() != CRRunGen::BossNodeId())
			{
				++Failures;
				AddError(FString::Printf(TEXT("seed %d path %s: %d rooms, %d combats, shop->shop %d"), Seed,
					*FString::JoinBy(Path, TEXT(">"), [](FName Id) { return Id.ToString(); }), Visits, Combats, bShopShop));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d seeds, %d Start->Boss paths enumerated (per graph min %d / max %d), %d failures"),
		RunGenTestLastSeed - RunGenTestFirstSeed + 1, PathCount, MinPaths, MaxPaths, Failures));
	return Failures == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCRRunGenFallbackTest, "CardsRoguelike.Run.ProceduralGeneration.Fallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCRRunGenFallbackTest::RunTest(const FString& Parameters)
{
	const FCRRunGenerationConfig Config;
	TArray<FString> Errors;
	const bool bValid = CRRunGen::ValidateGraph(CRRunGen::BuildFallbackGraph(Config), Config, Errors);
	for (const FString& Error : Errors)
	{
		AddError(FString::Printf(TEXT("fallback graph: %s"), *Error));
	}

	// Impossible rules must end in the (valid) fallback, never in an empty or broken graph.
	FCRRunGenerationConfig Impossible = Config;
	Impossible.MinShopRooms = 99;
	const FCRRunGenerationResult Forced = CRRunGen::Generate(123, Impossible);
	TArray<FString> ForcedErrors;
	TestTrue(TEXT("impossible rules use the fallback"), Forced.bUsedFallback);
	TestEqual(TEXT("fallback keeps all nodes"), Forced.Nodes.Num(), 13);
	TestTrue(TEXT("fallback passes the default rules"), CRRunGen::ValidateGraph(Forced.Nodes, Config, ForcedErrors));
	return bValid && Forced.bUsedFallback && ForcedErrors.Num() == 0;
}

#endif // WITH_DEV_AUTOMATION_TESTS
