#include "CRHamster.h"

#include "Camera/CameraComponent.h"
#include "CRCombatGameMode.h"
#include "CRTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ACRHamster::ACRHamster()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Body->SetStaticMesh(SphereFinder.Object);
	Body->SetRelativeScale3D(FVector(1.2f));
	// Kinematic blocker: enemies bounce off, the hamster never moves.
	Body->SetSimulatePhysics(false);
	Body->SetCollisionProfileName(TEXT("BlockAll"));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Body);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->SetUsingAbsoluteScale(true);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(Body);
	StatusText->SetUsingAbsoluteScale(true);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(55.f);
	StatusText->SetTextRenderColor(FColor(255, 220, 120));
}

void ACRHamster::BeginPlay()
{
	Super::BeginPlay();

	HP = MaxHP;
	Armor = StartingArmor;

	Camera->SetWorldLocation(GetActorLocation() + CameraOffset);
	Camera->SetWorldRotation(CameraRotation);
	Camera->SetFieldOfView(CameraFOV);

	// Lock exposure on this camera only (min == max), leaving project rendering settings alone.
	FPostProcessSettings& PostProcess = Camera->PostProcessSettings;
	PostProcess.bOverride_AutoExposureMinBrightness = true;
	PostProcess.bOverride_AutoExposureMaxBrightness = true;
	PostProcess.bOverride_AutoExposureBias = true;
	PostProcess.AutoExposureMinBrightness = CameraExposureEV100;
	PostProcess.AutoExposureMaxBrightness = CameraExposureEV100;
	PostProcess.AutoExposureBias = 0.f;
	Camera->PostProcessBlendWeight = 1.f;

	StatusText->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, 160.f));
	CRProto::ApplyColor(Body, FLinearColor(1.0f, 0.55f, 0.1f));
	RefreshStatus();
}

void ACRHamster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The HUD panel shows hamster status; the world label is a Debug View diagnostic only.
	const bool bDebug = CRProto::IsDebugView(this);
	StatusText->SetVisibility(bDebug);
	if (bDebug)
	{
		CRProto::FaceCamera(StatusText);
	}
}

int32 ACRHamster::ApplyCombatDamage(int32 Amount, const FString& Source)
{
	if (Amount <= 0 || IsDead())
	{
		return 0;
	}

	const int32 Absorbed = FMath::Min(Armor, Amount);
	Armor -= Absorbed;
	const int32 HPLoss = FMath::Min(HP, Amount - Absorbed);
	HP -= HPLoss;
	RefreshStatus();

	if (ACRCombatGameMode* GM = GetWorld()->GetAuthGameMode<ACRCombatGameMode>())
	{
		GM->LogEvent(FString::Printf(TEXT("Hamster takes %d from %s (armor -%d, HP -%d)"), Amount, *Source, Absorbed, HPLoss));
	}
	return HPLoss;
}

void ACRHamster::AddArmor(int32 Amount)
{
	Armor += FMath::Max(0, Amount);
	RefreshStatus();
}

int32 ACRHamster::Heal(int32 Amount)
{
	const int32 Healed = FMath::Clamp(Amount, 0, MaxHP - HP);
	HP += Healed;
	RefreshStatus();
	return Healed;
}

void ACRHamster::InitHealth(int32 InCurrentHP, int32 InMaxHP)
{
	MaxHP = FMath::Max(1, InMaxHP);
	HP = FMath::Clamp(InCurrentHP, 0, MaxHP);
	RefreshStatus();
}

void ACRHamster::RefreshStatus()
{
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("HAMSTER  HP %d/%d  ARM %d"), HP, MaxHP, Armor)));
}
