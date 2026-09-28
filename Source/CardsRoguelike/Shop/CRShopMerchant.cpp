#include "CRShopMerchant.h"

#include "../Combat/CRTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ACRShopMerchant::ACRShopMerchant()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyPivot"));
	BodyPivot->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(BodyPivot);
	Body->SetStaticMesh(CylinderFinder.Object);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Body->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.4f));

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(BodyPivot);
	Head->SetStaticMesh(SphereFinder.Object);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetRelativeLocation(FVector(0.f, 0.f, 172.f));
	Head->SetRelativeScale3D(FVector(0.6f));

	Hat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hat"));
	Hat->SetupAttachment(BodyPivot);
	Hat->SetStaticMesh(ConeFinder.Object);
	Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hat->SetRelativeLocation(FVector(0.f, 0.f, 222.f));
	Hat->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.5f));

	DialogueText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DialogueText"));
	DialogueText->SetupAttachment(SceneRoot);
	DialogueText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DialogueText->SetHorizontalAlignment(EHTA_Center);
	DialogueText->SetVerticalAlignment(EVRTA_TextCenter);
	DialogueText->SetWorldSize(34.f);
	DialogueText->SetTextRenderColor(FColor(255, 225, 150));
	DialogueText->SetRelativeLocation(FVector(0.f, 0.f, 285.f));
}

void ACRShopMerchant::BeginPlay()
{
	Super::BeginPlay();
	CRProto::ApplyColor(Body, FLinearColor(0.25f, 0.4f, 0.3f));
	CRProto::ApplyColor(Head, FLinearColor(0.85f, 0.7f, 0.55f));
	CRProto::ApplyColor(Hat, FLinearColor(0.45f, 0.15f, 0.35f));
	// Small phase offset so a future second merchant would not move in lockstep.
	IdleTime = FMath::FRandRange(0.f, 2.f);
}

void ACRShopMerchant::SetDialogueLine(const FString& Line)
{
	DialogueText->SetText(FText::FromString(Line.IsEmpty() ? FString() : FString::Printf(TEXT("\"%s\""), *Line)));
}

void ACRShopMerchant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Placeholder idle: gentle bob plus a slow yaw sway, replaceable by real animation later.
	IdleTime += DeltaSeconds;
	BodyPivot->SetRelativeLocation(FVector(0.f, 0.f, FMath::Sin(IdleTime * BobSpeed) * BobHeight));
	BodyPivot->SetRelativeRotation(FRotator(0.f, FMath::Sin(IdleTime * SwaySpeed) * SwayDegrees, 0.f));
	CRProto::FaceCamera(DialogueText);
}
