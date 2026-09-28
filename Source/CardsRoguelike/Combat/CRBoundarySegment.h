// One edge of the outer arena polygon with a configurable boundary behavior.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRTypes.h"
#include "CRBoundarySegment.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CARDSROGUELIKE_API ACRBoundarySegment : public AActor
{
	GENERATED_BODY()

public:
	ACRBoundarySegment();

	virtual void Tick(float DeltaSeconds) override;

	/** Places the segment along edge A->B. The wall sits just outside the edge line. */
	void Setup(int32 InEdgeIndex, const FVector2D& A, const FVector2D& B, const FVector2D& InInwardNormal, ECRBoundaryType InType);

	void SetBoundaryType(ECRBoundaryType NewType);
	ECRBoundaryType GetBoundaryType() const { return BoundaryType; }
	FVector2D GetInwardNormal() const { return InwardNormal; }
	int32 GetEdgeIndex() const { return EdgeIndex; }

	/** Rubber reaction for a physics body that hit this wall. PreImpactVelocity is the body's velocity before the hit. */
	void HandleBodyHit(UPrimitiveComponent* Body, const FVector& PreImpactVelocity);

	UPROPERTY(EditAnywhere, Category = "CR|Boundary")
	float WallHeight = 150.f;

	UPROPERTY(EditAnywhere, Category = "CR|Boundary")
	float StripHeight = 12.f;

	UPROPERTY(EditAnywhere, Category = "CR|Boundary")
	float Thickness = 40.f;

	/** Rubber: outgoing speed = incoming speed * factor. */
	UPROPERTY(EditAnywhere, Category = "CR|Boundary")
	float RubberBounceFactor = 1.15f;

	UPROPERTY(EditAnywhere, Category = "CR|Boundary")
	float RubberMinSpeed = 900.f;

private:
	UFUNCTION()
	void OnMeshOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyVisuals();
	void RefreshLabel();

	UPROPERTY(VisibleAnywhere, Category = "CR|Boundary")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "CR|Boundary")
	TObjectPtr<UTextRenderComponent> Label;

	ECRBoundaryType BoundaryType = ECRBoundaryType::Normal;
	FVector2D InwardNormal = FVector2D(0.f, 1.f);
	float Length = 100.f;
	int32 EdgeIndex = INDEX_NONE;
	bool bLabelShowsDebug = false;

	TMap<TWeakObjectPtr<UPrimitiveComponent>, double> LastBounceTime;
};
