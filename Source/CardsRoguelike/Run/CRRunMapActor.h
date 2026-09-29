// Runtime 3D diorama of the run graph: pedestals, path beams, the hamster marker and the map camera.
// Reads run data from UCRRunSubsystem; owns no run state itself.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRRunMapActor.generated.h"

class ACRRunNodeActor;
class UCameraComponent;
class UCRRunSubsystem;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class CARDSROGUELIKE_API ACRRunMapActor : public AActor
{
	GENERATED_BODY()

public:
	ACRRunMapActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Hovered node id (NAME_None for none). Only Available nodes show hover feedback. */
	void SetHoveredNode(FName NodeId);

	bool IsMarkerMoving() const { return bMarkerMoving; }

	/** Room reached by the last move, shown by the HUD until the next move. Empty at run start. */
	const FString& GetArrivalTitle() const { return ArrivalTitle; }
	const FString& GetArrivalSubtitle() const { return ArrivalSubtitle; }

	/** Spawned room actors by node id (the HUD draws their labels). */
	const TMap<FName, TObjectPtr<ACRRunNodeActor>>& GetNodeActors() const { return NodeActors; }

	UPROPERTY(EditAnywhere, Category = "CR|RunMap")
	float MarkerMoveTime = 0.9f;

	/** Pause on the arrival banner before loading the combat map. */
	UPROPERTY(EditAnywhere, Category = "CR|RunMap")
	float EnterCombatDelay = 1.0f;

	UPROPERTY(EditAnywhere, Category = "CR|RunMap")
	float CameraPitch = -55.f;

	/** Locked exposure (EV100) for the map camera, same approach as the combat camera. */
	UPROPERTY(EditAnywhere, Category = "CR|RunMap")
	float CameraExposureEV100 = 3.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FPathBeam
	{
		FName From;
		FName To;
		TObjectPtr<UStaticMeshComponent> Mesh;
	};

	void OnRunStateChanged();
	void RebuildMap();
	void ClearMap();
	void RefreshVisuals();
	void PlaceCamera();
	FVector GetMarkerRestLocation(FName NodeId) const;
	void OnMarkerArrived();
	void OpenRoomMap();
	UStaticMeshComponent* AddMeshComponent(UStaticMesh* Mesh, const FLinearColor& Color);

	UCRRunSubsystem* GetRunSubsystem() const;

	UPROPERTY(VisibleAnywhere, Category = "CR|RunMap")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "CR|RunMap")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "CR|RunMap")
	TObjectPtr<UStaticMeshComponent> Marker;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TMap<FName, TObjectPtr<ACRRunNodeActor>> NodeActors;

	/** Base slab, pillars and path beams created for the current graph. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> MapPieces;

	TArray<FPathBeam> Beams;

	FDelegateHandle RunStateChangedHandle;
	FName DisplayedNodeId;
	FName HoveredNodeId;
	FString ArrivalTitle;
	FString ArrivalSubtitle;
	/** Short pause on the arrival banner before loading a combat or shop map. */
	FTimerHandle EnterCombatTimer;
	FString PendingRoomMap;

	bool bMarkerMoving = false;
	float MarkerAlpha = 0.f;
	float IdleTime = 0.f;
	FVector MarkerFrom = FVector::ZeroVector;
	FVector MarkerTo = FVector::ZeroVector;
};
