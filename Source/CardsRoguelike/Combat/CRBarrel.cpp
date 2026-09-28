#include "CRBarrel.h"

#include "CRArena.h"
#include "CRBoundarySegment.h"
#include "CRCombatGameMode.h"
#include "CREnemy.h"
#include "CRTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACRBarrel::ACRBarrel()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Body->SetStaticMesh(CylinderFinder.Object);
	Body->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.0f));
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetGenerateOverlapEvents(true);
	Body->BodyInstance.bUseCCD = true;
	Body->BodyInstance.DOFMode = EDOFMode::SixDOF;
	Body->BodyInstance.bLockXRotation = true;
	Body->BodyInstance.bLockYRotation = true;
	Body->SetLinearDamping(0.8f);
	Body->SetAngularDamping(4.f);

	for (int32 i = 0; i < 2; ++i)
	{
		UStaticMeshComponent* Hoop = CreateDefaultSubobject<UStaticMeshComponent>(i == 0 ? TEXT("HoopTop") : TEXT("HoopBottom"));
		Hoop->SetupAttachment(Body);
		Hoop->SetStaticMesh(CylinderFinder.Object);
		Hoop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Relative to the scaled body: slightly wider, thin, at +/- 25 cm.
		Hoop->SetRelativeScale3D(FVector(1.08f, 1.08f, 0.08f));
		Hoop->SetRelativeLocation(FVector(0.f, 0.f, i == 0 ? 25.f : -25.f));
		Hoops.Add(Hoop);
	}
}

void ACRBarrel::BeginPlay()
{
	Super::BeginPlay();

	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	Body->OnComponentHit.AddDynamic(this, &ACRBarrel::OnBodyHit);
	CRProto::ApplyColor(Body, FLinearColor(1.f, 0.45f, 0.f));
	for (UStaticMeshComponent* Hoop : Hoops)
	{
		CRProto::ApplyColor(Hoop, FLinearColor(0.05f, 0.04f, 0.03f));
	}
}

void ACRBarrel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PrevVelocity = Body->GetPhysicsLinearVelocity();
}

void ACRBarrel::ReceiveDamage(int32 Amount)
{
	if (IsGone() || bExplosionPending || Amount <= 0)
	{
		return;
	}
	HP -= Amount;
	if (HP <= 0)
	{
		ScheduleExplosion(ChainDelay);
	}
}

void ACRBarrel::RemoveSilently()
{
	if (IsGone())
	{
		return;
	}
	bRemoved = true;
	GetWorldTimerManager().ClearTimer(ExplosionTimer);
	if (ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>())
	{
		GM->OnBarrelRemoved(this);
		GM->LogEvent(TEXT("Barrel lost without exploding"));
	}
	Destroy();
}

void ACRBarrel::OnBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (IsGone() || bExplosionPending || !OtherActor || Cast<ACRArena>(OtherActor))
	{
		return;
	}

	if (ACRBoundarySegment* Segment = Cast<ACRBoundarySegment>(OtherActor))
	{
		Segment->HandleBodyHit(Body, PrevVelocity);
	}

	FVector OtherVelocity = FVector::ZeroVector;
	if (const ACREnemy* Enemy = Cast<ACREnemy>(OtherActor))
	{
		OtherVelocity = Enemy->GetPrevVelocity();
	}
	else if (const ACRBarrel* Barrel = Cast<ACRBarrel>(OtherActor))
	{
		OtherVelocity = Barrel->GetPrevVelocity();
	}

	const FVector Relative = OtherVelocity - PrevVelocity;
	const float ImpactSpeed = FMath::Abs(FVector::DotProduct(Relative, Hit.ImpactNormal));
	if (ImpactSpeed >= ImpactExplodeSpeed)
	{
		if (ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>())
		{
			GM->LogEvent(FString::Printf(TEXT("Barrel hit hard (%.0f cm/s)"), ImpactSpeed));
		}
		ScheduleExplosion(0.02f);
	}
}

void ACRBarrel::ScheduleExplosion(float Delay)
{
	if (IsGone() || bExplosionPending)
	{
		return;
	}
	bExplosionPending = true;
	GetWorldTimerManager().SetTimer(ExplosionTimer, this, &ACRBarrel::Explode, FMath::Max(Delay, 0.01f), false);
}

void ACRBarrel::Explode()
{
	if (IsGone())
	{
		return;
	}
	bExploded = true;
	bExplosionPending = false;

	const FVector Origin = GetActorLocation();
	if (ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>())
	{
		GM->OnBarrelRemoved(this);
		GM->ApplyRadialBlast(Origin, ExplosionRadius, ExplosionDamage, ExplosionImpulse, true, TEXT("BARREL EXPLOSION"));
	}
	Destroy();
}
