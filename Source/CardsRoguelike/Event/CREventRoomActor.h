// Runtime graybox event room: a small stage with a fixed camera and one illustration preset
// (camp, shrine, door...). The narrative HUD covers the left of the screen; the scene sits right of it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CREventTypes.h"
#include "CREventRoomActor.generated.h"

class UCameraComponent;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class CARDSROGUELIKE_API ACREventRoomActor : public AActor
{
	GENERATED_BODY()

public:
	ACREventRoomActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Builds one of the built-in graybox illustrations on the stage. */
	void BuildPresentation(ECREventPresentation Presentation);

	/** Where a custom presentation actor (UCREventDefinition::PresentationActorClass) should be spawned. */
	FTransform GetStageTransform() const;

	/** Locked exposure (EV100), same approach as the other room cameras. */
	UPROPERTY(EditAnywhere, Category = "CR|Event")
	float CameraExposureEV100 = 3.f;

protected:
	virtual void BeginPlay() override;

private:
	UStaticMeshComponent* AddBlock(UStaticMesh* Mesh, const FVector& StageLocation, const FVector& Size, const FLinearColor& Color,
		const FRotator& Rotation = FRotator::ZeroRotator);
	void AddGlow(const FVector& StageLocation, const FLinearColor& Color, float Intensity, float Radius, float FlickerSpeed);

	void BuildAbandonedCamp();
	void BuildBlackShrine();
	void BuildThingBehindDoor();

	UPROPERTY(VisibleAnywhere, Category = "CR|Event")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "CR|Event")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Blocks;

	/** Small point lights (embers, candles, the gap under the door) that flicker gently. */
	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> Glows;

	/** Parallel to Glows: X = base intensity, Y = flicker speed, Z = phase. */
	TArray<FVector> GlowParams;
	float Time = 0.f;
};
