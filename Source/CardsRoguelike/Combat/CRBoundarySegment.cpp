#include "CRBoundarySegment.h"

#include "CRCombatGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ACRBoundarySegment::ACRBoundarySegment()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(CubeFinder.Object);
	Mesh->SetMobility(EComponentMobility::Movable);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Mesh);
	Label->SetUsingAbsoluteScale(true);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(55.f);
}

void ACRBoundarySegment::Setup(int32 InEdgeIndex, const FVector2D& A, const FVector2D& B, const FVector2D& InInwardNormal, ECRBoundaryType InType)
{
	EdgeIndex = InEdgeIndex;
	InwardNormal = InInwardNormal.GetSafeNormal();
	Length = FVector2D::Distance(A, B);

	const FVector2D Dir = B - A;
	const FVector2D Mid = (A + B) * 0.5f - InwardNormal * (Thickness * 0.5f);
	SetActorLocationAndRotation(FVector(Mid, 0.f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f));

	Mesh->OnComponentBeginOverlap.AddDynamic(this, &ACRBoundarySegment::OnMeshOverlap);
	SetBoundaryType(InType);
}

void ACRBoundarySegment::SetBoundaryType(ECRBoundaryType NewType)
{
	BoundaryType = NewType;

	const bool bBlocking = NewType == ECRBoundaryType::Normal || NewType == ECRBoundaryType::Rubber;
	if (bBlocking)
	{
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetNotifyRigidBodyCollision(true);
	}
	else
	{
		Mesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
		Mesh->SetGenerateOverlapEvents(true);
	}

	ApplyVisuals();

	if (!bBlocking)
	{
		// Catch bodies already resting on the edge when the type changes.
		TArray<AActor*> Overlapping;
		Mesh->UpdateOverlaps();
		Mesh->GetOverlappingActors(Overlapping);
		if (ACRCombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRCombatGameMode>() : nullptr)
		{
			for (AActor* Actor : Overlapping)
			{
				GM->HandleBoundaryContact(Actor, BoundaryType);
			}
		}
	}
}

void ACRBoundarySegment::ApplyVisuals()
{
	const bool bBlocking = BoundaryType == ECRBoundaryType::Normal || BoundaryType == ECRBoundaryType::Rubber;
	const float Height = bBlocking ? WallHeight : StripHeight;

	Mesh->SetWorldScale3D(FVector((Length + Thickness * 2.f) / 100.f, Thickness / 100.f, Height / 100.f));
	FVector Location = GetActorLocation();
	Location.Z = Height * 0.5f;
	SetActorLocation(Location);
	CRProto::ApplyColor(Mesh, CRProto::BoundaryTypeColor(BoundaryType));
	RefreshLabel();
}

void ACRBoundarySegment::RefreshLabel()
{
	// Playtest View: the combat HUD draws the Russian edge name here (the 3D text font has no Cyrillic),
	// so this label only shows in Debug View, with the cycling key.
	Label->SetVisibility(bLabelShowsDebug);
	const FString TypeName = CRProto::BoundaryTypeName(BoundaryType);
	Label->SetText(FText::FromString(bLabelShowsDebug ? FString::Printf(TEXT("[Shift+%d] %s"), EdgeIndex + 7, *TypeName) : TypeName));
	// Lightened so dark types (VOID) stay legible against the dark floor.
	const FLinearColor LabelColor = FMath::Lerp(CRProto::BoundaryTypeColor(BoundaryType), FLinearColor::White, 0.35f);
	Label->SetTextRenderColor(LabelColor.ToFColor(true));
	Label->SetWorldLocation(FVector(FVector2D(GetActorLocation()) + InwardNormal * 110.f, WallHeight + 40.f));
}

FVector ACRBoundarySegment::GetLabelWorldLocation() const
{
	return Label ? Label->GetComponentLocation() : GetActorLocation();
}

void ACRBoundarySegment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bDebug = CRProto::IsDebugView(this);
	if (bDebug != bLabelShowsDebug)
	{
		bLabelShowsDebug = bDebug;
		RefreshLabel();
	}
	CRProto::FaceCamera(Label);
}

void ACRBoundarySegment::HandleBodyHit(UPrimitiveComponent* Body, const FVector& PreImpactVelocity)
{
	if (BoundaryType != ECRBoundaryType::Rubber || !Body || !Body->IsSimulatingPhysics())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastBounceTime.Find(Body))
	{
		if (Now - *Last < 0.15)
		{
			return;
		}
	}

	const FVector N(InwardNormal, 0.f);
	const float IntoWall = FVector::DotProduct(PreImpactVelocity, N);
	if (IntoWall > -50.f)
	{
		return;
	}

	LastBounceTime.Add(Body, Now);

	FVector Reflected = PreImpactVelocity - 2.f * IntoWall * N;
	Reflected.Z = FMath::Max(Reflected.Z, 0.f);
	Reflected *= RubberBounceFactor;
	if (Reflected.Size() < RubberMinSpeed)
	{
		Reflected = Reflected.GetSafeNormal() * RubberMinSpeed;
	}
	Body->SetPhysicsLinearVelocity(Reflected);
}

void ACRBoundarySegment::OnMeshOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>())
	{
		GM->HandleBoundaryContact(OtherActor, BoundaryType);
	}
}
