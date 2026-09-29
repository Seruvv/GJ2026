// Procedural run graph generation (pure data: seed + rules in, nodes out; no World needed).
//
// Layout: layered DAG, Start -> 2 -> 3 -> 3 -> 2 -> Boss -> Return (13 nodes for the M2.6 defaults).
// Edges only lead to the next layer. Middle-layer topology and room types come from one FRandomStream
// seeded with the run seed, so the same seed always rebuilds exactly the same map.

#pragma once

#include "CoreMinimal.h"
#include "CRRunTypes.h"

/** Rules for one generated run. Plain C++ defaults for now; may move to a Data Asset later. */
struct CARDSROGUELIKE_API FCRRunGenerationConfig
{
	/** Node count per layer, Start first. The first entry must be 1 (Start); the last three are the
	 *  final normal layer, Boss (1) and Return (1). */
	TArray<int32> LayerSizes = { 1, 2, 3, 3, 2, 1, 1 };

	/** Relative weights for normal rooms outside layer 1 (layer 1 is always one Combat + one Event).
	 *  Rolled below the ~50/30/20 target because the two-fights-per-path repair turns some rooms into
	 *  Combat; the resulting maps land close to the target mix. */
	float CombatWeight = 30.f;
	float EventWeight = 36.f;
	float ShopWeight = 34.f;

	/** Whole-graph minimums over the normal layers. */
	int32 MinCombatRooms = 4;
	int32 MinEventRooms = 2;
	int32 MinShopRooms = 1;

	/** Every Start -> Boss path must fight at least this many times. */
	int32 MinCombatPerPath = 2;

	/** Candidates tried (same random stream) before falling back to the known-safe graph. */
	int32 MaxAttempts = 100;

	/** Run map placement (world units). X grows from Start toward Return; lanes spread along Y. */
	float LayerSpacingX = 550.f;
	float LaneSpacingY = 650.f;

	/** Event pool assigned to every generated Event node. */
	FString EventPoolPath = CRRun::DefaultEventPoolPath();
};

struct CARDSROGUELIKE_API FCRRunGenerationResult
{
	TArray<FCRRunNodeData> Nodes;
	int32 Seed = 0;
	/** Candidates generated, including the accepted one. */
	int32 Attempts = 0;
	/** True if every candidate failed and the fixed fallback graph was used. */
	bool bUsedFallback = false;
	/** Why the last rejected candidate failed (diagnostics). */
	FString LastRejectReason;
};

namespace CRRunGen
{
	/** Structural node ids (never tied to a room type). */
	inline FName StartNodeId() { return TEXT("Start"); }
	inline FName BossNodeId() { return TEXT("Boss"); }
	inline FName ReturnNodeId() { return TEXT("Return"); }
	/** Id of a normal-layer node, e.g. (2, 1) -> "L2_B". */
	FName MakeNodeId(int32 Layer, int32 Lane);

	/** Deterministic: the same seed and config always give the same nodes, types, edges and positions. */
	FCRRunGenerationResult Generate(int32 Seed, const FCRRunGenerationConfig& Config = FCRRunGenerationConfig());

	/** Known-safe graph with the same structural ids, used only if generation keeps failing. */
	TArray<FCRRunNodeData> BuildFallbackGraph(const FCRRunGenerationConfig& Config = FCRRunGenerationConfig());

	/**
	 * Checks every structural and design rule (ids, layers, edges, reachability, room minimums, Shop->Shop,
	 * combats per path, Boss->Return, event pools). Returns true if valid; otherwise OutErrors lists why.
	 */
	bool ValidateGraph(const TArray<FCRRunNodeData>& Nodes, const FCRRunGenerationConfig& Config, TArray<FString>& OutErrors);

	/** Every Start -> Boss path as node ids (Start and Boss included). Small graphs only. */
	TArray<TArray<FName>> EnumeratePathsToBoss(const TArray<FCRRunNodeData>& Nodes);

	/** Compact text of room types + edges, for diversity checks and logs. */
	FString GraphSignature(const TArray<FCRRunNodeData>& Nodes);

	/** Total directed edges. */
	int32 CountEdges(const TArray<FCRRunNodeData>& Nodes);
}
