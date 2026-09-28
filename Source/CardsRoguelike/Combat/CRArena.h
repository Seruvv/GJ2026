// Arena defined by two nested polygons: inner (melee) and outer (ranged) zones.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRTypes.h"
#include "CRArena.generated.h"

class ACRBoundarySegment;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CARDSROGUELIKE_API ACRArena : public AActor
{
	GENERATED_BODY()

public:
	ACRArena();

	/** Inner (melee) zone polygon, arena-relative XY. */
	UPROPERTY(EditAnywhere, Category = "CR|Arena")
	TArray<FVector2D> InnerVertices;

	/** Outer (ranged) zone polygon, arena-relative XY. Must contain the inner polygon. */
	UPROPERTY(EditAnywhere, Category = "CR|Arena")
	TArray<FVector2D> OuterVertices;

	/** Boundary type per outer edge; edge i runs from vertex i to vertex i+1. */
	UPROPERTY(EditAnywhere, Category = "CR|Arena")
	TArray<ECRBoundaryType> OuterEdgeTypes;

	/** Melee enemies stop this far from the center when walking in. */
	UPROPERTY(EditAnywhere, Category = "CR|Arena")
	float MeleeStopDistance = 200.f;

	/** Teleported enemies land at least this far from the center. */
	UPROPERTY(EditAnywhere, Category = "CR|Arena")
	float HamsterClearance = 180.f;

	UPROPERTY(EditAnywhere, Category = "CR|Arena")
	float FloorMargin = 800.f;

	// Zone queries (world XY)
	bool IsInInnerZone(const FVector2D& WorldPoint) const;
	bool IsInsideOuter(const FVector2D& WorldPoint) const;
	bool IsInOuterZone(const FVector2D& WorldPoint) const;
	ECRZone GetZone(const FVector2D& WorldPoint) const;
	FVector2D GetCenter() const;

	/** Next position for an enemy of the given type moving at most Speed toward its preferred zone. */
	FVector2D ComputeMoveTarget(ECRCombatType Type, const FVector2D& From, float Speed) const;

	int32 FindNearestOuterEdge(const FVector2D& WorldPoint) const;
	ECRBoundaryType GetEdgeType(int32 EdgeIndex) const;
	FVector2D ClampInsideEdge(const FVector2D& WorldPoint, int32 EdgeIndex, float Margin) const;
	FVector2D FindSafeInnerPoint(const TArray<FVector2D>& Occupied, float MinSeparation) const;

	int32 GetNumEdges() const { return Segments.Num(); }
	void CycleEdgeType(int32 EdgeIndex);

	static bool PointInPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Point);
	static float RayExitDistance(const TArray<FVector2D>& Polygon, const FVector2D& Origin, const FVector2D& Dir);
	static FVector2D ClosestPointOnSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void BuildFloor();
	void BuildInnerOutline();
	void BuildBoundaries();
	void AddLabel(const FString& Text, const FVector& WorldLocation, const FColor& Color, float Size);

	TArray<FVector2D> GetWorldPolygon(const TArray<FVector2D>& Local) const;

	UPROPERTY(VisibleAnywhere, Category = "CR|Arena")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "CR|Arena")
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TArray<TObjectPtr<ACRBoundarySegment>> Segments;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> Labels;

	TArray<FVector2D> InnerWorld;
	TArray<FVector2D> OuterWorld;
};
