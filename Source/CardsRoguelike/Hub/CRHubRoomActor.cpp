#include "CRHubRoomActor.h"

#include "../Combat/CRTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The camera looks along +Y, so screen-right is -X. The scene is offset to +X to leave the right third of
	// the screen for the details panel.
	const FVector HubCameraLocation(420.f, -1150.f, 620.f);
	const FRotator HubCameraRotation(-20.f, 90.f, 0.f);
	const float HubFloorRadius = 1250.f;
	/** Center of the bottle floor (the buildings sit around it, the ribs on its back half). */
	const FVector HubFloorCenter(650.f, 300.f, 0.f);

	const FLinearColor HubStone(0.32f, 0.3f, 0.27f);
	const FLinearColor HubDarkStone(0.16f, 0.15f, 0.14f);
	const FLinearColor HubWood(0.36f, 0.22f, 0.11f);
	const FLinearColor HubDarkWood(0.2f, 0.12f, 0.06f);
	const FLinearColor HubRoof(0.4f, 0.14f, 0.12f);
	const FLinearColor HubGlass(0.45f, 0.6f, 0.62f);
	const FLinearColor HubIron(0.2f, 0.2f, 0.22f);
}

ACRHubRoomActor::ACRHubRoomActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	SphereMesh = SphereFinder.Object;
	ConeMesh = ConeFinder.Object;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->SetRelativeLocationAndRotation(HubCameraLocation, HubCameraRotation);
	Camera->SetFieldOfView(70.f);

	// Anchors: Workshop screen-left, Heart center-back, Storage center-right, all left of the details panel
	// that covers the right third of the screen.
	Anchors.SetNum(3);
	Anchors[0].Id = HeartAnchor();
	Anchors[0].Location = FVector(760.f, 450.f, 0.f);
	Anchors[0].MarkerHeight = 430.f;
	Anchors[1].Id = WorkshopAnchor();
	Anchors[1].Location = FVector(1000.f, -60.f, 0.f);
	Anchors[1].MarkerHeight = 380.f;
	Anchors[2].Id = StorageAnchor();
	Anchors[2].Location = FVector(300.f, -60.f, 0.f);
	Anchors[2].MarkerHeight = 300.f;
}

void ACRHubRoomActor::BeginPlay()
{
	Super::BeginPlay();

	FPostProcessSettings& PostProcess = Camera->PostProcessSettings;
	PostProcess.bOverride_AutoExposureMinBrightness = true;
	PostProcess.bOverride_AutoExposureMaxBrightness = true;
	PostProcess.bOverride_AutoExposureBias = true;
	PostProcess.AutoExposureMinBrightness = CameraExposureEV100;
	PostProcess.AutoExposureMaxBrightness = CameraExposureEV100;
	PostProcess.AutoExposureBias = 0.f;
	Camera->PostProcessBlendWeight = 1.f;

	// Floor of the bottle: a wide stone disc with a darker rim.
	StaticBlocks.Add(AddBlock(CylinderMesh, HubFloorCenter + FVector(0.f, 0.f, -30.f), FVector(HubFloorRadius * 2.f + 120.f, HubFloorRadius * 2.f + 120.f, 40.f), HubDarkStone));
	StaticBlocks.Add(AddBlock(CylinderMesh, HubFloorCenter + FVector(0.f, 0.f, -10.f), FVector(HubFloorRadius * 2.f, HubFloorRadius * 2.f, 20.f), FLinearColor(0.24f, 0.25f, 0.2f)));
	// Paths from the Heart to the other buildings.
	for (int32 i = 1; i < Anchors.Num(); ++i)
	{
		const FVector From = Anchors[0].Location;
		const FVector Delta = Anchors[i].Location - From;
		StaticBlocks.Add(AddBlock(CubeMesh, From + Delta * 0.5f + FVector(0.f, 0.f, 1.f), FVector(Delta.Size2D(), 110.f, 4.f), HubStone,
			FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0.f)));
	}

	// Glass wall of the bottle: tall pale ribs on the back half-circle, bent inward at the top.
	for (int32 i = 0; i <= 12; ++i)
	{
		const float Angle = FMath::DegreesToRadians(-10.f + 200.f * i / 12.f);
		const FVector Base = HubFloorCenter + FVector(FMath::Cos(Angle) * HubFloorRadius, FMath::Sin(Angle) * HubFloorRadius, 0.f);
		StaticBlocks.Add(AddBlock(CylinderMesh, Base + FVector(0.f, 0.f, 450.f), FVector(28.f, 28.f, 900.f), HubGlass));
		const FVector Inward = HubFloorCenter - Base;
		StaticBlocks.Add(AddBlock(CylinderMesh, Base + Inward * 0.12f + FVector(0.f, 0.f, 1000.f), FVector(24.f, 24.f, 260.f), HubGlass,
			FRotator(-35.f, FMath::RadiansToDegrees(FMath::Atan2(Inward.Y, Inward.X)), 0.f)));
	}

	HeartLight = NewObject<UPointLightComponent>(this);
	HeartLight->SetupAttachment(SceneRoot);
	HeartLight->RegisterComponent();
	HeartLight->SetRelativeLocation(Anchors[0].Location + FVector(0.f, 0.f, 260.f));
	HeartLight->SetMobility(EComponentMobility::Movable);
	HeartLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.4f));
	HeartLight->SetAttenuationRadius(1400.f);
	HeartLight->SetCastShadows(false);

	for (FAnchor& Anchor : Anchors)
	{
		Anchor.Ring = AddBlock(CylinderMesh, Anchor.Location + FVector(0.f, 0.f, 3.f), FVector(560.f, 560.f, 4.f), FLinearColor::Black);
		Anchor.Ring->SetVisibility(false);
		StaticBlocks.Add(Anchor.Ring);
		SetAnchorLevel(Anchor.Id, 0);
	}
}

void ACRHubRoomActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	if (HeartCrystal)
	{
		HeartCrystal->SetRelativeLocation(HeartCrystalBase + FVector(0.f, 0.f, FMath::Sin(Time * 1.4f) * 10.f));
		HeartCrystal->AddRelativeRotation(FRotator(0.f, DeltaSeconds * 20.f, 0.f));
	}
	if (HeartLight)
	{
		HeartLight->SetIntensity(HeartLightBase * (0.9f + 0.1f * FMath::Sin(Time * 2.3f)));
	}
}

UStaticMeshComponent* ACRHubRoomActor::AddBlock(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, const FLinearColor& Color,
	const FRotator& Rotation)
{
	UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(this);
	Block->SetStaticMesh(Mesh);
	Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Block->SetupAttachment(SceneRoot);
	Block->RegisterComponent();
	Block->SetRelativeLocationAndRotation(Location, Rotation);
	Block->SetRelativeScale3D(Size / 100.f);
	CRProto::ApplyColor(Block, Color);
	return Block;
}

ACRHubRoomActor::FAnchor* ACRHubRoomActor::FindAnchor(FName AnchorId)
{
	return Anchors.FindByPredicate([AnchorId](const FAnchor& A) { return A.Id == AnchorId; });
}

const ACRHubRoomActor::FAnchor* ACRHubRoomActor::FindAnchor(FName AnchorId) const
{
	return Anchors.FindByPredicate([AnchorId](const FAnchor& A) { return A.Id == AnchorId; });
}

FVector ACRHubRoomActor::GetAnchorLocation(FName AnchorId) const
{
	const FAnchor* Anchor = FindAnchor(AnchorId);
	return Anchor ? GetActorTransform().TransformPosition(Anchor->Location + FVector(0.f, 0.f, Anchor->MarkerHeight)) : GetActorLocation();
}

void ACRHubRoomActor::SetAnchorHighlight(FName AnchorId, int32 Highlight)
{
	FAnchor* Anchor = FindAnchor(AnchorId);
	if (!Anchor || !Anchor->Ring)
	{
		return;
	}
	Anchor->Ring->SetVisibility(Highlight > 0);
	CRProto::ApplyColor(Anchor->Ring, Highlight >= 2 ? FLinearColor(1.f, 0.78f, 0.3f) : FLinearColor(0.55f, 0.55f, 0.6f));
}

void ACRHubRoomActor::SetAnchorLevel(FName AnchorId, int32 Level)
{
	FAnchor* Anchor = FindAnchor(AnchorId);
	if (!Anchor)
	{
		return;
	}
	for (UStaticMeshComponent* Piece : Anchor->Pieces)
	{
		if (Piece)
		{
			AnchorPieces.Remove(Piece);
			Piece->DestroyComponent();
		}
	}
	Anchor->Pieces.Reset();

	if (AnchorId == HeartAnchor())
	{
		BuildHeart(*Anchor, Level);
	}
	else if (AnchorId == WorkshopAnchor())
	{
		BuildWorkshop(*Anchor, Level);
	}
	else if (AnchorId == StorageAnchor())
	{
		BuildStorage(*Anchor, Level);
	}
	AnchorPieces.Append(Anchor->Pieces);
}

void ACRHubRoomActor::BuildHeart(FAnchor& Anchor, int32 Level)
{
	const FVector L = Anchor.Location;
	const int32 Lv = FMath::Max(1, Level);
	// Stepped pedestal, then the floating crystal (larger and brighter per level), then orbiting stones.
	Anchor.Pieces.Add(AddBlock(CylinderMesh, L + FVector(0.f, 0.f, 20.f), FVector(300.f, 300.f, 40.f), HubStone));
	Anchor.Pieces.Add(AddBlock(CylinderMesh, L + FVector(0.f, 0.f, 70.f), FVector(200.f, 200.f, 60.f), HubDarkStone));
	const float CrystalSize = 90.f + 30.f * Lv;
	HeartCrystalBase = L + FVector(0.f, 0.f, 150.f + CrystalSize * 0.6f);
	HeartCrystal = AddBlock(SphereMesh, HeartCrystalBase, FVector(CrystalSize * 0.8f, CrystalSize * 0.8f, CrystalSize * 1.3f),
		FLinearColor(0.9f, 0.55f + 0.1f * Lv, 0.25f + 0.1f * Lv));
	Anchor.Pieces.Add(HeartCrystal);
	for (int32 i = 0; i < Lv * 2; ++i)
	{
		const float Angle = 2.f * PI * i / (Lv * 2);
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(FMath::Cos(Angle) * 180.f, FMath::Sin(Angle) * 180.f, 30.f), FVector(40.f, 40.f, 60.f),
			HubStone, FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f)));
	}
	HeartLightBase = 2500.f + 2500.f * Lv;
}

void ACRHubRoomActor::BuildWorkshop(FAnchor& Anchor, int32 Level)
{
	const FVector L = Anchor.Location;
	if (Level <= 0)
	{
		// Not built: a marked plot with a pile of planks.
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(0.f, 0.f, 4.f), FVector(300.f, 260.f, 8.f), HubDarkWood));
		for (int32 i = 0; i < 4; ++i)
		{
			Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(-60.f + 40.f * i, 40.f, 20.f + 14.f * (i % 2)), FVector(220.f, 26.f, 14.f), HubWood, FRotator(0.f, 80.f + 5.f * i, 0.f)));
		}
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector Corner((i % 2 ? 140.f : -140.f), (i < 2 ? 120.f : -120.f), 70.f);
			Anchor.Pieces.Add(AddBlock(CylinderMesh, L + Corner, FVector(12.f, 12.f, 140.f), HubWood));
		}
		return;
	}

	// Level 1: a hut with a roof and a workbench.
	Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(0.f, 60.f, 90.f), FVector(280.f, 200.f, 180.f), HubWood));
	Anchor.Pieces.Add(AddBlock(ConeMesh, L + FVector(0.f, 60.f, 240.f), FVector(330.f, 260.f, 130.f), HubRoof, FRotator(0.f, 45.f, 0.f)));
	Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(0.f, -35.f, 70.f), FVector(70.f, 12.f, 130.f), HubDarkWood));
	Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(-60.f, -120.f, 45.f), FVector(140.f, 60.f, 12.f), HubWood));
	Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(-60.f, -120.f, 20.f), FVector(120.f, 40.f, 40.f), HubDarkWood));
	if (Level >= 2)
	{
		// Chimney and anvil.
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(90.f, 100.f, 250.f), FVector(50.f, 50.f, 200.f), HubStone));
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(90.f, -130.f, 30.f), FVector(60.f, 40.f, 60.f), HubIron));
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(90.f, -130.f, 68.f), FVector(90.f, 36.f, 16.f), HubIron));
	}
	if (Level >= 3)
	{
		// Second workroom and a banner pole.
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(-190.f, 90.f, 70.f), FVector(160.f, 150.f, 140.f), HubWood));
		Anchor.Pieces.Add(AddBlock(ConeMesh, L + FVector(-190.f, 90.f, 185.f), FVector(200.f, 190.f, 90.f), HubRoof, FRotator(0.f, 45.f, 0.f)));
		Anchor.Pieces.Add(AddBlock(CylinderMesh, L + FVector(150.f, -40.f, 160.f), FVector(10.f, 10.f, 320.f), HubDarkWood));
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(150.f, -75.f, 280.f), FVector(8.f, 70.f, 50.f), FLinearColor(0.2f, 0.35f, 0.6f)));
	}
}

void ACRHubRoomActor::BuildStorage(FAnchor& Anchor, int32 Level)
{
	const FVector L = Anchor.Location;
	const int32 Lv = FMath::Max(1, Level);
	// A platform with crates; more rows per level, then a shelf and barrels.
	Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(0.f, 0.f, 6.f), FVector(360.f, 280.f, 12.f), HubDarkWood));
	const int32 Crates = 2 + 2 * Lv;
	for (int32 i = 0; i < Crates; ++i)
	{
		const int32 Col = i % 3;
		const int32 Row = (i / 3) % 2;
		const int32 Stack = i / 6;
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(-110.f + 110.f * Col, -60.f + 110.f * Row, 57.f + 92.f * Stack), FVector(90.f, 90.f, 90.f),
			(i % 2) ? HubWood : FLinearColor(0.42f, 0.28f, 0.14f), FRotator(0.f, (i * 7) % 15 - 7.f, 0.f)));
	}
	if (Lv >= 2)
	{
		Anchor.Pieces.Add(AddBlock(CubeMesh, L + FVector(0.f, 150.f, 120.f), FVector(340.f, 40.f, 240.f), HubDarkWood));
		for (int32 i = 0; i < 3; ++i)
		{
			Anchor.Pieces.Add(AddBlock(CylinderMesh, L + FVector(-110.f + 110.f * i, 125.f, 150.f), FVector(50.f, 50.f, 50.f), FLinearColor(0.55f, 0.55f, 0.6f)));
		}
	}
	if (Lv >= 3)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			Anchor.Pieces.Add(AddBlock(CylinderMesh, L + FVector(220.f, -100.f + 90.f * i, 55.f), FVector(80.f, 80.f, 110.f), HubWood));
		}
	}
}
