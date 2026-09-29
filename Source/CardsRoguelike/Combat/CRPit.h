// Pit hazard: enemies that slide into it are eliminated.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRPit.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CARDSROGUELIKE_API ACRPit : public AActor
{
	GENERATED_BODY()

public:
	ACRPit();

	virtual void Tick(float DeltaSeconds) override;

	void SetPitSize(const FVector2D& InSize);

	/** World position of the pit's name label (centre, slightly above the floor). */
	FVector GetLabelWorldLocation() const;

	/** The trigger is shrunk by this margin so a body has to be mostly inside. */
	UPROPERTY(EditAnywhere, Category = "CR|Pit")
	float TriggerInset = 40.f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "CR|Pit")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "CR|Pit")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, Category = "CR|Pit")
	TObjectPtr<UTextRenderComponent> Label;

	/** Visual-only red hazard rim around the pit. */
	UPROPERTY(VisibleAnywhere, Category = "CR|Pit")
	TArray<TObjectPtr<UStaticMeshComponent>> Rim;

	FVector2D Size = FVector2D(220.f, 220.f);
};
