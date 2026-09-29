#include "CRShopHUD.h"

#include "../Combat/CRCardLibrary.h"
#include "../Run/CRRunTypes.h"
#include "CRShopGameMode.h"
#include "CRShopMerchant.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor ShopHudPanelColor(0.02f, 0.02f, 0.03f, 0.78f);
	const FLinearColor ShopHudTextColor(0.92f, 0.92f, 0.95f);
	const FLinearColor ShopHudDimTextColor(0.55f, 0.55f, 0.6f);
	const FLinearColor ShopHudSilverColor(0.85f, 0.88f, 0.95f);
	const FLinearColor ShopHudGoodColor(0.3f, 0.9f, 0.45f);
	const FLinearColor ShopHudBadColor(1.f, 0.4f, 0.3f);
}

void ACRShopHUD::DrawHUD()
{
	Super::DrawHUD();

	OfferRects.Reset();
	HealRect = FBox2D(ForceInit);
	LeaveRect = FBox2D(ForceInit);

	const ACRShopGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRShopGameMode>() : nullptr;
	if (!GM || !Canvas)
	{
		return;
	}

	const float S = Canvas->ClipY / 1080.f;
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetMousePosition(MouseX, MouseY);
	}
	const FVector2D Mouse(MouseX, MouseY);

	if (!GM->IsValidShop())
	{
		// Direct sandbox open: show the room, but nothing is for sale and no run data is touched.
		const float CenterX = Canvas->ClipX * 0.5f;
		DrawBox(FBox2D(FVector2D(CenterX - 300.f * S, 40.f * S), FVector2D(CenterX + 300.f * S, 150.f * S)), ShopHudPanelColor);
		DrawTextCentered(TEXT("ЛАВКА (ПЕСОЧНИЦА)"), FLinearColor(1.f, 0.85f, 0.35f), CenterX, 56.f * S, 2.f * S);
		DrawTextCentered(TEXT("Нет активной комнаты лавки"), ShopHudDimTextColor, CenterX, 104.f * S, 1.3f * S);
		DrawMerchantSpeech(GM, S);
		return;
	}

	DrawMerchantSpeech(GM, S);

	DrawStatusPanel(GM, S);
	DrawOffers(GM, S, Mouse);
	DrawHeal(GM, S, Mouse);
	DrawLeave(GM, S, Mouse);

	const FString Message = GM->GetActiveMessage();
	if (!Message.IsEmpty())
	{
		DrawTextCentered(Message, FLinearColor(1.f, 0.75f, 0.3f), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f + 20.f * S, 1.5f * S);
	}
}

void ACRShopHUD::DrawStatusPanel(const ACRShopGameMode* GM, float S)
{
	const float X = 28.f * S;
	float Y = 24.f * S;
	DrawBox(FBox2D(FVector2D(X - 12.f * S, Y - 10.f * S), FVector2D(X + 300.f * S, Y + 150.f * S)), ShopHudPanelColor);
	DrawTextAt(TEXT("ЛАВКА"), FLinearColor(1.f, 0.85f, 0.35f), X, Y, 2.0f * S);
	Y += 46.f * S;
	DrawTextAt(FString::Printf(TEXT("Серебро: %d"), GM->GetSilver()), ShopHudSilverColor, X, Y, 1.35f * S);
	Y += 34.f * S;
	DrawTextAt(FString::Printf(TEXT("Здоровье: %d / %d"), GM->GetHP(), GM->GetMaxHP()), ShopHudTextColor, X, Y, 1.35f * S);

	if (const FCRShopState* Shop = GM->GetShopState())
	{
		// Merchant line, top centre (the speech drawn above the merchant shows the same text).
		const float CenterX = Canvas->ClipX * 0.5f;
		DrawTextCentered(FString::Printf(TEXT("Торговец: «%s»"), *Shop->MerchantLine), FLinearColor(1.f, 0.9f, 0.65f), CenterX, 30.f * S, 1.3f * S);
	}
}

void ACRShopHUD::DrawMerchantSpeech(const ACRShopGameMode* GM, float S)
{
	// Drawn on the Canvas (Cyrillic-capable font) at the merchant's projected dialogue point; the merchant's
	// old 3D TextRender uses the default distance-field font, which has no Cyrillic glyphs.
	const ACRShopMerchant* Merchant = GM->GetMerchant();
	if (!Merchant || Merchant->GetDialogueLine().IsEmpty())
	{
		return;
	}
	const FVector Screen = Project(Merchant->GetDialogueWorldLocation());
	if (Screen.Z <= 0.f)
	{
		return;
	}
	const FString Line = FString::Printf(TEXT("«%s»"), *Merchant->GetDialogueLine());
	const float Scale = FitScale(Line, 2.0f * S, Canvas->ClipX * 0.6f);
	const float Offset = FMath::Max(1.f, 2.f * S);
	DrawTextCentered(Line, FLinearColor(0.f, 0.f, 0.f, 0.85f), Screen.X + Offset, Screen.Y - 16.f * S + Offset, Scale);
	DrawTextCentered(Line, FLinearColor(1.f, 0.88f, 0.59f), Screen.X, Screen.Y - 16.f * S, Scale);
}

void ACRShopHUD::DrawOffers(const ACRShopGameMode* GM, float S, const FVector2D& Mouse)
{
	const FCRShopState* Shop = GM->GetShopState();
	if (!Shop)
	{
		return;
	}

	const int32 Count = Shop->OfferCardIds.Num();
	const float CardW = 170.f * S;
	const float CardH = 230.f * S;
	const float Gap = 26.f * S;
	const float RowW = Count * CardW + FMath::Max(0, Count - 1) * Gap;
	const float X0 = Canvas->ClipX * 0.5f - RowW * 0.5f - 110.f * S;
	const float Top = Canvas->ClipY - CardH - 40.f * S;

	for (int32 i = 0; i < Count; ++i)
	{
		FBox2D Rect(FVector2D(X0 + i * (CardW + Gap), Top), FVector2D(X0 + i * (CardW + Gap) + CardW, Top + CardH));
		OfferRects.Add(Rect);

		const ECRShopOfferStatus Status = GM->GetCardStatus(i);
		const bool bAvailable = Status == ECRShopOfferStatus::Available;
		const bool bHovered = bAvailable && Rect.IsInside(Mouse);
		if (bHovered)
		{
			Rect = Rect.ShiftBy(FVector2D(0.f, -12.f * S));
		}

		const FLinearColor Fill = bAvailable ? FLinearColor(0.12f, 0.12f, 0.17f, 0.95f) : FLinearColor(0.07f, 0.07f, 0.08f, 0.9f);
		const FLinearColor Border = bHovered ? FLinearColor::White : (bAvailable ? FLinearColor(0.45f, 0.45f, 0.55f) : FLinearColor(0.22f, 0.22f, 0.24f));
		DrawBox(Rect, Fill);
		DrawFrame(Rect, Border, 3.f * S);

		const float CenterX = Rect.GetCenter().X;
		const FCRCardDef* Card = CRCardLibrary::FindCard(Shop->OfferCardIds[i]);
		const FLinearColor NameColor = bAvailable ? ShopHudTextColor : ShopHudDimTextColor;

		// Combat mana cost (small badge) is shown for information; the shop price is separate.
		if (Card)
		{
			const FBox2D Badge(Rect.Min + FVector2D(8.f * S), Rect.Min + FVector2D(8.f * S) + FVector2D(30.f * S));
			DrawBox(Badge, FLinearColor(0.25f, 0.6f, 1.f, bAvailable ? 1.f : 0.4f));
			DrawTextCentered(FString::FromInt(Card->ManaCost), FLinearColor::White, Badge.GetCenter().X, Badge.Min.Y + 3.f * S, 1.2f * S);
			// Card titles/descriptions (Russian) can be wider than the offer card: shrink them to fit.
			const float MaxW = CardW - 14.f * S;
			DrawTextCentered(Card->Name, NameColor, CenterX, Rect.Min.Y + 60.f * S, FitScale(Card->Name, 1.6f * S, MaxW));
			DrawTextCentered(Card->ShortText, bAvailable ? FLinearColor(0.75f, 0.8f, 0.9f) : ShopHudDimTextColor, CenterX, Rect.Min.Y + 100.f * S, FitScale(Card->ShortText, 1.05f * S, MaxW));
		}
		else
		{
			DrawTextCentered(Shop->OfferCardIds[i].ToString(), NameColor, CenterX, Rect.Min.Y + 60.f * S, 1.4f * S);
		}

		DrawTextCentered(FString::Printf(TEXT("%d серебра"), GM->CardPrice), bAvailable ? ShopHudSilverColor : ShopHudDimTextColor, CenterX, Rect.Max.Y - 72.f * S, 1.3f * S);
		const FString Label = StatusLabel(Status);
		if (!Label.IsEmpty())
		{
			DrawTextCentered(Label, Status == ECRShopOfferStatus::Sold ? ShopHudGoodColor : ShopHudBadColor, CenterX, Rect.Max.Y - 42.f * S, FitScale(Label, 1.2f * S, CardW - 14.f * S));
		}
		DrawTextCentered(FString::Printf(TEXT("[%d]"), i + 1), ShopHudDimTextColor, CenterX, Rect.Max.Y - 20.f * S, 0.9f * S);
	}
}

void ACRShopHUD::DrawHeal(const ACRShopGameMode* GM, float S, const FVector2D& Mouse)
{
	if (OfferRects.Num() == 0)
	{
		return;
	}

	const FVector2D Size(200.f * S, 150.f * S);
	const FVector2D Min(OfferRects.Last().Max.X + 40.f * S, OfferRects.Last().Max.Y - Size.Y);
	HealRect = FBox2D(Min, Min + Size);

	const ECRShopOfferStatus Status = GM->GetHealStatus();
	const bool bAvailable = Status == ECRShopOfferStatus::Available;
	const bool bHovered = bAvailable && HealRect.IsInside(Mouse);
	DrawBox(HealRect, bAvailable ? (bHovered ? FLinearColor(0.15f, 0.42f, 0.22f, 0.95f) : FLinearColor(0.1f, 0.3f, 0.16f, 0.95f)) : FLinearColor(0.07f, 0.07f, 0.08f, 0.9f));
	DrawFrame(HealRect, bHovered ? FLinearColor::White : (bAvailable ? ShopHudGoodColor : FLinearColor(0.22f, 0.22f, 0.24f)), 3.f * S);

	const float CenterX = HealRect.GetCenter().X;
	DrawTextCentered(FString::Printf(TEXT("ЛЕЧЕНИЕ +%d"), GM->HealAmount), bAvailable ? ShopHudTextColor : ShopHudDimTextColor, CenterX, HealRect.Min.Y + 16.f * S, 1.45f * S);
	DrawTextCentered(FString::Printf(TEXT("%d серебра"), GM->HealPrice), bAvailable ? ShopHudSilverColor : ShopHudDimTextColor, CenterX, HealRect.Min.Y + 56.f * S, 1.3f * S);
	const FString Label = StatusLabel(Status);
	if (!Label.IsEmpty())
	{
		DrawTextCentered(Label, Status == ECRShopOfferStatus::Sold ? ShopHudGoodColor : ShopHudBadColor, CenterX, HealRect.Min.Y + 90.f * S, FitScale(Label, 1.2f * S, Size.X - 14.f * S));
	}
	DrawTextCentered(TEXT("[4]"), ShopHudDimTextColor, CenterX, HealRect.Max.Y - 22.f * S, 0.9f * S);
}

void ACRShopHUD::DrawLeave(const ACRShopGameMode* GM, float S, const FVector2D& Mouse)
{
	const FVector2D Size(220.f * S, 64.f * S);
	const FVector2D Min(Canvas->ClipX - Size.X - 28.f * S, Canvas->ClipY - Size.Y - 28.f * S);
	LeaveRect = FBox2D(Min, Min + Size);

	const bool bActive = !GM->IsLeaving();
	const bool bHovered = bActive && LeaveRect.IsInside(Mouse);
	DrawBox(LeaveRect, bActive ? (bHovered ? FLinearColor(0.55f, 0.35f, 0.15f, 0.95f) : FLinearColor(0.42f, 0.26f, 0.1f, 0.92f)) : FLinearColor(0.08f, 0.08f, 0.09f, 0.8f));
	DrawFrame(LeaveRect, bHovered ? FLinearColor::White : FLinearColor(0.9f, 0.7f, 0.4f), 3.f * S);
	const FString LeaveText = TEXT("УЙТИ ИЗ ЛАВКИ");
	DrawTextCentered(LeaveText, bActive ? ShopHudTextColor : ShopHudDimTextColor, LeaveRect.GetCenter().X, Min.Y + 8.f * S, FitScale(LeaveText, 1.5f * S, Size.X - 16.f * S));
	DrawTextCentered(TEXT("Пробел"), ShopHudDimTextColor, LeaveRect.GetCenter().X, Min.Y + 38.f * S, 1.0f * S);
}

ECRShopHitTarget ACRShopHUD::HitTest(const FVector2D& ScreenPos) const
{
	for (int32 i = 0; i < OfferRects.Num() && i < 3; ++i)
	{
		if (OfferRects[i].IsInside(ScreenPos))
		{
			return static_cast<ECRShopHitTarget>(static_cast<uint8>(ECRShopHitTarget::Offer0) + i);
		}
	}
	if (HealRect.bIsValid && HealRect.IsInside(ScreenPos))
	{
		return ECRShopHitTarget::Heal;
	}
	if (LeaveRect.bIsValid && LeaveRect.IsInside(ScreenPos))
	{
		return ECRShopHitTarget::Leave;
	}
	return ECRShopHitTarget::None;
}

FString ACRShopHUD::StatusLabel(ECRShopOfferStatus Status)
{
	switch (Status)
	{
	case ECRShopOfferStatus::Sold:            return TEXT("ПРОДАНО");
	case ECRShopOfferStatus::NotEnoughSilver: return TEXT("Недостаточно серебра");
	case ECRShopOfferStatus::FullHP:          return TEXT("Здоровье уже полное");
	default:                                  return FString();
	}
}

void ACRShopHUD::DrawBox(const FBox2D& Box, const FLinearColor& Fill)
{
	const FVector2D Size = Box.GetSize();
	DrawRect(Fill, Box.Min.X, Box.Min.Y, Size.X, Size.Y);
}

void ACRShopHUD::DrawFrame(const FBox2D& Box, const FLinearColor& Color, float Thickness)
{
	const FVector2D Size = Box.GetSize();
	DrawRect(Color, Box.Min.X, Box.Min.Y, Size.X, Thickness);
	DrawRect(Color, Box.Min.X, Box.Max.Y - Thickness, Size.X, Thickness);
	DrawRect(Color, Box.Min.X, Box.Min.Y, Thickness, Size.Y);
	DrawRect(Color, Box.Max.X - Thickness, Box.Min.Y, Thickness, Size.Y);
}

void ACRShopHUD::DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale)
{
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
}

float ACRShopHUD::FitScale(const FString& Text, float Scale, float MaxWidth) const
{
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetMediumFont(), Scale);
	return W > MaxWidth && W > 0.f ? Scale * MaxWidth / W : Scale;
}

void ACRShopHUD::DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale)
{
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}
