// Event input: mouse clicks on choices / cards / buttons, 1-9 pick a choice, Space continues from the
// result, mouse wheel (or PageUp/PageDown) scrolls long text, Backspace backs out of card selection.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CREventPlayerController.generated.h"

class ACREventGameMode;
class ACREventHUD;

UCLASS()
class CARDSROGUELIKE_API ACREventPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACREventPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	ACREventGameMode* GetEventMode() const;
	ACREventHUD* GetEventHUD() const;

	void OnLeftClick();
	void OnChooseKey(FKey Key);
	void OnContinueKey();
	void OnBackKey();
	void OnScrollUp();
	void OnScrollDown();
	void OnPageUp();
	void OnPageDown();
};
