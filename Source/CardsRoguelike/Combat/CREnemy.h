// Physics-driven enemy with a preferred combat zone and a single action per enemy turn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRTypes.h"
#include "CREnemy.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CARDSROGUELIKE_API ACREnemy : public AActor
{
	GENERATED_BODY()

public:
	ACREnemy();

	virtual void Tick(float DeltaSeconds) override;

	/** Call before FinishSpawning. */
	void InitEnemy(const FCREnemyStats& InStats, const FString& InDisplayName);

	/** Recomputes intent from the current zone. */
	void UpdateIntent();
	bool WillAttack() const { return bIntentAttack; }

	void StartAction();
	bool IsActionFinished() const { return !bMoving; }
	void StopAction();

	void ApplyCombatDamage(int32 Amount, const FString& Source);

	/** Removes the enemy from combat. Hazard reasons can be refused by CanBeEliminatedBy. */
	void Eliminate(ECREliminationReason Reason);

	/** Override point for future bosses immune to specific hazards. */
	virtual bool CanBeEliminatedBy(ECREliminationReason Reason) const { return true; }

	bool IsEliminated() const { return bEliminated; }
	UStaticMeshComponent* GetBody() const { return Body; }
	FVector GetPrevVelocity() const { return PrevVelocity; }
	const FCREnemyStats& GetStats() const { return Stats; }
	int32 GetHP() const { return HP; }
	const FString& GetDisplayName() const { return DisplayName; }
	ECRCombatType GetCombatType() const { return Stats.CombatType; }

	/** Approximate horizontal radius used for blast overlap. */
	UPROPERTY(EditAnywhere, Category = "CR|Enemy")
	float BodyRadius = 50.f;

	/** Visual walking speed (cm/s) while executing a move action. */
	UPROPERTY(EditAnywhere, Category = "CR|Enemy")
	float WalkSpeed = 700.f;

	/** A move ends when the destination is closer than this. */
	UPROPERTY(EditAnywhere, Category = "CR|Enemy")
	float ArriveTolerance = 25.f;

	/** A move ends if the body makes almost no progress for this much simulated time (blocked). */
	UPROPERTY(EditAnywhere, Category = "CR|Enemy")
	float StallTime = 0.5f;

	UPROPERTY(EditAnywhere, Category = "CR|Enemy")
	float LinearDamping = 0.8f;

	UPROPERTY(EditAnywhere, Category = "CR|Enemy")
	float AngularDamping = 4.f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void PerformAttack();
	void RefreshTexts();

	UPROPERTY(VisibleAnywhere, Category = "CR|Enemy")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "CR|Enemy")
	TObjectPtr<UTextRenderComponent> IntentText;

	UPROPERTY(VisibleAnywhere, Category = "CR|Enemy")
	TObjectPtr<UTextRenderComponent> HPText;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(VisibleAnywhere, Category = "CR|Enemy")
	FCREnemyStats Stats;

	FString DisplayName;
	int32 HP = 1;
	int32 Armor = 0;
	bool bIntentAttack = false;
	bool bMoving = false;
	bool bEliminated = false;
	FVector2D MoveTarget = FVector2D::ZeroVector;

	// Move action bookkeeping. Distance, not time, ends a normal move; the simulated-time
	// values only catch a blocked body or act as a watchdog.
	FVector2D LastMoveLocation = FVector2D::ZeroVector;
	float DistanceBudget = 0.f;
	float DistanceTravelled = 0.f;
	float MoveSimTime = 0.f;
	float StallSimTime = 0.f;
	FVector PrevVelocity = FVector::ZeroVector;
};
