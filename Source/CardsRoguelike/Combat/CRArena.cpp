#include "CRArena.h"

#include "CRBoundarySegment.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ACRArena::ACRArena()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = CubeFinder.Object;

	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(SceneRoot);
	Floor->SetStaticMesh(CubeMesh);
	Floor->SetCollisionProfileName(TEXT("BlockAll"));

	// Prototype layout: square inside an equilateral triangle (circumradius 1600).
	InnerVertices = {
		FVector2D(-350.f, -350.f), FVector2D(350.f, -350.f),
		FVector2D(350.f, 350.f), FVector2D(-350.f, 350.f)
	};
	OuterVertices = {
		FVector2D(0.f, 1600.f), FVector2D(-1385.6f, -800.f), FVector2D(1385.6f, -800.f)
	};
	// Edge 0: left side, edge 1: bottom, edge 2: right side.
	OuterEdgeTypes = { ECRBoundaryType::Normal, ECRBoundaryType::Rubber, ECRBoundaryType::Void };
}

void ACRArena::BeginPlay()
{
	Super::BeginPlay();

	InnerWorld = GetWorldPolygon(InnerVertices);
	OuterWorld = GetWorldPolygon(OuterVertices);

	BuildFloor();
	BuildInnerOutline();
	BuildBoundaries();

	const FVector C(GetCenter(), 0.f);
	AddLabel(TEXT("INNER  -  MELEE"), C + FVector(0.f, -470.f, 20.f), FColor(255, 200, 40), 60.f);
	AddLabel(TEXT("OUTER  -  RANGED"), C + FVector(0.f, 1000.f, 20.f), FColor(200, 200, 255), 70.f);
}

void ACRArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Zone labels are a Debug View aid; in Playtest View the fill and outline carry the zones.
	const bool bDebug = CRProto::IsDebugView(this);
	for (UTextRenderComponent* Label : Labels)
	{
		Label->SetVisibility(bDebug);
		if (bDebug)
		{
			CRProto::FaceCamera(Label);
		}
	}
}

TArray<FVector2D> ACRArena::GetWorldPolygon(const TArray<FVector2D>& Local) const
{
	const FVector2D Offset(GetActorLocation());
	TArray<FVector2D> Result;
	Result.Reserve(Local.Num());
	for (const FVector2D& V : Local)
	{
		Result.Add(V + Offset);
	}
	return Result;
}

void ACRArena::BuildFloor()
{
	FBox2D Bounds(ForceInit);
	for (const FVector2D& V : OuterVertices)
	{
		Bounds += V;
	}
	Bounds = Bounds.ExpandBy(FloorMargin);

	const FVector2D Size = Bounds.GetSize();
	const FVector2D Mid = Bounds.GetCenter();
	Floor->SetRelativeLocation(FVector(Mid.X, Mid.Y, -50.f));
	Floor->SetRelativeScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, 1.f));
	CRProto::ApplyColor(Floor, FLinearColor(0.06f, 0.06f, 0.07f));
}

void ACRArena::BuildInnerOutline()
{
	const int32 Num = InnerWorld.Num();

	// Subtle fill: the polygon is sliced into horizontal bands, each band a thin slab spanning
	// the polygon's chords at that height. Works for any simple polygon without custom meshes.
	FBox2D Bounds(ForceInit);
	for (const FVector2D& V : InnerWorld)
	{
		Bounds += V;
	}
	const float BandHeight = 50.f;
	for (float Y = Bounds.Min.Y + BandHeight * 0.5f; Y < Bounds.Max.Y; Y += BandHeight)
	{
		TArray<float> Crossings;
		for (int32 i = 0; i < Num; ++i)
		{
			const FVector2D A = InnerWorld[i];
			const FVector2D B = InnerWorld[(i + 1) % Num];
			if ((A.Y > Y) != (B.Y > Y))
			{
				Crossings.Add(A.X + (Y - A.Y) / (B.Y - A.Y) * (B.X - A.X));
			}
		}
		Crossings.Sort();
		for (int32 c = 0; c + 1 < Crossings.Num(); c += 2)
		{
			UStaticMeshComponent* Band = NewObject<UStaticMeshComponent>(this);
			Band->SetStaticMesh(CubeMesh);
			Band->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Band->SetupAttachment(SceneRoot);
			Band->RegisterComponent();
			Band->SetWorldLocation(FVector((Crossings[c] + Crossings[c + 1]) * 0.5f, Y, 0.5f));
			Band->SetWorldScale3D(FVector((Crossings[c + 1] - Crossings[c]) / 100.f, BandHeight / 100.f, 0.01f));
			CRProto::ApplyColor(Band, FLinearColor(0.16f, 0.12f, 0.05f));
		}
	}

	for (int32 i = 0; i < Num; ++i)
	{
		const FVector2D A = InnerWorld[i];
		const FVector2D B = InnerWorld[(i + 1) % Num];
		const FVector2D Dir = B - A;
		const float Len = Dir.Size();

		UStaticMeshComponent* Strip = NewObject<UStaticMeshComponent>(this);
		Strip->SetStaticMesh(CubeMesh);
		Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Strip->SetupAttachment(SceneRoot);
		Strip->RegisterComponent();
		Strip->SetWorldLocationAndRotation(
			FVector((A + B) * 0.5f, 2.f),
			FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f));
		Strip->SetWorldScale3D(FVector((Len + 24.f) / 100.f, 0.24f, 0.04f));
		CRProto::ApplyColor(Strip, FLinearColor(0.9f, 0.65f, 0.05f));
	}
}

void ACRArena::BuildBoundaries()
{
	const int32 Num = OuterWorld.Num();

	// Signed area tells us the winding so inward normals point into the arena.
	float Area2 = 0.f;
	for (int32 i = 0; i < Num; ++i)
	{
		const FVector2D& A = OuterWorld[i];
		const FVector2D& B = OuterWorld[(i + 1) % Num];
		Area2 += A.X * B.Y - B.X * A.Y;
	}
	const bool bCCW = Area2 > 0.f;

	for (int32 i = 0; i < Num; ++i)
	{
		const FVector2D A = OuterWorld[i];
		const FVector2D B = OuterWorld[(i + 1) % Num];
		const FVector2D Dir = (B - A).GetSafeNormal();
		const FVector2D Inward = bCCW ? FVector2D(-Dir.Y, Dir.X) : FVector2D(Dir.Y, -Dir.X);
		const ECRBoundaryType Type = OuterEdgeTypes.IsValidIndex(i) ? OuterEdgeTypes[i] : ECRBoundaryType::Normal;

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACRBoundarySegment* Segment = GetWorld()->SpawnActor<ACRBoundarySegment>(ACRBoundarySegment::StaticClass(), FTransform::Identity, Params);
		if (Segment)
		{
			Segment->Setup(i, A, B, Inward, Type);
			Segments.Add(Segment);
		}
	}
}

void ACRArena::AddLabel(const FString& Text, const FVector& WorldLocation, const FColor& Color, float Size)
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(SceneRoot);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(Size);
	Label->SetTextRenderColor(Color);
	Label->SetText(FText::FromString(Text));
	Label->RegisterComponent();
	Label->SetWorldLocation(WorldLocation);
	Labels.Add(Label);
}

bool ACRArena::IsInInnerZone(const FVector2D& WorldPoint) const
{
	return PointInPolygon(InnerWorld, WorldPoint);
}

bool ACRArena::IsInsideOuter(const FVector2D& WorldPoint) const
{
	return PointInPolygon(OuterWorld, WorldPoint);
}

bool ACRArena::IsInOuterZone(const FVector2D& WorldPoint) const
{
	return IsInsideOuter(WorldPoint) && !IsInInnerZone(WorldPoint);
}

ECRZone ACRArena::GetZone(const FVector2D& WorldPoint) const
{
	if (IsInInnerZone(WorldPoint))
	{
		return ECRZone::Inner;
	}
	return IsInsideOuter(WorldPoint) ? ECRZone::Outer : ECRZone::Outside;
}

FVector2D ACRArena::GetCenter() const
{
	FVector2D Sum = FVector2D::ZeroVector;
	for (const FVector2D& V : InnerWorld)
	{
		Sum += V;
	}
	return InnerWorld.Num() > 0 ? Sum / InnerWorld.Num() : FVector2D(GetActorLocation());
}

FVector2D ACRArena::ComputeMoveTarget(ECRCombatType Type, const FVector2D& From, float Speed) const
{
	const FVector2D C = GetCenter();
	FVector2D Dir = From - C;
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(1.f, 0.f);
	}
	Dir.Normalize();

	const float InnerExit = RayExitDistance(InnerWorld, C, Dir);
	FVector2D Goal;
	if (Type == ECRCombatType::Melee)
	{
		Goal = C + Dir * FMath::Min(MeleeStopDistance, InnerExit * 0.7f);
	}
	else
	{
		// Midway through the outer band along the same ray.
		const float OuterExit = RayExitDistance(OuterWorld, C, Dir);
		Goal = C + Dir * ((InnerExit + OuterExit) * 0.5f);
	}

	FVector2D Step = Goal - From;
	if (Step.Size() > Speed)
	{
		Step = Step.GetSafeNormal() * Speed;
	}
	return From + Step;
}

int32 ACRArena::FindNearestOuterEdge(const FVector2D& WorldPoint) const
{
	int32 Best = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();
	const int32 Num = OuterWorld.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		const FVector2D Closest = ClosestPointOnSegment(WorldPoint, OuterWorld[i], OuterWorld[(i + 1) % Num]);
		const float DistSq = FVector2D::DistSquared(Closest, WorldPoint);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = i;
		}
	}
	return Best;
}

ECRBoundaryType ACRArena::GetEdgeType(int32 EdgeIndex) const
{
	return Segments.IsValidIndex(EdgeIndex) && Segments[EdgeIndex] ? Segments[EdgeIndex]->GetBoundaryType() : ECRBoundaryType::Normal;
}

FVector2D ACRArena::ClampInsideEdge(const FVector2D& WorldPoint, int32 EdgeIndex, float Margin) const
{
	if (!Segments.IsValidIndex(EdgeIndex) || !Segments[EdgeIndex])
	{
		return GetCenter();
	}
	const int32 Num = OuterWorld.Num();
	const FVector2D Closest = ClosestPointOnSegment(WorldPoint, OuterWorld[EdgeIndex], OuterWorld[(EdgeIndex + 1) % Num]);
	return Closest + Segments[EdgeIndex]->GetInwardNormal() * Margin;
}

FVector2D ACRArena::FindSafeInnerPoint(const TArray<FVector2D>& Occupied, float MinSeparation) const
{
	const FVector2D C = GetCenter();
	FBox2D Bounds(ForceInit);
	for (const FVector2D& V : InnerWorld)
	{
		Bounds += V;
	}

	for (int32 Attempt = 0; Attempt < 40; ++Attempt)
	{
		const FVector2D P(FMath::FRandRange(Bounds.Min.X, Bounds.Max.X), FMath::FRandRange(Bounds.Min.Y, Bounds.Max.Y));
		if (!IsInInnerZone(P) || FVector2D::Distance(P, C) < HamsterClearance)
		{
			continue;
		}

		bool bFree = true;
		for (const FVector2D& O : Occupied)
		{
			if (FVector2D::Distance(P, O) < MinSeparation)
			{
				bFree = false;
				break;
			}
		}
		if (bFree)
		{
			return P;
		}
	}

	const FVector2D Dir = FVector2D(1.f, 0.f).GetRotated(FMath::FRandRange(0.f, 360.f));
	return C + Dir * RayExitDistance(InnerWorld, C, Dir) * 0.6f;
}

void ACRArena::CycleEdgeType(int32 EdgeIndex)
{
	if (Segments.IsValidIndex(EdgeIndex) && Segments[EdgeIndex])
	{
		const uint8 Next = (static_cast<uint8>(Segments[EdgeIndex]->GetBoundaryType()) + 1) % 4;
		Segments[EdgeIndex]->SetBoundaryType(static_cast<ECRBoundaryType>(Next));
	}
}

bool ACRArena::PointInPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Point)
{
	bool bInside = false;
	const int32 Num = Polygon.Num();
	for (int32 i = 0, j = Num - 1; i < Num; j = i++)
	{
		const FVector2D& A = Polygon[i];
		const FVector2D& B = Polygon[j];
		if (((A.Y > Point.Y) != (B.Y > Point.Y)) &&
			(Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X))
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

float ACRArena::RayExitDistance(const TArray<FVector2D>& Polygon, const FVector2D& Origin, const FVector2D& Dir)
{
	float Best = TNumericLimits<float>::Max();
	const int32 Num = Polygon.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		const FVector2D A = Polygon[i];
		const FVector2D E = Polygon[(i + 1) % Num] - A;
		const float Denom = FVector2D::CrossProduct(Dir, E);
		if (FMath::IsNearlyZero(Denom))
		{
			continue;
		}
		const FVector2D AO = A - Origin;
		const float T = FVector2D::CrossProduct(AO, E) / Denom;
		const float U = FVector2D::CrossProduct(AO, Dir) / Denom;
		if (T >= 0.f && U >= 0.f && U <= 1.f)
		{
			Best = FMath::Min(Best, T);
		}
	}
	return Best == TNumericLimits<float>::Max() ? 0.f : Best;
}

FVector2D ACRArena::ClosestPointOnSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
	const FVector2D AB = B - A;
	const float LenSq = AB.SizeSquared();
	if (LenSq <= KINDA_SMALL_NUMBER)
	{
		return A;
	}
	const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / LenSq, 0.f, 1.f);
	return A + AB * T;
}
