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
	void SelectCard1() { SelectCard(0); }
	void SelectCard2() { SelectCard(1); }
	void SelectCard3() { SelectCard(2); }
	void SelectCard4() { SelectCard(3); }
	void SelectCard5() { SelectCard(4); }

	void CycleEdge1();
	void CycleEdge2();
	void CycleEdge3();

	void OnLeftPressed();
	void OnLeftReleased();
	void OnCancel();
	void OnEndTurn();
	void OnRestart();
	void OnToggleDebugView();

	int32 DraggedCard = INDEX_NONE;
};
