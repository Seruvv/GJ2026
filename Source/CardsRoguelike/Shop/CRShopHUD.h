// Prototype shop HUD on the canvas: Silver/HP, three card offers, heal service, merchant line, Leave.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CRShopHUD.generated.h"

class ACRShopGameMode;
enum class ECRShopOfferStatus : uint8;

enum class ECRShopHitTarget : uint8
{
	None,
	Offer0,
	Offer1,
	Offer2,
	Heal,
	Leave
};

UCLASS()
class CARDSROGUELIKE_API ACRShopHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** What a click at this screen position would hit (from the last drawn layout). */
	ECRShopHitTarget HitTest(const FVector2D& ScreenPos) const;

private:
	void DrawStatusPanel(const ACRShopGameMode* GM, float S);
	void DrawOffers(const ACRShopGameMode* GM, float S, const FVector2D& Mouse);
	void DrawHeal(const ACRShopGameMode* GM, float S, const FVector2D& Mouse);
	void DrawLeave(const ACRShopGameMode* GM, float S, const FVector2D& Mouse);
	/** Merchant dialogue as Canvas text over the merchant (same line as the top HUD line). */
	void DrawMerchantSpeech(const ACRShopGameMode* GM, float S);

	void DrawBox(const FBox2D& Box, const FLinearColor& Fill);
	void DrawFrame(const FBox2D& Box, const FLinearColor& Color, float Thickness);
	void DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale);
	void DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale);
	/** Scale at which Text fits MaxWidth (never larger than Scale). */
	float FitScale(const FString& Text, float Scale, float MaxWidth) const;
	static FString StatusLabel(ECRShopOfferStatus Status);

	TArray<FBox2D> OfferRects;
	FBox2D HealRect = FBox2D(ForceInit);
	FBox2D LeaveRect = FBox2D(ForceInit);
};
