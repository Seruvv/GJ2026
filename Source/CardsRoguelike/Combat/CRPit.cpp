#include "CRPit.h"

#include "CRBarrel.h"
#include "CREnemy.h"
#include "CRTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ACRPit::ACRPit()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Trigger->SetGenerateOverlapEvents(true);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Trigger);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(CubeFinder.Object);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetUsingAbsoluteScale(true);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Trigger);
	Label->SetUsingAbsoluteScale(true);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(60.f);
	Label->SetTextRenderColor(FColor(255, 80, 80));
	Label->SetText(FText::FromString(TEXT("PIT")));
	Label->SetWorldSize(45.f);
	// The combat HUD draws the Russian "ЯМА" label at this spot (the 3D text font has no Cyrillic).
	Label->SetVisibility(false);

	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* Strip = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Rim%d"), i));
		Strip->SetupAttachment(Trigger);
		Strip->SetStaticMesh(CubeFinder.Object);
		Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Strip->SetUsingAbsoluteScale(true);
		Rim.Add(Strip);
	}
}

void ACRPit::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACRPit::OnTriggerOverlap);
	CRProto::ApplyColor(Visual, FLinearColor(0.005f, 0.005f, 0.005f));
	for (UStaticMeshComponent* Strip : Rim)
	{
		CRProto::ApplyColor(Strip, FLinearColor(0.75f, 0.06f, 0.04f));
	}
	SetPitSize(Size);
}

void ACRPit::SetPitSize(const FVector2D& InSize)
{
	Size = InSize;
	const FVector Base = GetActorLocation();
	// Trigger spans the height of a standing enemy; the visual is a flat black slab on the floor.
	Trigger->SetBoxExtent(FVector(FMath::Max(Size.X * 0.5f - TriggerInset, 10.f), FMath::Max(Size.Y * 0.5f - TriggerInset, 10.f), 80.f));
	Trigger->SetWorldLocation(FVector(Base.X, Base.Y, 80.f));
	Visual->SetWorldLocation(FVector(Base.X, Base.Y, 1.f));
	Visual->SetWorldScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, 0.02f));
	Label->SetWorldLocation(FVector(Base.X, Base.Y, 60.f));

	// Rim strips: +X, -X, +Y, -Y sides.
	const float RimWidth = 16.f;
	for (int32 i = 0; i < Rim.Num(); ++i)
	{
		const bool bAlongY = i < 2;
		const float Sign = (i % 2 == 0) ? 1.f : -1.f;
		const FVector Offset = bAlongY ? FVector(Sign * (Size.X + RimWidth) * 0.5f, 0.f, 0.f) : FVector(0.f, Sign * (Size.Y + RimWidth) * 0.5f, 0.f);
		const FVector Scale = bAlongY
			? FVector(RimWidth / 100.f, (Size.Y + RimWidth * 2.f) / 100.f, 0.04f)
			: FVector((Size.X + RimWidth * 2.f) / 100.f, RimWidth / 100.f, 0.04f);
		Rim[i]->SetWorldLocation(FVector(Base.X, Base.Y, 2.f) + Offset);
		Rim[i]->SetWorldScale3D(Scale);
	}
}

FVector ACRPit::GetLabelWorldLocation() const
{
	return Label ? Label->GetComponentLocation() : GetActorLocation();
}

void ACRPit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CRProto::FaceCamera(Label);
}

void ACRPit::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACREnemy* Enemy = Cast<ACREnemy>(OtherActor))
	{
		Enemy->Eliminate(ECREliminationReason::Pit);
	}
	else if (ACRBarrel* Barrel = Cast<ACRBarrel>(OtherActor))
	{
		Barrel->RemoveSilently();
	}
}
