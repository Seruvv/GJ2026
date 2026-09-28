#include "CRShopRoomActor.h"

#include "../Combat/CRTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Room footprint (cm). The camera looks along +Y at the back wall; the front stays open.
	const float RoomHalfWidth = 650.f;
	const float RoomDepth = 900.f;
	const float WallHeight = 420.f;
	const float CounterY = 200.f;
}

ACRShopRoomActor::ACRShopRoomActor()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->SetRelativeLocation(FVector(0.f, -820.f, 330.f));
	Camera->SetRelativeRotation(FRotator(-14.f, 90.f, 0.f));
	Camera->SetFieldOfView(70.f);
}

UStaticMeshComponent* ACRShopRoomActor::AddBlock(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, const FLinearColor& Color, bool bCollision)
{
	UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(this);
	Block->SetStaticMesh(Mesh);
	Block->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Block->SetupAttachment(SceneRoot);
	Block->RegisterComponent();
	Block->SetRelativeLocation(Location);
	Block->SetRelativeScale3D(Size / 100.f);
	CRProto::ApplyColor(Block, Color);
	Blocks.Add(Block);
	return Block;
}

void ACRShopRoomActor::BeginPlay()
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

	const FLinearColor FloorColor(0.2f, 0.13f, 0.07f);
	const FLinearColor WallColor(0.26f, 0.27f, 0.3f);
	const FLinearColor WoodColor(0.36f, 0.22f, 0.1f);
	const FLinearColor DarkWood(0.18f, 0.11f, 0.06f);

	// Shell: floor, back wall and side walls. No ceiling so the level's sun lights the room.
	AddBlock(CubeMesh, FVector(0.f, RoomDepth * 0.5f - 300.f, -10.f), FVector(RoomHalfWidth * 2.f, RoomDepth + 600.f, 20.f), FloorColor, true);
	AddBlock(CubeMesh, FVector(0.f, RoomDepth - 300.f, WallHeight * 0.5f), FVector(RoomHalfWidth * 2.f, 30.f, WallHeight), WallColor);
	AddBlock(CubeMesh, FVector(-RoomHalfWidth, RoomDepth * 0.5f - 300.f, WallHeight * 0.5f), FVector(30.f, RoomDepth, WallHeight), WallColor);
	AddBlock(CubeMesh, FVector(RoomHalfWidth, RoomDepth * 0.5f - 300.f, WallHeight * 0.5f), FVector(30.f, RoomDepth, WallHeight), WallColor);

	// Merchant counter with a darker top.
	AddBlock(CubeMesh, FVector(0.f, CounterY, 50.f), FVector(520.f, 90.f, 100.f), WoodColor, true);
	AddBlock(CubeMesh, FVector(0.f, CounterY, 104.f), FVector(560.f, 110.f, 8.f), DarkWood);

	// Back shelves with a few blocky goods.
	for (int32 Shelf = 0; Shelf < 3; ++Shelf)
	{
		const float Z = 90.f + Shelf * 90.f;
		AddBlock(CubeMesh, FVector(0.f, RoomDepth - 345.f, Z), FVector(700.f, 50.f, 8.f), DarkWood);
		for (int32 Item = 0; Item < 5; ++Item)
		{
			const float X = -280.f + Item * 140.f + (Shelf % 2) * 40.f;
			const FLinearColor Goods = (Item + Shelf) % 3 == 0 ? FLinearColor(0.5f, 0.12f, 0.1f)
				: ((Item + Shelf) % 3 == 1 ? FLinearColor(0.15f, 0.3f, 0.45f) : FLinearColor(0.55f, 0.45f, 0.15f));
			AddBlock((Item % 2) ? CylinderMesh : CubeMesh, FVector(X, RoomDepth - 345.f, Z + 30.f), FVector(40.f, 40.f, 50.f), Goods);
		}
	}

	// Crates and a barrel on the sides.
	AddBlock(CubeMesh, FVector(-470.f, 120.f, 45.f), FVector(90.f, 90.f, 90.f), WoodColor, true);
	AddBlock(CubeMesh, FVector(-470.f, 120.f, 125.f), FVector(70.f, 70.f, 70.f), DarkWood, true);
	AddBlock(CubeMesh, FVector(-380.f, 20.f, 35.f), FVector(70.f, 70.f, 70.f), WoodColor, true);
	AddBlock(CylinderMesh, FVector(470.f, 100.f, 55.f), FVector(80.f, 80.f, 110.f), FLinearColor(0.3f, 0.18f, 0.08f), true);
	AddBlock(CubeMesh, FVector(430.f, -20.f, 30.f), FVector(110.f, 60.f, 60.f), DarkWood, true);
}

FVector ACRShopRoomActor::GetMerchantLocation() const
{
	return GetActorTransform().TransformPosition(FVector(0.f, CounterY + 120.f, 0.f));
}
