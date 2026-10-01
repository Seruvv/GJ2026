// Prototype run map HUD drawn on the canvas: run/hamster panel, carried currencies, arrival banner.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CRRunMapHUD.generated.h"

/** Run map buttons shown for runs started from a profile (hub). */
enum class ECRRunMapButton : uint8
{
	None,
	/** The run ended (Return reached or hamster dead): go back to the sanctuary. */
	ReturnToHub,
	/** Leave the run early (asks for a second click; carried loot is lost). */
	LeaveRun
};

UCLASS()
class CARDSROGUELIKE_API ACRRunMapHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Which button (drawn last frame) is at ScreenPos. */
	ECRRunMapButton HitTest(const FVector2D& ScreenPos) const;

	/** First click on "leave" arms it; returns true if it was already armed (confirmed). */
	bool ConfirmLeave();

private:
	void DrawButtonBox(const FBox2D& Rect, const FString& Label, const FString& SubLabel, bool bPrimary, float S);
	void DrawProfileRunControls(const struct FCRRunState& State, float S);

	FBox2D ReturnRect = FBox2D(ForceInit);
	FBox2D LeaveRect = FBox2D(ForceInit);
	double LeaveArmedTime = -100.0;

	void DrawPanel(float X, float Y, float W, float H);
	void DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale);
	void DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale);
};
