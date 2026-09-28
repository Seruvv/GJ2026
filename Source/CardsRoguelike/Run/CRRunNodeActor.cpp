#include "CRRunNodeActor.h"

#include "../Combat/CRTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACRRunNodeActor::ACRRunNodeActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// Pedestal: 200 cm wide, 40 cm tall, top surface at the actor origin. Visual only: it lifts on hover.
	Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));
	Pedestal->SetupAttachment(SceneRoot);
	Pedestal->SetStaticMesh(CylinderFinder.Object);
	Pedestal->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Mouse picking uses this hidden copy of the resting pedestal. It never moves, so the hover lift
	// cannot pull the pick shape out from under the cursor (which made edge hover flicker on and off).
	HitArea = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HitArea"));
	HitArea->SetupAttachment(SceneRoot);
	HitArea->SetStaticMesh(CylinderFinder.Object);
	HitArea->SetCollisionProfileName(TEXT("BlockAll"));
	HitArea->SetHiddenInGame(true);
	HitArea->SetCastShadow(false);

	// Halo: flat disc under the pedestal rim, shown for Available / Current / hover.
	Halo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Halo"));
	Halo->SetupAttachment(SceneRoot);
	Halo->SetStaticMesh(CylinderFinder.Object);
	Halo->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(SceneRoot);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(55.f);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
}

void ACRRunNodeActor::InitNode(FName InNodeId, ECRRoomType InRoomType)
{
	NodeId = InNodeId;
	RoomType = InRoomType;
	PedestalScale = RoomType == ECRRoomType::Boss ? 1.4f : 1.f;

	Pedestal->SetRelativeScale3D(FVector(2.f * PedestalScale, 2.f * PedestalScale, 0.4f));
	Pedestal->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	HitArea->SetRelativeScale3D(Pedestal->GetRelativeScale3D());
	HitArea->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	Halo->SetRelativeScale3D(FVector(2.6f * PedestalScale, 2.6f * PedestalScale, 0.04f));
	Halo->SetRelativeLocation(FVector(0.f, 0.f, -36.f));
	SetVisualState(ECRRunNodeState::Locked, false);
}

void ACRRunNodeActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CRProto::FaceCamera(Label);
}

void ACRRunNodeActor::SetVisualState(ECRRunNodeState InState, bool bHovered)
{
	const FLinearColor Base = CRRun::RoomTypeColor(RoomType);
	FString Text = CRRun::RoomTypeName(RoomType);
	FLinearColor PedestalColor = Base;
	FLinearColor HaloColor = FLinearColor::Black;
	FColor LabelColor = FColor::White;
	bool bShowHalo = false;
	float Lift = 0.f;

	switch (InState)
	{
	case ECRRunNodeState::Locked:
		PedestalColor = FMath::Lerp(Base, FLinearColor(0.1f, 0.1f, 0.1f), 0.6f) * 0.35f;
		LabelColor = FColor(110, 110, 110);
		break;
	case ECRRunNodeState::Available:
		bShowHalo = true;
		HaloColor = bHovered ? FLinearColor::White : FLinearColor(1.f, 0.8f, 0.15f);
		LabelColor = bHovered ? FColor::White : FColor(255, 225, 120);
		Lift = bHovered ? 25.f : 0.f;
		break;
	case ECRRunNodeState::Current:
		bShowHalo = true;
		HaloColor = FLinearColor(0.1f, 0.9f, 1.f);
		LabelColor = FColor(120, 240, 255);
		break;
	case ECRRunNodeState::Completed:
		PedestalColor = FMath::Lerp(Base, FLinearColor(0.35f, 0.35f, 0.35f), 0.7f) * 0.5f;
		LabelColor = FColor(150, 150, 150);
		Text += TEXT("  (done)");
		break;
	}

	SetComponentColor(Pedestal, PedestalColor);
	SetComponentColor(Halo, HaloColor);
	Halo->SetVisibility(bShowHalo);
	Pedestal->SetRelativeLocation(FVector(0.f, 0.f, -20.f + Lift));
	Label->SetRelativeLocation(FVector(0.f, 0.f, 110.f + Lift));
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(LabelColor);
}

void ACRRunNodeActor::SetComponentColor(UStaticMeshComponent* Component, const FLinearColor& Color)
{
	// Reuse the component's dynamic material once created; hover changes happen every few frames.
	if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
	else
	{
		CRProto::ApplyColor(Component, Color);
	}
}
