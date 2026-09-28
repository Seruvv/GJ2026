#include "CREnemy.h"

#include "CRArena.h"
#include "CRBoundarySegment.h"
#include "CRCombatGameMode.h"
#include "CRHamster.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ACREnemy::ACREnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetStaticMesh(CubeMesh);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetGenerateOverlapEvents(true);
	Body->BodyInstance.bUseCCD = true;
	// Stay upright: only yaw may rotate.
	Body->BodyInstance.DOFMode = EDOFMode::SixDOF;
	Body->BodyInstance.bLockXRotation = true;
	Body->BodyInstance.bLockYRotation = true;

	IntentText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IntentText"));
	IntentText->SetupAttachment(Body);
	IntentText->SetUsingAbsoluteRotation(true);
	IntentText->SetUsingAbsoluteScale(true);
	IntentText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IntentText->SetHorizontalAlignment(EHTA_Center);
	IntentText->SetVerticalAlignment(EVRTA_TextCenter);
	IntentText->SetWorldSize(60.f);

	HPText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HPText"));
	HPText->SetupAttachment(Body);
	HPText->SetUsingAbsoluteRotation(true);
	HPText->SetUsingAbsoluteScale(true);
	HPText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HPText->SetHorizontalAlignment(EHTA_Center);
	HPText->SetVerticalAlignment(EVRTA_TextCenter);
	HPText->SetWorldSize(45.f);
	HPText->SetTextRenderColor(FColor::White);
}

void ACREnemy::InitEnemy(const FCREnemyStats& InStats, const FString& InDisplayName)
{
	Stats = InStats;
	DisplayName = InDisplayName;
	HP = Stats.MaxHP;
	Armor = Stats.Armor;

	if (Stats.CombatType == ECRCombatType::Melee)
	{
		Body->SetStaticMesh(CubeMesh);
		Body->SetWorldScale3D(FVector(0.9f, 0.9f, 1.0f));
	}
	else
	{
		Body->SetStaticMesh(CylinderMesh);
		Body->SetWorldScale3D(FVector(0.8f, 0.8f, 1.2f));
	}
	Body->SetMassOverrideInKg(NAME_None, Stats.MassKg, true);
	Body->SetLinearDamping(LinearDamping);
	Body->SetAngularDamping(AngularDamping);
}

void ACREnemy::BeginPlay()
{
	Super::BeginPlay();

	Body->OnComponentHit.AddDynamic(this, &ACREnemy::OnBodyHit);
	CRProto::ApplyColor(Body, Stats.CombatType == ECRCombatType::Melee
		? FLinearColor(0.85f, 0.12f, 0.08f)
		: FLinearColor(0.2f, 0.3f, 0.95f));
	// HP, intent and debug IDs are drawn by the combat HUD as screen overlays.
	IntentText->SetVisibility(false);
	HPText->SetVisibility(false);
	UpdateIntent();
}

void ACREnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bEliminated)
	{
		return;
	}

	const FVector Location = GetActorLocation();

	if (bMoving)
	{
		const float SimDelta = FMath::Max(CRProto::GetSimulatedDeltaSeconds(DeltaSeconds), KINDA_SMALL_NUMBER);
		const FVector2D Here(Location);
		const float Step = FVector2D::Distance(Here, LastMoveLocation);
		LastMoveLocation = Here;
		DistanceTravelled += Step;
		MoveSimTime += SimDelta;
		StallSimTime = Step < WalkSpeed * SimDelta * 0.1f ? StallSimTime + SimDelta : 0.f;

		const FVector2D Delta = MoveTarget - Here;
		const float Remaining = Delta.Size();
		const float BudgetLeft = DistanceBudget - DistanceTravelled;
		const float Watchdog = DistanceBudget / FMath::Max(WalkSpeed, 1.f) * 2.f + 1.f;
		const FVector CurrentVelocity = Body->GetPhysicsLinearVelocity();

		if (Remaining < ArriveTolerance || BudgetLeft <= 0.f || StallSimTime >= StallTime || MoveSimTime >= Watchdog)
		{
			bMoving = false;
			Body->SetPhysicsLinearVelocity(FVector(0.f, 0.f, CurrentVelocity.Z));
		}
		else
		{
			// Never ask physics to cover more than the destination or the remaining budget in one step.
			const float StepSpeed = FMath::Min(WalkSpeed, FMath::Min(Remaining, BudgetLeft) / SimDelta);
			Body->SetPhysicsLinearVelocity(FVector(Delta.GetSafeNormal() * StepSpeed, CurrentVelocity.Z));
		}
	}

	PrevVelocity = Body->GetPhysicsLinearVelocity();
}

void ACREnemy::UpdateIntent()
{
	const ACRCombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRCombatGameMode>() : nullptr;
	const ACRArena* Arena = GM ? GM->GetArena() : nullptr;
	if (!Arena)
	{
		bIntentAttack = false;
		RefreshTexts();
		return;
	}

	const ECRZone Preferred = Stats.CombatType == ECRCombatType::Melee ? ECRZone::Inner : ECRZone::Outer;
	bIntentAttack = Arena->GetZone(FVector2D(GetActorLocation())) == Preferred;
	RefreshTexts();
}

void ACREnemy::RefreshTexts()
{
	const TCHAR* TypeName = Stats.CombatType == ECRCombatType::Melee ? TEXT("MELEE") : TEXT("RANGED");
	if (bIntentAttack)
	{
		IntentText->SetText(FText::FromString(FString::Printf(TEXT("ATTACK %d"), Stats.Damage)));
		IntentText->SetTextRenderColor(FColor(255, 60, 40));
	}
	else
	{
		IntentText->SetText(FText::FromString(TEXT("MOVE")));
		IntentText->SetTextRenderColor(FColor(255, 230, 60));
	}

	const FString ArmorText = Armor > 0 ? FString::Printf(TEXT(" ARM %d"), Armor) : FString();
	HPText->SetText(FText::FromString(FString::Printf(TEXT("%s %s  HP %d/%d%s"), *DisplayName, TypeName, HP, Stats.MaxHP, *ArmorText)));
}

void ACREnemy::StartAction()
{
	UpdateIntent();

	ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>();
	const ACRArena* Arena = GM ? GM->GetArena() : nullptr;
	if (!Arena)
	{
		return;
	}

	if (bIntentAttack)
	{
		PerformAttack();
		bMoving = false;
		return;
	}

	LastMoveLocation = FVector2D(GetActorLocation());
	MoveTarget = Arena->ComputeMoveTarget(Stats.CombatType, LastMoveLocation, Stats.Speed);
	DistanceBudget = Stats.Speed;
	DistanceTravelled = 0.f;
	MoveSimTime = 0.f;
	StallSimTime = 0.f;
	bMoving = true;
	GM->LogEvent(FString::Printf(TEXT("%s moves toward its %s zone"), *DisplayName,
		Stats.CombatType == ECRCombatType::Melee ? TEXT("inner") : TEXT("outer")));
}

void ACREnemy::StopAction()
{
	if (bMoving)
	{
		bMoving = false;
		const FVector V = Body->GetPhysicsLinearVelocity();
		Body->SetPhysicsLinearVelocity(FVector(0.f, 0.f, V.Z));
	}
}

void ACREnemy::PerformAttack()
{
	ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>();
	ACRHamster* Hamster = GM ? GM->GetHamster() : nullptr;
	if (!Hamster)
	{
		return;
	}

	const FColor LineColor = Stats.CombatType == ECRCombatType::Melee ? FColor::Red : FColor(255, 120, 0);
	DrawDebugLine(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 40.f), Hamster->GetActorLocation(), LineColor, false, 0.8f, 0, 8.f);
	GM->LogEvent(FString::Printf(TEXT("%s attacks the hamster (%s)"), *DisplayName,
		Stats.CombatType == ECRCombatType::Melee ? TEXT("melee") : TEXT("ranged")));
	Hamster->ApplyCombatDamage(Stats.Damage, DisplayName);
}

void ACREnemy::ApplyCombatDamage(int32 Amount, const FString& Source)
{
	if (bEliminated || Amount <= 0)
	{
		return;
	}

	const int32 Absorbed = FMath::Min(Armor, Amount);
	Armor -= Absorbed;
	HP -= Amount - Absorbed;

	if (ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>())
	{
		GM->LogEvent(FString::Printf(TEXT("%s takes %d from %s"), *DisplayName, Amount, *Source));
	}

	if (HP <= 0)
	{
		Eliminate(ECREliminationReason::Damage);
	}
	else
	{
		RefreshTexts();
	}
}

void ACREnemy::Eliminate(ECREliminationReason Reason)
{
	if (bEliminated)
	{
		return;
	}
	if (Reason != ECREliminationReason::Damage && !CanBeEliminatedBy(Reason))
	{
		return;
	}

	bEliminated = true;
	bMoving = false;
	if (ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>())
	{
		GM->OnEnemyEliminated(this, Reason);
	}
	Destroy();
}

void ACREnemy::OnBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (ACRBoundarySegment* Segment = Cast<ACRBoundarySegment>(OtherActor))
	{
		Segment->HandleBodyHit(Body, PrevVelocity);
	}
}
