#include "CREventRoomActor.h"

#include "../Combat/CRTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The stage (illustration) sits in front of a dark backdrop. The camera looks along +Y, so screen-right
	// is -X: standing at +X puts the scene on the right of the screen, beside the narrative panel.
	const FVector EventStageCenter(0.f, 300.f, 0.f);
	const float EventStageHalfWidth = 900.f;
	// Far and level enough that the tallest scene (the door and its lintel, ~4 m) fits in frame.
	const FVector EventCameraLocation(400.f, -620.f, 260.f);
	const FRotator EventCameraRotation(-7.f, 90.f, 0.f);
}

ACREventRoomActor::ACREventRoomActor()
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
	Camera->SetRelativeLocation(EventCameraLocation);
	Camera->SetRelativeRotation(EventCameraRotation);
	Camera->SetFieldOfView(70.f);
}

void ACREventRoomActor::BeginPlay()
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
	Camera->SetRelativeLocationAndRotation(EventCameraLocation, EventCameraRotation);

	// Shared stage: ground and a dark backdrop, so every illustration reads as a lit vignette.
	AddBlock(CubeMesh, FVector(0.f, 0.f, -10.f), FVector(EventStageHalfWidth * 2.f, 1400.f, 20.f), FLinearColor(0.09f, 0.085f, 0.07f));
	AddBlock(CubeMesh, FVector(0.f, 650.f, 300.f), FVector(EventStageHalfWidth * 2.f, 30.f, 620.f), FLinearColor(0.03f, 0.03f, 0.035f));
}

UStaticMeshComponent* ACREventRoomActor::AddBlock(UStaticMesh* Mesh, const FVector& StageLocation, const FVector& Size,
	const FLinearColor& Color, const FRotator& Rotation)
{
	UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(this);
	Block->SetStaticMesh(Mesh);
	Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Block->SetupAttachment(SceneRoot);
	Block->RegisterComponent();
	Block->SetRelativeLocationAndRotation(EventStageCenter + StageLocation, Rotation);
	Block->SetRelativeScale3D(Size / 100.f);
	CRProto::ApplyColor(Block, Color);
	Blocks.Add(Block);
	return Block;
}

void ACREventRoomActor::AddGlow(const FVector& StageLocation, const FLinearColor& Color, float Intensity, float Radius, float FlickerSpeed)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(SceneRoot);
	Light->RegisterComponent();
	Light->SetRelativeLocation(EventStageCenter + StageLocation);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetLightColor(Color);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetCastShadows(false);
	Glows.Add(Light);
	GlowParams.Add(FVector(Intensity, FlickerSpeed, FMath::FRandRange(0.f, 6.28f)));
}

FTransform ACREventRoomActor::GetStageTransform() const
{
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(EventStageCenter));
}

void ACREventRoomActor::BuildPresentation(ECREventPresentation Presentation)
{
	switch (Presentation)
	{
	case ECREventPresentation::AbandonedCamp:   BuildAbandonedCamp(); break;
	case ECREventPresentation::BlackShrine:     BuildBlackShrine(); break;
	case ECREventPresentation::ThingBehindDoor: BuildThingBehindDoor(); break;
	default:
		// Neutral fallback: a lone marker stone, so the room never looks broken.
		AddBlock(CubeMesh, FVector(0.f, 0.f, 60.f), FVector(60.f, 60.f, 120.f), FLinearColor(0.25f, 0.25f, 0.27f));
		break;
	}
}

void ACREventRoomActor::BuildAbandonedCamp()
{
	const FLinearColor Ash(0.12f, 0.11f, 0.1f);
	const FLinearColor Charred(0.07f, 0.05f, 0.04f);
	const FLinearColor Stone(0.3f, 0.29f, 0.27f);
	const FLinearColor Canvas(0.42f, 0.38f, 0.28f);
	const FLinearColor Wood(0.33f, 0.2f, 0.1f);

	// Cold fire pit: ash disc, ring of stones, crossed charred logs, a last ember.
	AddBlock(CylinderMesh, FVector(0.f, 0.f, 2.f), FVector(150.f, 150.f, 4.f), Ash);
	for (int32 i = 0; i < 9; ++i)
	{
		const float Angle = i * (360.f / 9.f);
		const FVector Offset = FRotator(0.f, Angle, 0.f).Vector() * 80.f;
		AddBlock(CubeMesh, Offset + FVector(0.f, 0.f, 10.f), FVector(26.f, 20.f, 20.f), Stone, FRotator(0.f, Angle + 17.f, 0.f));
	}
	AddBlock(CylinderMesh, FVector(0.f, 0.f, 12.f), FVector(14.f, 14.f, 110.f), Charred, FRotator(90.f, 30.f, 0.f));
	AddBlock(CylinderMesh, FVector(0.f, 0.f, 16.f), FVector(12.f, 12.f, 100.f), Charred, FRotator(90.f, -40.f, 0.f));
	AddBlock(SphereMesh, FVector(5.f, 0.f, 14.f), FVector(22.f, 22.f, 10.f), FLinearColor(0.9f, 0.3f, 0.05f));
	AddGlow(FVector(0.f, 0.f, 40.f), FLinearColor(1.f, 0.45f, 0.15f), 2500.f, 420.f, 5.f);

	// Collapsed tent behind the fire.
	AddBlock(CubeMesh, FVector(-120.f, 250.f, 45.f), FVector(220.f, 130.f, 8.f), Canvas, FRotator(0.f, 10.f, 32.f));
	AddBlock(CubeMesh, FVector(-120.f, 330.f, 30.f), FVector(220.f, 110.f, 8.f), Canvas * 0.8f, FRotator(0.f, 10.f, -25.f));
	AddBlock(CylinderMesh, FVector(-230.f, 280.f, 50.f), FVector(6.f, 6.f, 100.f), Wood, FRotator(0.f, 0.f, 18.f));

	// Packs, crates and scattered supplies.
	AddBlock(CubeMesh, FVector(210.f, 120.f, 35.f), FVector(80.f, 70.f, 70.f), Wood);
	AddBlock(CubeMesh, FVector(250.f, 190.f, 25.f), FVector(60.f, 60.f, 50.f), Wood * 0.8f, FRotator(0.f, 25.f, 0.f));
	AddBlock(CubeMesh, FVector(170.f, 40.f, 12.f), FVector(50.f, 40.f, 24.f), Wood * 0.7f, FRotator(0.f, -30.f, 70.f));
	AddBlock(CylinderMesh, FVector(-200.f, 60.f, 22.f), FVector(45.f, 45.f, 44.f), FLinearColor(0.28f, 0.33f, 0.2f));
	AddBlock(CubeMesh, FVector(-170.f, -60.f, 5.f), FVector(160.f, 55.f, 10.f), FLinearColor(0.35f, 0.12f, 0.1f), FRotator(0.f, 20.f, 0.f));
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector Spot(-90.f + i * 45.f, -120.f - (i % 2) * 40.f, 4.f);
		AddBlock((i % 2) ? CylinderMesh : CubeMesh, Spot, FVector(16.f, 16.f, 8.f), FLinearColor(0.5f, 0.45f, 0.3f), FRotator(0.f, i * 40.f, 0.f));
	}
}

void ACREventRoomActor::BuildBlackShrine()
{
	const FLinearColor Obsidian(0.03f, 0.03f, 0.04f);
	const FLinearColor DarkStone(0.1f, 0.1f, 0.12f);
	const FLinearColor Wax(0.75f, 0.72f, 0.6f);
	const FLinearColor BlackGold(0.25f, 0.2f, 0.08f);

	// Stepped plinth, pedestal and a faceless idol.
	AddBlock(CubeMesh, FVector(0.f, 60.f, 15.f), FVector(300.f, 220.f, 30.f), DarkStone);
	AddBlock(CubeMesh, FVector(0.f, 70.f, 45.f), FVector(210.f, 160.f, 30.f), DarkStone * 0.8f);
	AddBlock(CylinderMesh, FVector(0.f, 80.f, 110.f), FVector(70.f, 70.f, 100.f), Obsidian);
	AddBlock(CubeMesh, FVector(0.f, 80.f, 205.f), FVector(46.f, 36.f, 90.f), Obsidian);
	AddBlock(SphereMesh, FVector(0.f, 80.f, 270.f), FVector(44.f, 40.f, 50.f), FLinearColor(0.06f, 0.05f, 0.06f));
	AddBlock(ConeMesh, FVector(0.f, 80.f, 312.f), FVector(30.f, 30.f, 40.f), Obsidian);

	// Offerings: a bowl and blackened coins at the foot of the idol.
	AddBlock(CylinderMesh, FVector(0.f, -40.f, 64.f), FVector(60.f, 60.f, 10.f), FLinearColor(0.15f, 0.12f, 0.1f));
	for (int32 i = 0; i < 7; ++i)
	{
		const FVector Spot(-70.f + i * 22.f, -60.f + (i % 3) * 12.f, 61.f + (i % 2) * 2.f);
		AddBlock(CylinderMesh, Spot, FVector(12.f, 12.f, 2.f), BlackGold);
	}

	// Candles on both sides, each with a small pale flame light.
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector Base(Side * (95.f + i * 40.f), 20.f - i * 50.f, 60.f);
			const float Height = 30.f + i * 18.f;
			AddBlock(CylinderMesh, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(9.f, 9.f, Height), Wax);
			AddBlock(SphereMesh, Base + FVector(0.f, 0.f, Height + 5.f), FVector(5.f, 5.f, 9.f), FLinearColor(1.f, 0.85f, 0.5f));
			AddGlow(Base + FVector(0.f, 0.f, Height + 20.f), FLinearColor(1.f, 0.75f, 0.45f), 700.f, 260.f, 7.f + i);
		}
	}
	// A faint red glow from beneath the stone ("something is breathing").
	AddGlow(FVector(0.f, 80.f, 8.f), FLinearColor(0.8f, 0.05f, 0.05f), 900.f, 320.f, 0.8f);

	// Leaning standing stones framing the shrine.
	AddBlock(CubeMesh, FVector(-280.f, 220.f, 110.f), FVector(60.f, 40.f, 220.f), DarkStone, FRotator(0.f, 10.f, 8.f));
	AddBlock(CubeMesh, FVector(290.f, 240.f, 90.f), FVector(55.f, 40.f, 180.f), DarkStone, FRotator(0.f, -12.f, -10.f));
}

void ACREventRoomActor::BuildThingBehindDoor()
{
	const FLinearColor OldWood(0.28f, 0.17f, 0.09f);
	const FLinearColor FrameStone(0.32f, 0.31f, 0.3f);
	const FLinearColor Iron(0.12f, 0.12f, 0.13f);
	const FLinearColor Road(0.16f, 0.14f, 0.11f);

	// A strip of road the door stands beside.
	AddBlock(CubeMesh, FVector(0.f, -130.f, 1.f), FVector(1400.f, 150.f, 2.f), Road);

	// Heavy stone frame with a lintel, and a threshold slab. No wall: the door stands alone.
	AddBlock(CubeMesh, FVector(0.f, 60.f, 6.f), FVector(240.f, 90.f, 12.f), FrameStone * 0.8f);
	AddBlock(CubeMesh, FVector(-100.f, 60.f, 170.f), FVector(40.f, 60.f, 340.f), FrameStone);
	AddBlock(CubeMesh, FVector(100.f, 60.f, 170.f), FVector(40.f, 60.f, 340.f), FrameStone);
	AddBlock(CubeMesh, FVector(0.f, 60.f, 355.f), FVector(260.f, 70.f, 40.f), FrameStone);
	AddBlock(CubeMesh, FVector(0.f, 60.f, 390.f), FVector(160.f, 50.f, 30.f), FrameStone * 0.9f);

	// The old door: planks, iron bands and a ring handle.
	AddBlock(CubeMesh, FVector(0.f, 60.f, 176.f), FVector(160.f, 16.f, 320.f), OldWood);
	for (int32 i = 0; i < 3; ++i)
	{
		AddBlock(CubeMesh, FVector(0.f, 50.f, 70.f + i * 105.f), FVector(164.f, 6.f, 12.f), Iron);
	}
	AddBlock(CylinderMesh, FVector(55.f, 48.f, 165.f), FVector(22.f, 22.f, 4.f), Iron, FRotator(90.f, 0.f, 0.f));

	// Something on the other side: a thin cold light leaking under the door, and a glow behind it.
	AddBlock(CubeMesh, FVector(0.f, 54.f, 14.f), FVector(150.f, 4.f, 4.f), FLinearColor(0.75f, 0.7f, 1.f));
	AddGlow(FVector(0.f, 40.f, 20.f), FLinearColor(0.55f, 0.45f, 1.f), 1200.f, 260.f, 2.5f);
	AddGlow(FVector(0.f, 160.f, 180.f), FLinearColor(0.4f, 0.3f, 0.9f), 1800.f, 380.f, 0.6f);

	// Dead grass tufts and a signpost with nothing written on it.
	for (int32 i = 0; i < 5; ++i)
	{
		const FVector Spot(-260.f + i * 130.f, -30.f - (i % 2) * 30.f, 12.f);
		AddBlock(ConeMesh, Spot, FVector(18.f, 18.f, 26.f), FLinearColor(0.25f, 0.23f, 0.12f));
	}
	AddBlock(CylinderMesh, FVector(-260.f, 130.f, 80.f), FVector(10.f, 10.f, 160.f), OldWood * 0.7f);
	AddBlock(CubeMesh, FVector(-240.f, 130.f, 140.f), FVector(80.f, 6.f, 26.f), OldWood * 0.8f, FRotator(0.f, 0.f, -6.f));
}

void ACREventRoomActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Gentle flicker: embers and candles breathe, the door's glow pulses slowly.
	Time += DeltaSeconds;
	for (int32 i = 0; i < Glows.Num(); ++i)
	{
		if (Glows[i])
		{
			const FVector& P = GlowParams[i];
			const float Flicker = 0.85f + 0.1f * FMath::Sin(Time * P.Y + P.Z) + 0.05f * FMath::Sin(Time * P.Y * 2.3f + P.Z * 1.7f);
			Glows[i]->SetIntensity(P.X * Flicker);
		}
	}
}
