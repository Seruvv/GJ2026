// Sanctuary hub between runs: shows the active profile's buildings, and starts runs ("В ПОХОД").

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRHubGameMode.generated.h"

class ACRHubRoomActor;
class UCRProfileSubsystem;

UCLASS()
class CARDSROGUELIKE_API ACRHubGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRHubGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	ACRHubRoomActor* GetRoom() const { return Room; }
	UCRProfileSubsystem* GetProfiles() const;

	/** Starts a fresh run for the active profile and opens the run map. */
	bool StartRun();
	/** Back to the main menu (profile selection). */
	void OpenMainMenu();

private:
	/** Mirrors building levels into the scene. */
	void RefreshRoom();

	UPROPERTY()
	TObjectPtr<ACRHubRoomActor> Room;

	FDelegateHandle ProfileChangedHandle;
};
