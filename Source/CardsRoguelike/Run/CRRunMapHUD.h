// Prototype run map HUD drawn on the canvas: run/hamster panel, carried currencies, arrival banner.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CRRunMapHUD.generated.h"

UCLASS()
class CARDSROGUELIKE_API ACRRunMapHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawPanel(float X, float Y, float W, float H);
	void DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale);
	void DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale);
};
