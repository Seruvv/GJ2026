// Shared canvas UI for the out-of-run screens (main menu, hub): scaled drawing helpers, buttons that are
// re-registered every frame (so hit testing always matches what is drawn) and a short feedback message.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CRMenuHUDBase.generated.h"

/** A clickable rectangle drawn this frame. Action/Arg tell the owning HUD what to do. */
struct FCRUIButton
{
	FBox2D Rect = FBox2D(ForceInit);
	FName Action;
	FString Arg;
	bool bEnabled = true;
};

enum class ECRUIButtonStyle : uint8
{
	Normal,
	Primary,
	Danger,
	Subtle
};

UCLASS(Abstract)
class CARDSROGUELIKE_API ACRMenuHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Called by the UI player controller on a left click. Returns true if a button handled it. */
	bool HandleClick(const FVector2D& ScreenPos);

	/** Mouse wheel (Delta > 0 = up) at ScreenPos. Returns true if handled. */
	virtual bool HandleScroll(const FVector2D& ScreenPos, float Delta) { return false; }

	/** Short centered feedback line ("Мастерская: уровень 2"), fades after a few seconds. */
	void ShowMessage(const FString& Message, bool bError = false);

protected:
	/** Draw the screen; register buttons with DrawButton. */
	virtual void DrawScreen() {}
	/** A registered, enabled button was clicked. */
	virtual void OnButton(const FCRUIButton& Button) {}

	/** UI scale (1 at 1080p). */
	float S = 1.f;
	FVector2D Mouse = FVector2D::ZeroVector;

	void DrawBox(const FBox2D& Box, const FLinearColor& Fill);
	void DrawFrame(const FBox2D& Box, const FLinearColor& Color, float Thickness);
	void DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale);
	void DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale);
	void DrawTextShadowCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale);
	float TextWidth(const FString& Text, float Scale);
	float TextHeight(float Scale);
	/** Scale at which Text fits MaxWidth (never larger than Scale). */
	float FitScale(const FString& Text, float Scale, float MaxWidth);
	/** Word-wrapped text; returns the Y below the last line. */
	float DrawWrapped(const FString& Text, const FLinearColor& Color, float X, float Y, float MaxWidth, float Scale);

	/** Registers a clickable area drawn by the caller. Returns true if the mouse is over it (and it is enabled). */
	bool RegisterButton(const FBox2D& Rect, FName Action, const FString& Arg = FString(), bool bEnabled = true);

	/** Draws and registers a button. Returns true if the mouse is over it (and it is enabled). */
	bool DrawButton(const FBox2D& Rect, const FString& Label, FName Action, const FString& Arg = FString(), bool bEnabled = true,
		ECRUIButtonStyle Style = ECRUIButtonStyle::Normal, const FString& SubLabel = FString());

	void DrawMessage(float CenterY);

private:
	TArray<FCRUIButton> Buttons;
	FString Message;
	bool bMessageError = false;
	double MessageTime = -100.0;
};
