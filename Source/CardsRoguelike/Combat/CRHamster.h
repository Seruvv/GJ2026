// The player's hamster: fixed at the arena center, cannot be pushed, owns the combat camera.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CRHamster.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CARDSROGUELIKE_API ACRHamster : public APawn
{
	GENERATED_BODY()

public:
	ACRHamster();

	virtual void Tick(float DeltaSeconds) override;

	/** Armor absorbs damage before HP. Returns HP actually lost. */
	int32 ApplyCombatDamage(int32 Amount, const FString& Source);
	void AddArmor(int32 Amount);
	int32 Heal(int32 Amount);

	/** Overrides the prototype defaults, e.g. with the persistent run hamster's HP. */
	void InitHealth(int32 InCurrentHP, int32 InMaxHP);

	bool IsDead() const { return HP <= 0; }
	int32 GetHP() const { return HP; }
	int32 GetMaxHP() const { return MaxHP; }
	int32 GetArmor() const { return Armor; }
	float GetBodyRadius() const { return BodyRadius; }

	UPROPERTY(EditAnywhere, Category = "CR|Hamster")
	int32 MaxHP = 30;

	UPROPERTY(EditAnywhere, Category = "CR|Hamster")
	int32 StartingArmor = 0;

	UPROPERTY(EditAnywhere, Category = "CR|Hamster")
	float BodyRadius = 60.f;

	UPROPERTY(EditAnywhere, Category = "CR|Camera")
	FVector CameraOffset = FVector(0.f, -1850.f, 3000.f);

	UPROPERTY(EditAnywhere, Category = "CR|Camera")
	FRotator CameraRotation = FRotator(-62.f, 90.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "CR|Camera")
	float CameraFOV = 80.f;

	/**
	 * Locked exposure (EV100) for the combat camera. Auto-exposure brightens the mostly dark
	 * arena until it washes out; higher values darken the image.
	 */
	UPROPERTY(EditAnywhere, Category = "CR|Camera")
	float CameraExposureEV100 = 3.f;

protected:
	virtual void BeginPlay() override;

private:
	void RefreshStatus();

	UPROPERTY(VisibleAnywhere, Category = "CR|Hamster")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "CR|Hamster")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "CR|Hamster")
	TObjectPtr<UTextRenderComponent> StatusText;

	int32 HP = 0;
	int32 Armor = 0;
};
