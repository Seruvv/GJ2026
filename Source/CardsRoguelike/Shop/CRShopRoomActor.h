// Runtime graybox shop interior: floor, walls, counter, blockout props and the fixed shop camera.
// Deliberately utilitarian so it can be replaced once the jam theme is known.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRShopRoomActor.generated.h"

class UCameraComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class CARDSROGUELIKE_API ACRShopRoomActor : public AActor
{
	GENERATED_BODY()

public:
	ACRShopRoomActor();

	/** Where the merchant stands (behind the counter), in world space. */
	FVector GetMerchantLocation() const;

	/** Locked exposure (EV100), same approach as the combat and run-map cameras. */
	UPROPERTY(EditAnywhere, Category = "CR|Shop")
	float CameraExposureEV100 = 3.f;

protected:
	virtual void BeginPlay() override;

private:
	UStaticMeshComponent* AddBlock(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, const FLinearColor& Color, bool bCollision = false);

	UPROPERTY(VisibleAnywhere, Category = "CR|Shop")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "CR|Shop")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Blocks;
};
