// Runtime greybox "sanctuary in a bottle": a round floor inside a ring of glass ribs, a fixed camera and three
// building anchors (Heart, Workshop, Storage). Each anchor's blocks are rebuilt when its level changes, so
// progression is visible in the scene. The hub HUD draws the clickable markers at GetAnchorLocation().

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRHubRoomActor.generated.h"

class UCameraComponent;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class CARDSROGUELIKE_API ACRHubRoomActor : public AActor
{
	GENERATED_BODY()

public:
	ACRHubRoomActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Rebuilds the blocks of one anchor for a building level (0 = not built yet). */
	void SetAnchorLevel(FName AnchorId, int32 Level);

	/** Ring under an anchor: 0 = off, 1 = hovered, 2 = selected. */
	void SetAnchorHighlight(FName AnchorId, int32 Highlight);

	/** World point above an anchor for its HUD marker (the actor location if the anchor is unknown). */
	FVector GetAnchorLocation(FName AnchorId) const;

	static FName HeartAnchor() { return TEXT("Heart"); }
	static FName WorkshopAnchor() { return TEXT("Workshop"); }
	static FName StorageAnchor() { return TEXT("Storage"); }

	UPROPERTY(EditAnywhere, Category = "CR|Hub")
	float CameraExposureEV100 = 3.f;

protected:
	virtual void BeginPlay() override;

private:
	struct FAnchor
	{
		FName Id;
		FVector Location = FVector::ZeroVector;
		float MarkerHeight = 300.f;
		TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
		TObjectPtr<UStaticMeshComponent> Ring;
	};

	FAnchor* FindAnchor(FName AnchorId);
	const FAnchor* FindAnchor(FName AnchorId) const;
	UStaticMeshComponent* AddBlock(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, const FLinearColor& Color,
		const FRotator& Rotation = FRotator::ZeroRotator);
	void BuildHeart(FAnchor& Anchor, int32 Level);
	void BuildWorkshop(FAnchor& Anchor, int32 Level);
	void BuildStorage(FAnchor& Anchor, int32 Level);

	UPROPERTY(VisibleAnywhere, Category = "CR|Hub")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "CR|Hub")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	/** Warm light of the Heart; brighter with its level. */
	UPROPERTY()
	TObjectPtr<UPointLightComponent> HeartLight;

	/** Blocks that belong to no anchor (floor, glass ribs). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> StaticBlocks;

	/** Anchor pieces, kept referenced for GC (FAnchor is not reflected). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AnchorPieces;

	TArray<FAnchor> Anchors;
	float Time = 0.f;
	float HeartLightBase = 0.f;
	/** The Heart crystal bobs gently. */
	TObjectPtr<UStaticMeshComponent> HeartCrystal;
	FVector HeartCrystalBase = FVector::ZeroVector;
};
