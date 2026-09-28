// Explosive barrel: explodes from damage or a hard enough impact.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRBarrel.generated.h"

class UStaticMeshComponent;

UCLASS()
class CARDSROGUELIKE_API ACRBarrel : public AActor
{
	GENERATED_BODY()

public:
	ACRBarrel();

	virtual void Tick(float DeltaSeconds) override;

	/** Any damage >= HP schedules an explosion after ChainDelay. */
	void ReceiveDamage(int32 Amount);

	/** Removes the barrel without exploding (pit, void). */
	void RemoveSilently();

	bool IsExplosionPending() const { return bExplosionPending; }
	bool IsGone() const { return bExploded || bRemoved; }
	UStaticMeshComponent* GetBody() const { return Body; }
	FVector GetPrevVelocity() const { return PrevVelocity; }

	UPROPERTY(EditAnywhere, Category = "CR|Barrel")
	int32 HP = 1;

	/** Relative impact speed (cm/s) along the contact normal that triggers an explosion. */
	UPROPERTY(EditAnywhere, Category = "CR|Barrel")
	float ImpactExplodeSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category = "CR|Barrel")
	float ExplosionRadius = 450.f;

	UPROPERTY(EditAnywhere, Category = "CR|Barrel")
	int32 ExplosionDamage = 5;

	/** Radial velocity change (cm/s) at the explosion center. */
	UPROPERTY(EditAnywhere, Category = "CR|Barrel")
	float ExplosionImpulse = 1600.f;

	UPROPERTY(EditAnywhere, Category = "CR|Barrel")
	float ChainDelay = 0.15f;

	UPROPERTY(EditAnywhere, Category = "CR|Barrel")
	float MassKg = 40.f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void ScheduleExplosion(float Delay);
	void Explode();

	UPROPERTY(VisibleAnywhere, Category = "CR|Barrel")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Visual-only dark hoops so the barrel reads differently from enemies. */
	UPROPERTY(VisibleAnywhere, Category = "CR|Barrel")
	TArray<TObjectPtr<UStaticMeshComponent>> Hoops;

	FTimerHandle ExplosionTimer;
	FVector PrevVelocity = FVector::ZeroVector;
	bool bExplosionPending = false;
	bool bExploded = false;
	bool bRemoved = false;
};
