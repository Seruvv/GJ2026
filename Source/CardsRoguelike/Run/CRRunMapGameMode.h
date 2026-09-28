// Run map GameMode: ensures a run exists, spawns the map diorama and points the player at its camera.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRRunMapGameMode.generated.h"

class ACRRunMapActor;

UCLASS()
class CARDSROGUELIKE_API ACRRunMapGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRRunMapGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	ACRRunMapActor* GetMapActor() const { return MapActor; }

private:
	UPROPERTY()
	TObjectPtr<ACRRunMapActor> MapActor;
};
