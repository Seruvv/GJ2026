// Placeholder merchant: primitive body with a subtle procedural idle and a world-space dialogue line.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRShopMerchant.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CARDSROGUELIKE_API ACRShopMerchant : public AActor
{
	GENERATED_BODY()

public:
	ACRShopMerchant();

	virtual void Tick(float DeltaSeconds) override;

	void SetDialogueLine(const FString& Line);

	UPROPERTY(EditAnywhere, Category = "CR|Merchant")
	float BobHeight = 4.f;

	UPROPERTY(EditAnywhere, Category = "CR|Merchant")
	float BobSpeed = 2.f;

	UPROPERTY(EditAnywhere, Category = "CR|Merchant")
	float SwayDegrees = 5.f;

	UPROPERTY(EditAnywhere, Category = "CR|Merchant")
	float SwaySpeed = 0.8f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CR|Merchant")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Everything that bobs and sways; the dialogue line stays steady for readability. */
	UPROPERTY(VisibleAnywhere, Category = "CR|Merchant")
	TObjectPtr<USceneComponent> BodyPivot;

	UPROPERTY(VisibleAnywhere, Category = "CR|Merchant")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "CR|Merchant")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "CR|Merchant")
	TObjectPtr<UStaticMeshComponent> Hat;

	UPROPERTY(VisibleAnywhere, Category = "CR|Merchant")
	TObjectPtr<UTextRenderComponent> DialogueText;

	float IdleTime = 0.f;
};
