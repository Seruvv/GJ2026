// Main menu / profile selection: the game's start screen. The sanctuary scene is shown as a backdrop.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRMainMenuGameMode.generated.h"

class ACRHubRoomActor;
class UCRProfileSubsystem;

UCLASS()
class CARDSROGUELIKE_API ACRMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRMainMenuGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	UCRProfileSubsystem* GetProfiles() const;

	/** Activates the profile and opens the hub. */
	bool ContinueWithProfile(const FString& ProfileId);
	/** Creates a profile, activates it and opens the hub. */
	bool CreateProfileAndEnter(const FString& DisplayName);
	void QuitGame();

private:
	void OpenHub();

	UPROPERTY()
	TObjectPtr<ACRHubRoomActor> Backdrop;
};
