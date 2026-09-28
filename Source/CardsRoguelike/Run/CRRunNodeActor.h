// Visual for one run-map room: a pedestal colored by room type, with a state halo and label.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRRunTypes.h"
#include "CRRunNodeActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CARDSROGUELIKE_API ACRRunNodeActor : public AActor
{
	GENERATED_BODY()

public:
	ACRRunNodeActor();

	virtual void Tick(float DeltaSeconds) override;

	void InitNode(FName InNodeId, ECRRoomType InRoomType);

	/** Applies state + hover visuals. The actor location is the pedestal's top center. */
	void SetVisualState(ECRRunNodeState InState, bool bHovered);

	FName GetNodeId() const { return NodeId; }
	ECRRoomType GetRoomType() const { return RoomType; }

private:
	void SetComponentColor(UStaticMeshComponent* Component, const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere, Category = "CR|RunMap")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "CR|RunMap")
	TObjectPtr<UStaticMeshComponent> Pedestal;

	/** Hidden, non-animated pick shape matching the resting pedestal (mouse hover/click). */
	UPROPERTY(VisibleAnywhere, Category = "CR|RunMap")
	TObjectPtr<UStaticMeshComponent> HitArea;

	UPROPERTY(VisibleAnywhere, Category = "CR|RunMap")
	TObjectPtr<UStaticMeshComponent> Halo;

	UPROPERTY(VisibleAnywhere, Category = "CR|RunMap")
	TObjectPtr<UTextRenderComponent> Label;

	FName NodeId;
	ECRRoomType RoomType = ECRRoomType::Combat;
	float PedestalScale = 1.f;
};
