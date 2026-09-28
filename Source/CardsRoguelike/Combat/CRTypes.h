// Shared types for the spatial card combat prototype.

#pragma once

#include "CoreMinimal.h"
#include "CRTypes.generated.h"

class UPrimitiveComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ECRTurnState : uint8
{
	PlayerTurn,
	ResolvingCard,
	EnemyTurn,
	Victory,
	Defeat,
	/** Run-integrated combat only: choosing a card reward after Victory. */
	Reward
};

UENUM(BlueprintType)
enum class ECRCombatType : uint8
{
	Melee,
	Ranged
};

UENUM(BlueprintType)
enum class ECRZone : uint8
{
	Inner,
	Outer,
	Outside
};

UENUM(BlueprintType)
enum class ECRBoundaryType : uint8
{
	Normal,
	Rubber,
	Void,
	TeleportInner
};

UENUM(BlueprintType)
enum class ECRCardEffect : uint8
{
	Push,
	Blast,
	Pull,
	Guard,
	Mend
};

UENUM(BlueprintType)
enum class ECRCardTargeting : uint8
{
	None,
	PhysicsTarget,
	PhysicsTargetThenPoint,
	GroundPoint
};

UENUM(BlueprintType)
enum class ECREliminationReason : uint8
{
	Damage,
	Void,
	Pit,
	Fell
};

USTRUCT(BlueprintType)
struct FCRCardDef
{
	GENERATED_BODY()

	/** Lightweight id used by the persistent run deck (e.g. "Push"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	FString Name;

	/** One-line effect summary shown on the prototype card. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	FString ShortText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 ManaCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	ECRCardEffect Effect = ECRCardEffect::Push;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	ECRCardTargeting Targeting = ECRCardTargeting::None;

	/** Velocity change (cm/s) for Push/Pull; radial velocity change for Blast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	float Strength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 Damage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	float Radius = 0.f;

	/** Armor for Guard, HP for Mend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 Amount = 0;

	/** Physical cards enter ResolvingCard and wait for bodies to settle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	bool bIsPhysical = false;
};

USTRUCT(BlueprintType)
struct FCREnemyStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	ECRCombatType CombatType = ECRCombatType::Melee;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	int32 MaxHP = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	int32 Damage = 3;

	/** Distance (cm) the enemy may move with one action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	float Speed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	int32 Armor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	float MassKg = 80.f;
};

USTRUCT(BlueprintType)
struct FCREnemySpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	ECRCombatType CombatType = ECRCombatType::Melee;

	/** Arena-relative XY location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FVector2D Location = FVector2D::ZeroVector;
};

namespace CRProto
{
	/** Applies a flat debug color using the engine basic shape material. */
	void ApplyColor(UPrimitiveComponent* Component, const FLinearColor& Color);

	/** Rotates a text render component so it faces the local player's camera. */
	void FaceCamera(UTextRenderComponent* Text);

	/**
	 * Portion of a frame's delta that physics actually simulates. Physics clamps each frame's
	 * step, so at low FPS game time runs ahead of the simulation; combat timers use this instead.
	 */
	float GetSimulatedDeltaSeconds(float DeltaSeconds);

	/** True when the combat GameMode is showing the developer Debug View. */
	bool IsDebugView(const UObject* WorldContext);

	FString BoundaryTypeName(ECRBoundaryType Type);
	FLinearColor BoundaryTypeColor(ECRBoundaryType Type);
	FString EliminationReasonName(ECREliminationReason Reason);
}
