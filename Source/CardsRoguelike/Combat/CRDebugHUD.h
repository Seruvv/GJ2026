// Prototype combat HUD drawn on the canvas: turn/mana, hamster panel, card hand, enemy intents.
// Debug View (F10) adds actor IDs, the event log and debug key hints.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CRDebugHUD.generated.h"

class ACRCombatGameMode;
class ACRPlayerController;
struct FCRCardDef;

UCLASS()
class CARDSROGUELIKE_API ACRDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Hand card under a screen position (last drawn layout), or INDEX_NONE. */
	int32 GetCardIndexAt(const FVector2D& ScreenPos) const;

	/** True when a screen position is over the hand area (drops there return the card). */
	bool IsOverHand(const FVector2D& ScreenPos) const;

	/** Screen rect of the End Turn button (bottom right), from the current viewport size. */
	FBox2D GetEndTurnRect() const;

	/** Reward offer under a screen position (last drawn reward panel), or INDEX_NONE. */
	int32 GetRewardIndexAt(const FVector2D& ScreenPos) const;
	bool IsOverRewardSkip(const FVector2D& ScreenPos) const;

private:
	void LayoutHand(int32 NumCards);
	void DrawTurnPanel(const ACRCombatGameMode* GM);
	void DrawHamsterPanel(const ACRCombatGameMode* GM);
	void DrawHand(const ACRCombatGameMode* GM, const ACRPlayerController* PC, bool bValidDrop, bool bCursorOverHand);
	void DrawCard(const FCRCardDef& Card, int32 Index, const FBox2D& Rect, const FLinearColor& Border, bool bAffordable, float Alpha);
	void DrawEnemyOverlays(const ACRCombatGameMode* GM);
	void DrawTargetingPreview(const ACRCombatGameMode* GM, const ACRPlayerController* PC, bool& bOutValidDrop);
	void DrawDebugExtras(const ACRCombatGameMode* GM);
	void DrawResultBanner(const ACRCombatGameMode* GM);
	void DrawEndTurnButton(const ACRCombatGameMode* GM, const FVector2D& MousePos);
	void DrawRewardPanel(const ACRCombatGameMode* GM, const FVector2D& MousePos);
	/** Russian edge / pit names at their world positions (the 3D text font has no Cyrillic). */
	void DrawArenaLabels(const ACRCombatGameMode* GM);

	// Canvas helpers
	void DrawBox(const FBox2D& Box, const FLinearColor& Fill);
	void DrawFrame(const FBox2D& Box, const FLinearColor& Color, float Thickness);
	void DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale);
	void DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale);
	/** Centered text shrunk (down to MinFactor) so it fits MaxWidth; long text wraps onto a second line. */
	void DrawTextFitted(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale, float MaxWidth, float MinFactor = 0.7f);
	/** Centered text with a dark drop shadow, for labels over the 3D arena. */
	void DrawTextShadowCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale);
	float TextWidth(const FString& Text, float Scale);
	void DrawWorldCircle(const FVector& Center, float Radius, const FLinearColor& Color, float Thickness);
	void DrawWorldArrow(const FVector& From, const FVector& To, const FLinearColor& Color, float Thickness);
	float UIScale() const;

	TArray<FBox2D> CardRects;
	FBox2D HandBounds = FBox2D(ForceInit);
	TArray<FBox2D> RewardCardRects;
	FBox2D RewardSkipRect = FBox2D(ForceInit);
};
