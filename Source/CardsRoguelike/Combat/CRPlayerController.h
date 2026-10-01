// Prototype combat input: drag cards from the hand into the arena, number keys as fallback.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CRPlayerController.generated.h"

class ACRCombatGameMode;
class ACRDebugHUD;

UCLASS()
class CARDSROGUELIKE_API ACRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACRPlayerController();

	/** Card currently being dragged from the hand, or INDEX_NONE. */
	int32 GetDraggedCard() const { return DraggedCard; }

	/** World hit under the mouse cursor (arena floor, enemies, hamster...). */
	bool GetCursorWorldHit(FHitResult& OutHit) const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	ACRCombatGameMode* GetCombatMode() const;
	ACRDebugHUD* GetCombatHUD() const;
	bool IsCursorOverHand() const;

	void SelectCard(int32 Index);

	/**
	 * Number keys 1-9: select hand card Digit-1 (missing cards are ignored). In Debug View only, Shift+7/8/9
	 * cycles arena edges 1-3 instead; that developer tool is never reachable in normal play.
	 */
	void OnNumberKey(int32 Digit);
	bool IsShiftDown() const;

	void OnLeftPressed();
	void OnLeftReleased();
	void OnCancel();
	void OnEndTurn();
	void OnRestart();
	void OnToggleDebugView();

	int32 DraggedCard = INDEX_NONE;
};
