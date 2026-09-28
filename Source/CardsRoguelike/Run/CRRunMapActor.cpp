#include "CRRunMapActor.h"

#include "../Combat/CRTypes.h"
#include "Camera/CameraComponent.h"
#include "CRRunNodeActor.h"
#include "CRRunSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const float BaseDepth = 180.f;      // Diorama base sits this far below the lowest pedestal.
	const float MarkerHover = 60.f;     // Marker center above the pedestal top.
	const float PedestalRadius = 100.f;

	/** Recolors a mesh, reusing its dynamic material once one exists. */
	void SetMeshColor(UStaticMeshComponent* Component, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
		else
		{
			CRProto::ApplyColor(Component, Color);
		}
	}
}

ACRRunMapActor::ACRRunMapActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->SetFieldOfView(70.f);

	// Hamster placeholder: an orange sphere, clearly not a room.
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(SceneRoot);
	Marker->SetStaticMesh(SphereFinder.Object);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetRelativeScale3D(FVector(0.7f));
}

UCRRunSubsystem* ACRRunMapActor::GetRunSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCRRunSubsystem>() : nullptr;
}

void ACRRunMapActor::BeginPlay()
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
	CRProto::ApplyColor(Marker, FLinearColor(1.f, 0.5f, 0.05f));

	if (UCRRunSubsystem* Run = GetRunSubsystem())
	{
		RunStateChangedHandle = Run->OnRunStateChanged.AddUObject(this, &ACRRunMapActor::OnRunStateChanged);
	}
	RebuildMap();
}

void ACRRunMapActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCRRunSubsystem* Run = GetRunSubsystem())
	{
		Run->OnRunStateChanged.Remove(RunStateChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

UStaticMeshComponent* ACRRunMapActor::AddMeshComponent(UStaticMesh* Mesh, const FLinearColor& Color)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetupAttachment(SceneRoot);
	Component->RegisterComponent();
	CRProto::ApplyColor(Component, Color);
	MapPieces.Add(Component);
	return Component;
}

void ACRRunMapActor::ClearMap()
{
	for (const TPair<FName, TObjectPtr<ACRRunNodeActor>>& Pair : NodeActors)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	NodeActors.Reset();
	for (UStaticMeshComponent* Piece : MapPieces)
	{
		if (Piece)
		{
			Piece->DestroyComponent();
		}
	}
	MapPieces.Reset();
	Beams.Reset();
}

void ACRRunMapActor::RebuildMap()
{
	// A restart (R) during the short "entering combat" pause must not still load combat.
	GetWorldTimerManager().ClearTimer(EnterCombatTimer);
	ClearMap();

	// A failed run keeps its map on screen for inspection; only "no run at all" shows nothing.
	const UCRRunSubsystem* Run = GetRunSubsystem();
	if (!Run || !Run->HasRun())
	{
		Marker->SetVisibility(false);
		DisplayedNodeId = NAME_None;
		return;
	}

	const FCRRunState& State = Run->GetRunState();
	const FVector Origin = GetActorLocation();

	// Diorama base slab under the whole route, and a pillar holding up each pedestal.
	FBox Bounds(ForceInit);
	for (const FCRRunNodeData& Node : State.Nodes)
	{
		Bounds += Origin + Node.Position;
	}
	const float BaseTop = Bounds.Min.Z - BaseDepth;
	const FVector2D BaseSize = FVector2D(Bounds.GetSize()) + FVector2D(900.f, 900.f);
	UStaticMeshComponent* Base = AddMeshComponent(CubeMesh, FLinearColor(0.07f, 0.08f, 0.06f));
	Base->SetWorldLocation(FVector(FVector2D(Bounds.GetCenter()), BaseTop - 25.f));
	Base->SetWorldScale3D(FVector(BaseSize.X / 100.f, BaseSize.Y / 100.f, 0.5f));

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FCRRunNodeData& Node : State.Nodes)
	{
		const FVector Top = Origin + Node.Position;
		const float PillarHeight = FMath::Max(Top.Z - 40.f - BaseTop, 1.f);
		UStaticMeshComponent* Pillar = AddMeshComponent(CylinderMesh, FLinearColor(0.12f, 0.11f, 0.1f));
		Pillar->SetWorldLocation(FVector(Top.X, Top.Y, BaseTop + PillarHeight * 0.5f));
		Pillar->SetWorldScale3D(FVector(0.9f, 0.9f, PillarHeight / 100.f));

		if (ACRRunNodeActor* NodeActor = GetWorld()->SpawnActor<ACRRunNodeActor>(ACRRunNodeActor::StaticClass(), FTransform(Top), Params))
		{
			NodeActor->InitNode(Node.NodeId, Node.RoomType);
			NodeActors.Add(Node.NodeId, NodeActor);
		}
	}

	// Physical path beams between pedestal rims (one per directed connection).
	for (const FCRRunNodeData& Node : State.Nodes)
	{
		for (const FName NextId : Node.ConnectedNodeIds)
		{
			const FCRRunNodeData* Next = Run->FindNode(NextId);
			if (!Next)
			{
				continue;
			}
			const FVector A = Origin + Node.Position + FVector(0.f, 0.f, -8.f);
			const FVector B = Origin + Next->Position + FVector(0.f, 0.f, -8.f);
			const FVector Dir = (B - A).GetSafeNormal();
			const FVector Start = A + Dir * PedestalRadius;
			const FVector End = B - Dir * PedestalRadius;

			UStaticMeshComponent* Beam = AddMeshComponent(CubeMesh, FLinearColor(0.12f, 0.12f, 0.14f));
			Beam->SetWorldLocationAndRotation((Start + End) * 0.5f, Dir.Rotation());
			Beam->SetWorldScale3D(FVector(FVector::Distance(Start, End) / 100.f, 0.28f, 0.1f));
			Beams.Add({ Node.NodeId, NextId, Beam });
		}
	}

	PlaceCamera();

	DisplayedNodeId = State.CurrentNodeId;
	bMarkerMoving = false;
	ArrivalTitle.Reset();
	ArrivalSubtitle.Reset();

	// Returning from a won combat: the hamster stands on the cleared room.
	const FCRRunNodeData* CurrentNode = Run->GetCurrentNode();
	if (Run->IsRunActive() && State.bCurrentRoomResolved && CurrentNode && CurrentNode->RoomType == ECRRoomType::Combat)
	{
		ArrivalTitle = TEXT("COMBAT CLEARED");
	}
	else if (Run->IsRunActive() && State.bCurrentRoomResolved && CurrentNode && CurrentNode->RoomType == ECRRoomType::Shop)
	{
		ArrivalTitle = TEXT("LEFT THE SHOP");
	}
	else if (Run->IsRunActive() && State.bCurrentRoomResolved && CurrentNode && CurrentNode->RoomType == ECRRoomType::Event)
	{
		// The event's own result screen already listed the consequences.
		ArrivalTitle = TEXT("EVENT RESOLVED");
	}

	Marker->SetVisibility(true);
	Marker->SetWorldLocation(GetMarkerRestLocation(DisplayedNodeId));
	RefreshVisuals();
}

void ACRRunMapActor::PlaceCamera()
{
	FBox Bounds(ForceInit);
	for (const TPair<FName, TObjectPtr<ACRRunNodeActor>>& Pair : NodeActors)
	{
		Bounds += Pair.Value->GetActorLocation();
	}
	if (!Bounds.IsValid)
	{
		return;
	}

	// Route progress (+X) reads left-to-right: the camera looks along -Y, so screen-right is +X.
	const FRotator Rotation(CameraPitch, -90.f, 0.f);
	const FVector Size = Bounds.GetSize();
	const float Distance = FMath::Max(Size.X * 1.05f, 3000.f);
	Camera->SetWorldRotation(Rotation);
	Camera->SetWorldLocation(Bounds.GetCenter() - Rotation.Vector() * Distance);
}

FVector ACRRunMapActor::GetMarkerRestLocation(FName NodeId) const
{
	if (const TObjectPtr<ACRRunNodeActor>* NodeActor = NodeActors.Find(NodeId))
	{
		return (*NodeActor)->GetActorLocation() + FVector(0.f, 0.f, MarkerHover);
	}
	return GetActorLocation();
}

void ACRRunMapActor::OnRunStateChanged()
{
	const UCRRunSubsystem* Run = GetRunSubsystem();
	if (!Run || !Run->HasRun() || NodeActors.Num() != Run->GetRunState().Nodes.Num())
	{
		// Run started, restarted or abandoned: rebuild from scratch.
		RebuildMap();
		return;
	}

	const FName NewNodeId = Run->GetRunState().CurrentNodeId;
	if (NewNodeId != DisplayedNodeId)
	{
		MarkerFrom = Marker->GetComponentLocation();
		MarkerTo = GetMarkerRestLocation(NewNodeId);
		MarkerAlpha = 0.f;
		bMarkerMoving = true;
		DisplayedNodeId = NewNodeId;
		HoveredNodeId = NAME_None;
		ArrivalTitle.Reset();
		ArrivalSubtitle.Reset();
	}
	RefreshVisuals();
}

void ACRRunMapActor::SetHoveredNode(FName NodeId)
{
	if (NodeId != HoveredNodeId)
	{
		HoveredNodeId = NodeId;
		RefreshVisuals();
	}
}

void ACRRunMapActor::RefreshVisuals()
{
	const UCRRunSubsystem* Run = GetRunSubsystem();
	if (!Run || !Run->HasRun())
	{
		return;
	}
	const FCRRunState& State = Run->GetRunState();

	for (const FCRRunNodeData& Node : State.Nodes)
	{
		if (TObjectPtr<ACRRunNodeActor>* NodeActor = NodeActors.Find(Node.NodeId))
		{
			const bool bHovered = !bMarkerMoving && Node.NodeId == HoveredNodeId && Node.State == ECRRunNodeState::Available;
			(*NodeActor)->SetVisualState(Node.State, bHovered);
		}
	}

	for (const FPathBeam& Beam : Beams)
	{
		const FCRRunNodeData* To = Run->FindNode(Beam.To);
		const int32 FromIndex = State.VisitedNodeIds.Find(Beam.From);
		const bool bTraversed = FromIndex != INDEX_NONE && State.VisitedNodeIds.IsValidIndex(FromIndex + 1) && State.VisitedNodeIds[FromIndex + 1] == Beam.To;
		const bool bOpen = Beam.From == State.CurrentNodeId && To && To->State == ECRRunNodeState::Available;

		FLinearColor Color(0.12f, 0.12f, 0.14f);
		if (bOpen)
		{
			Color = Beam.To == HoveredNodeId ? FLinearColor::White : FLinearColor(1.f, 0.8f, 0.2f);
		}
		else if (bTraversed)
		{
			Color = FLinearColor(0.5f, 0.38f, 0.15f);
		}
		SetMeshColor(Beam.Mesh, Color);
	}
}

void ACRRunMapActor::OnMarkerArrived()
{
	const UCRRunSubsystem* Run = GetRunSubsystem();
	const FCRRunNodeData* Node = Run ? Run->GetCurrentNode() : nullptr;
	ArrivalTitle = Node ? FString::Printf(TEXT("%s ROOM"), *CRRun::RoomTypeName(Node->RoomType)) : FString();

	// Run state already points at the unresolved room; the room's map reads it on load.
	PendingRoomMap.Reset();
	if (Run && Run->IsInCombatRoom())
	{
		ArrivalSubtitle = TEXT("Entering combat...");
		PendingRoomMap = CRRun::CombatMapPath();
	}
	else if (Run && Run->IsInShopRoom())
	{
		ArrivalSubtitle = TEXT("Entering shop...");
		PendingRoomMap = CRRun::ShopMapPath();
	}
	else if (Run && Run->IsInEventRoom())
	{
		ArrivalSubtitle = TEXT("Something waits on the road...");
		PendingRoomMap = CRRun::EventMapPath();
	}
	else
	{
		ArrivalSubtitle = TEXT("Placeholder room - no gameplay yet");
	}

	if (!PendingRoomMap.IsEmpty())
	{
		GetWorldTimerManager().SetTimer(EnterCombatTimer, this, &ACRRunMapActor::OpenRoomMap, FMath::Max(EnterCombatDelay, 0.01f), false);
	}
	RefreshVisuals();
}

void ACRRunMapActor::OpenRoomMap()
{
	if (!PendingRoomMap.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*PendingRoomMap));
	}
}

void ACRRunMapActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMarkerMoving)
	{
		MarkerAlpha = FMath::Min(MarkerAlpha + DeltaSeconds / FMath::Max(MarkerMoveTime, 0.05f), 1.f);
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, MarkerAlpha, 2.f);
		const FVector Arc(0.f, 0.f, FMath::Sin(MarkerAlpha * PI) * 150.f);
		Marker->SetWorldLocation(FMath::Lerp(MarkerFrom, MarkerTo, Eased) + Arc);

		if (MarkerAlpha >= 1.f)
		{
			bMarkerMoving = false;
			OnMarkerArrived();
		}
	}
	else if (Marker->IsVisible())
	{
		// Gentle idle bob so the marker reads as "the hamster" rather than scenery.
		IdleTime += DeltaSeconds;
		Marker->SetWorldLocation(GetMarkerRestLocation(DisplayedNodeId) + FVector(0.f, 0.f, FMath::Sin(IdleTime * 2.5f) * 8.f));
	}
}
