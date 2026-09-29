// Player controller for the canvas-UI screens (main menu, hub): visible cursor, clicks go to the HUD.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CRUIPlayerController.generated.h"

UCLASS()
class CARDSROGUELIKE_API ACRUIPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACRUIPlayerController();

	/** Restores normal game+UI input after a Slate dialog closes. */
	void RestoreDefaultInput();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void OnLeftClick();
};
