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
	// The 3D text is lit by the dim map lighting and hard to read; it only marks the label anchor now.
	// The run map HUD draws the label flat, bright and with a shadow at this position.
	Label->SetHiddenInGame(true);
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
	FString Text = CRRun::RoomTypeDisplayName(RoomType);
	FLinearColor PedestalColor = Base;
	FLinearColor HaloColor = FLinearColor::Black;
	FColor StateLabelColor = FColor::White;
	bool bShowHalo = false;
	float Lift = 0.f;

	// Label colors stay bright in every state so they read on the dark map;
	// Locked/Completed are still a step dimmer than Available/Current.
	switch (InState)
	{
	case ECRRunNodeState::Locked:
		PedestalColor = FMath::Lerp(Base, FLinearColor(0.1f, 0.1f, 0.1f), 0.6f) * 0.35f;
		StateLabelColor = FColor(190, 190, 198);
		break;
	case ECRRunNodeState::Available:
		bShowHalo = true;
		HaloColor = bHovered ? FLinearColor::White : FLinearColor(1.f, 0.8f, 0.15f);
		StateLabelColor = bHovered ? FColor::White : FColor(255, 236, 150);
		Lift = bHovered ? 25.f : 0.f;
		break;
	case ECRRunNodeState::Current:
		bShowHalo = true;
		HaloColor = FLinearColor(0.1f, 0.9f, 1.f);
		StateLabelColor = FColor(160, 246, 255);
		break;
	case ECRRunNodeState::Completed:
		PedestalColor = FMath::Lerp(Base, FLinearColor(0.35f, 0.35f, 0.35f), 0.7f) * 0.5f;
		StateLabelColor = FColor(205, 205, 210);
		// Short completed mark: "(пройдено)" would collide with neighbouring labels (e.g. БОСС / ВОЗВРАЩЕНИЕ).
		Text += TEXT(" ✓");
		break;
	}

	SetComponentColor(Pedestal, PedestalColor);
	SetComponentColor(Halo, HaloColor);
	Halo->SetVisibility(bShowHalo);
	Pedestal->SetRelativeLocation(FVector(0.f, 0.f, -20.f + Lift));
	Label->SetRelativeLocation(FVector(0.f, 0.f, 110.f + Lift));
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(StateLabelColor);
	LabelText = Text;
	LabelColor = StateLabelColor.ReinterpretAsLinear();
}

FVector ACRRunNodeActor::GetLabelWorldLocation() const
{
	return Label ? Label->GetComponentLocation() : GetActorLocation();
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
