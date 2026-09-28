// Narrative event HUD on the canvas: a large word-wrapped text panel (title, body, choices), the card
// sacrifice grid, and the result screen with Continue. Long text scrolls with the mouse wheel.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CREventHUD.generated.h"

class ACREventGameMode;
class UFont;
enum class ECREventScreen : uint8;

enum class ECREventHitKind : uint8
{
	None,
	Choice,
	Card,
	Back,
	Continue
};

struct FCREventHit
{
	ECREventHitKind Kind = ECREventHitKind::None;
	/** Choice index or deck index. */
	int32 Index = INDEX_NONE;
};

UCLASS()
class CARDSROGUELIKE_API ACREventHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** What a click at this screen position would hit (from the last drawn layout). */
	FCREventHit HitTest(const FVector2D& ScreenPos) const;

	/** Scrolls the active text region by a number of lines (positive = down). */
	void Scroll(float Lines);

private:
	struct FTextLine
	{
		FString Text;
		FLinearColor Color;
		float Scale = 1.f;
	};

	void DrawChoicesScreen(const ACREventGameMode* GM, const FBox2D& Content, float S, const FVector2D& Mouse);
	void DrawCardSelectScreen(const ACREventGameMode* GM, const FBox2D& Content, float S, const FVector2D& Mouse);
	void DrawResultScreen(const ACREventGameMode* GM, const FBox2D& Content, float S, const FVector2D& Mouse);
	void DrawStatus(const ACREventGameMode* GM, float S);

	/** Draws wrapped lines inside a vertical region with the current scroll offset; returns overflow height. */
	float DrawScrollRegion(const TArray<FTextLine>& Lines, const FBox2D& Region, float S);
	void AppendWrapped(TArray<FTextLine>& Out, const FString& Text, const FLinearColor& Color, float Scale, float MaxWidth);

	/** Word wrap (paragraph and blank-line aware; overlong words are split). Cached per text/width/scale. */
	const TArray<FString>& WrapText(const FString& Text, float Scale, float MaxWidth);
	float LineHeight(float Scale) const;
	float TextWidth(const FString& Text, float Scale) const;
	UFont* GetFont() const;

	void DrawBox(const FBox2D& Box, const FLinearColor& Fill);
	void DrawFrame(const FBox2D& Box, const FLinearColor& Color, float Thickness);
	void DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale);
	void DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale);
	void AddHit(const FBox2D& Box, ECREventHitKind Kind, int32 Index = INDEX_NONE);

	TArray<TPair<FBox2D, FCREventHit>> Hits;
	TMap<FString, TArray<FString>> WrapCache;

	float ScrollOffset = 0.f;
	float ScrollLineStep = 30.f;
	uint8 LastScreen = 255;
};
