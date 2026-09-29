#include "CRTypes.h"

#include "Camera/PlayerCameraManager.h"
#include "CRCombatGameMode.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsSettings.h"

namespace CRProto
{
	void ApplyColor(UPrimitiveComponent* Component, const FLinearColor& Color)
	{
		if (!Component)
		{
			return;
		}

		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Base)
		{
			return;
		}

		if (UMaterialInstanceDynamic* MID = Component->CreateDynamicMaterialInstance(0, Base))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	void FaceCamera(UTextRenderComponent* Text)
	{
		if (!Text)
		{
			return;
		}

		if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(Text, 0))
		{
			const FVector ToCamera = CameraManager->GetCameraLocation() - Text->GetComponentLocation();
			if (!ToCamera.IsNearlyZero())
			{
				// Text renders facing its +X axis.
				Text->SetWorldRotation(ToCamera.Rotation());
			}
		}
	}

	float GetSimulatedDeltaSeconds(float DeltaSeconds)
	{
		const UPhysicsSettings* Settings = UPhysicsSettings::Get();
		const float MaxStep = Settings->bSubstepping
			? Settings->MaxSubstepDeltaTime * Settings->MaxSubsteps
			: Settings->MaxPhysicsDeltaTime;
		return FMath::Min(DeltaSeconds, MaxStep);
	}

	bool IsDebugView(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const ACRCombatGameMode* GM = World ? World->GetAuthGameMode<ACRCombatGameMode>() : nullptr;
		return GM && GM->IsDebugView();
	}

	FString BoundaryTypeName(ECRBoundaryType Type)
	{
		switch (Type)
		{
		case ECRBoundaryType::Normal:        return TEXT("NORMAL");
		case ECRBoundaryType::Rubber:        return TEXT("RUBBER");
		case ECRBoundaryType::Void:          return TEXT("VOID");
		case ECRBoundaryType::TeleportInner: return TEXT("TELEPORT");
		}
		return TEXT("?");
	}

	FString BoundaryTypeDisplayName(ECRBoundaryType Type)
	{
		switch (Type)
		{
		case ECRBoundaryType::Normal:        return TEXT("ОБЫЧНАЯ");
		case ECRBoundaryType::Rubber:        return TEXT("ОТСКАКИВАЮЩАЯ");
		case ECRBoundaryType::Void:          return TEXT("ПАДЕНИЕ ЗА КРАЙ");
		case ECRBoundaryType::TeleportInner: return TEXT("ТЕЛЕПОРТ");
		}
		return TEXT("?");
	}

	FLinearColor BoundaryTypeColor(ECRBoundaryType Type)
	{
		switch (Type)
		{
		case ECRBoundaryType::Normal:        return FLinearColor(0.45f, 0.45f, 0.45f);
		case ECRBoundaryType::Rubber:        return FLinearColor(0.1f, 0.85f, 0.15f);
		case ECRBoundaryType::Void:          return FLinearColor(0.35f, 0.0f, 0.5f);
		case ECRBoundaryType::TeleportInner: return FLinearColor(0.0f, 0.6f, 1.0f);
		}
		return FLinearColor::White;
	}

	FString EliminationReasonName(ECREliminationReason Reason)
	{
		switch (Reason)
		{
		case ECREliminationReason::Damage: return TEXT("killed");
		case ECREliminationReason::Void:   return TEXT("fell into the VOID");
		case ECREliminationReason::Pit:    return TEXT("fell into the PIT");
		case ECREliminationReason::Fell:   return TEXT("fell off the arena");
		}
		return TEXT("eliminated");
	}
}
