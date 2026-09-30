#include "CRDebugHUD.h"

#include "CRBoundarySegment.h"
#include "CRCombatGameMode.h"
#include "CREnemy.h"
#include "CRHamster.h"
#include "CRPit.h"
#include "CRPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	const FLinearColor PanelColor(0.02f, 0.02f, 0.03f, 0.72f);
	const FLinearColor TextColor(0.92f, 0.92f, 0.95f);
	const FLinearColor DimTextColor(0.55f, 0.55f, 0.6f);
	const FLinearColor ValidColor(0.25f, 1.f, 0.55f);
	const FLinearColor InvalidColor(1.f, 0.25f, 0.2f);
	const FLinearColor ManaColor(0.25f, 0.6f, 1.f);
}

float ACRDebugHUD::UIScale() const
{
	return Canvas ? Canvas->ClipY / 1080.f : 1.f;
}

void ACRDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	const ACRCombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRCombatGameMode>() : nullptr;
	const ACRPlayerController* PC = Cast<ACRPlayerController>(GetOwningPlayerController());
	if (!GM || !PC || !Canvas)
	{
		return;
	}

	LayoutHand(GM->Cards.Num());

	float MouseX = 0.f;
	float MouseY = 0.f;
	PC->GetMousePosition(MouseX, MouseY);
	// The End Turn button is treated like the hand: no world targeting preview over it.
	const bool bCursorOverHand = IsOverHand(FVector2D(MouseX, MouseY)) || GetEndTurnRect().IsInside(FVector2D(MouseX, MouseY));

	const bool bReward = GM->GetTurnState() == ECRTurnState::Reward;
	RewardCardRects.Reset();
	RewardSkipRect = FBox2D(ForceInit);

	// World labels (edge names, pit, enemy intents) only while the fight is on; never over the Victory,
	// Defeat or Reward overlays.
	const ECRTurnState TurnState = GM->GetTurnState();
	const bool bFightActive = TurnState == ECRTurnState::PlayerTurn || TurnState == ECRTurnState::ResolvingCard || TurnState == ECRTurnState::EnemyTurn;
	if (bFightActive)
	{
		DrawArenaLabels(GM);
		DrawEnemyOverlays(GM);
	}

	bool bValidDrop = false;
	if (!bCursorOverHand && !bReward)
	{
		DrawTargetingPreview(GM, PC, bValidDrop);
	}

	DrawTurnPanel(GM);
	DrawHamsterPanel(GM);

	if (bReward)
	{
		// The reward panel replaces the hand and End Turn button.
		DrawRewardPanel(GM, FVector2D(MouseX, MouseY));
	}
	else
	{
		DrawHand(GM, PC, bValidDrop, bCursorOverHand);
		DrawEndTurnButton(GM, FVector2D(MouseX, MouseY));
	}

	if (GM->IsDebugView())
	{
		DrawDebugExtras(GM);
	}

	DrawResultBanner(GM);
}

int32 ACRDebugHUD::GetRewardIndexAt(const FVector2D& ScreenPos) const
{
	for (int32 i = 0; i < RewardCardRects.Num(); ++i)
	{
		if (RewardCardRects[i].IsInside(ScreenPos))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool ACRDebugHUD::IsOverRewardSkip(const FVector2D& ScreenPos) const
{
	return RewardSkipRect.bIsValid && RewardSkipRect.IsInside(ScreenPos);
}

void ACRDebugHUD::DrawRewardPanel(const ACRCombatGameMode* GM, const FVector2D& MousePos)
{
	const float S = UIScale();
	const float CenterX = Canvas->ClipX * 0.5f;
	const TArray<FName>& Offers = GM->GetRewardOffers();
	const bool bCommitted = GM->IsRewardCommitted();

	const float CardW = 170.f * S;
	const float CardH = 226.f * S;
	const float Gap = 28.f * S;
	const float RowW = Offers.Num() * CardW + FMath::Max(0, Offers.Num() - 1) * Gap;
	const float PanelW = FMath::Max(RowW + 80.f * S, 640.f * S);
	const float Top = Canvas->ClipY * 0.2f;
	const float PanelH = 470.f * S;
	DrawBox(FBox2D(FVector2D(CenterX - PanelW * 0.5f, Top), FVector2D(CenterX + PanelW * 0.5f, Top + PanelH)), FLinearColor(0.02f, 0.02f, 0.03f, 0.88f));

	DrawTextCentered(TEXT("НАГРАДА"), FLinearColor(1.f, 0.85f, 0.35f), CenterX, Top + 18.f * S, 2.2f * S);
	DrawTextCentered(bCommitted ? TEXT("Возвращение на карту...") : TEXT("Выберите одну карту"), TextColor, CenterX, Top + 72.f * S, 1.3f * S);

	const float RowTop = Top + 112.f * S;
	const float X0 = CenterX - RowW * 0.5f;
	for (int32 i = 0; i < Offers.Num(); ++i)
	{
		FBox2D Rect(FVector2D(X0 + i * (CardW + Gap), RowTop), FVector2D(X0 + i * (CardW + Gap) + CardW, RowTop + CardH));
		RewardCardRects.Add(Rect);

		const FCRCardDef* Card = GM->FindCardDef(Offers[i]);
		if (!Card)
		{
			continue;
		}
		const bool bChosen = bCommitted && GM->GetChosenReward() == Offers[i];
		const bool bHovered = !bCommitted && Rect.IsInside(MousePos);
		FLinearColor Border(0.4f, 0.4f, 0.5f);
		if (bHovered)
		{
			Rect = Rect.ShiftBy(FVector2D(0.f, -12.f * S));
			Border = FLinearColor(0.95f, 0.95f, 1.f);
		}
		if (bChosen)
		{
			Border = ValidColor;
		}
		const float Alpha = (bCommitted && !bChosen) ? 0.35f : 1.f;
		DrawCard(*Card, i, Rect, Border, true, Alpha);
	}

	// Skip button.
	const FVector2D SkipSize(240.f * S, 52.f * S);
	const FVector2D SkipMin(CenterX - SkipSize.X * 0.5f, RowTop + CardH + 26.f * S);
	RewardSkipRect = FBox2D(SkipMin, SkipMin + SkipSize);
	const bool bSkipHovered = !bCommitted && RewardSkipRect.IsInside(MousePos);
	const bool bSkipped = bCommitted && GM->GetChosenReward().IsNone();
	DrawBox(RewardSkipRect, bSkipHovered ? FLinearColor(0.3f, 0.3f, 0.35f, 0.95f) : FLinearColor(0.15f, 0.15f, 0.18f, 0.92f));
	DrawFrame(RewardSkipRect, bSkipped ? ValidColor : (bSkipHovered ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.55f)), 3.f * S);
	DrawTextCentered(TEXT("ПРОПУСТИТЬ"), bCommitted && !bSkipped ? DimTextColor : TextColor, CenterX, SkipMin.Y + 10.f * S, 1.5f * S);

	DrawTextCentered(FString::Printf(TEXT("Серебро +%d     Еда +%d     Дерево +%d"), GM->RewardSilver, GM->RewardFood, GM->RewardWood),
		FLinearColor(0.85f, 0.9f, 0.7f), CenterX, RewardSkipRect.Max.Y + 18.f * S, 1.3f * S);
}

// ---------------------------------------------------------------------------
// Layout

void ACRDebugHUD::LayoutHand(int32 NumCards)
{
	const float S = UIScale();
	const float CardW = 150.f * S;
	const float CardH = 200.f * S;
	const float Gap = 18.f * S;
	const float TotalW = NumCards * CardW + FMath::Max(0, NumCards - 1) * Gap;
	const float X0 = (Canvas->ClipX - TotalW) * 0.5f;
	const float Y0 = Canvas->ClipY - CardH - 28.f * S;

	CardRects.Reset();
	HandBounds = FBox2D(ForceInit);
	for (int32 i = 0; i < NumCards; ++i)
	{
		const FVector2D Min(X0 + i * (CardW + Gap), Y0);
		CardRects.Add(FBox2D(Min, Min + FVector2D(CardW, CardH)));
		HandBounds += CardRects.Last();
	}
	if (NumCards > 0)
	{
		HandBounds = HandBounds.ExpandBy(FVector2D(12.f * S, 24.f * S));
	}
}

int32 ACRDebugHUD::GetCardIndexAt(const FVector2D& ScreenPos) const
{
	for (int32 i = 0; i < CardRects.Num(); ++i)
	{
		if (CardRects[i].IsInside(ScreenPos))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool ACRDebugHUD::IsOverHand(const FVector2D& ScreenPos) const
{
	return HandBounds.bIsValid && HandBounds.IsInside(ScreenPos);
}

FBox2D ACRDebugHUD::GetEndTurnRect() const
{
	int32 ViewW = 0;
	int32 ViewH = 0;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetViewportSize(ViewW, ViewH);
	}
	if (ViewW <= 0 || ViewH <= 0)
	{
		return FBox2D(ForceInit);
	}

	const float S = ViewH / 1080.f;
	const FVector2D Size(260.f * S, 64.f * S);
	const FVector2D Min(ViewW - 28.f * S - Size.X, ViewH - 28.f * S - Size.Y);
	return FBox2D(Min, Min + Size);
}

void ACRDebugHUD::DrawEndTurnButton(const ACRCombatGameMode* GM, const FVector2D& MousePos)
{
	const FBox2D Rect = GetEndTurnRect();
	if (!Rect.bIsValid)
	{
		return;
	}

	const float S = UIScale();
	const bool bActive = GM->CanEndTurn();
	const bool bHovered = bActive && Rect.IsInside(MousePos);

	const FLinearColor Fill = !bActive ? FLinearColor(0.08f, 0.08f, 0.09f, 0.8f)
		: (bHovered ? FLinearColor(0.2f, 0.62f, 0.3f, 0.95f) : FLinearColor(0.13f, 0.45f, 0.22f, 0.92f));
	const FLinearColor Border = !bActive ? FLinearColor(0.22f, 0.22f, 0.24f) : (bHovered ? FLinearColor::White : FLinearColor(0.6f, 0.9f, 0.65f));
	const FLinearColor Label = bActive ? FLinearColor::White : DimTextColor;

	DrawBox(Rect, Fill);
	DrawFrame(Rect, Border, 3.f * S);
	const float CenterX = Rect.GetCenter().X;
	DrawTextFitted(TEXT("ЗАВЕРШИТЬ ХОД"), Label, CenterX, Rect.Min.Y + 8.f * S, 1.5f * S, Rect.GetSize().X - 16.f * S);
	DrawTextCentered(TEXT("Пробел"), bActive ? FLinearColor(0.8f, 0.9f, 0.8f) : DimTextColor, CenterX, Rect.Min.Y + 38.f * S, 1.0f * S);
}

// ---------------------------------------------------------------------------
// Panels

void ACRDebugHUD::DrawTurnPanel(const ACRCombatGameMode* GM)
{
	const float S = UIScale();
	const float X = 28.f * S;
	float Y = 24.f * S;

	FLinearColor StateColor = TextColor;
	switch (GM->GetTurnState())
	{
	case ECRTurnState::PlayerTurn:    StateColor = FLinearColor(0.45f, 1.f, 0.55f); break;
	case ECRTurnState::ResolvingCard: StateColor = FLinearColor(1.f, 0.85f, 0.3f); break;
	case ECRTurnState::EnemyTurn:     StateColor = FLinearColor(1.f, 0.4f, 0.35f); break;
	default: break;
	}

	DrawBox(FBox2D(FVector2D(X - 12.f * S, Y - 10.f * S), FVector2D(X + 330.f * S, Y + 138.f * S)), PanelColor);
	DrawTextAt(GM->GetTurnStateName(), StateColor, X, Y, 2.0f * S);
	Y += 44.f * S;
	DrawTextAt(FString::Printf(TEXT("Ход %d"), GM->GetTurnNumber()), TextColor, X, Y, 1.3f * S);
	Y += 34.f * S;

	DrawTextAt(TEXT("Мана"), TextColor, X, Y + 2.f * S, 1.3f * S);
	const float PipSize = 22.f * S;
	for (int32 i = 0; i < GM->ManaPerTurn; ++i)
	{
		const FVector2D Min(X + 80.f * S + i * (PipSize + 10.f * S), Y + 4.f * S);
		const FBox2D Pip(Min, Min + FVector2D(PipSize));
		if (i < GM->GetMana())
		{
			DrawBox(Pip, ManaColor);
		}
		DrawFrame(Pip, ManaColor, 2.f * S);
	}
	Y += 56.f * S;

	// Temporary feedback and the current targeting instruction, under the panel.
	const FString Message = GM->GetActiveMessage();
	if (!Message.IsEmpty())
	{
		DrawTextAt(Message, FLinearColor(1.f, 0.65f, 0.25f), X, Y, 1.3f * S);
		Y += 30.f * S;
	}
	const FString Prompt = GM->GetTargetingPrompt();
	if (!Prompt.IsEmpty())
	{
		DrawTextAt(Prompt, FLinearColor(0.55f, 0.95f, 1.f), X, Y, 1.2f * S);
	}
}

void ACRDebugHUD::DrawHamsterPanel(const ACRCombatGameMode* GM)
{
	const ACRHamster* Hamster = GM->GetHamster();
	if (!Hamster)
	{
		return;
	}

	const float S = UIScale();
	const float W = 340.f * S;
	const float X = Canvas->ClipX - W - 28.f * S;
	float Y = 24.f * S;

	const FString Name = GM->GetHamsterDisplayName();
	DrawBox(FBox2D(FVector2D(X - 12.f * S, Y - 10.f * S), FVector2D(X + W, Y + (Name.IsEmpty() ? 128.f : 158.f) * S)), PanelColor);
	DrawTextAt(TEXT("ХОМЯК"), FLinearColor(1.f, 0.75f, 0.35f), X, Y, 1.4f * S);
	Y += 34.f * S;
	if (!Name.IsEmpty())
	{
		DrawTextAt(Name, TextColor, X, Y, 1.3f * S);
		Y += 30.f * S;
	}

	const float Ratio = Hamster->GetMaxHP() > 0 ? FMath::Clamp(float(Hamster->GetHP()) / Hamster->GetMaxHP(), 0.f, 1.f) : 0.f;
	const FLinearColor BarColor = Ratio > 0.5f ? FLinearColor(0.3f, 0.85f, 0.35f) : (Ratio > 0.25f ? FLinearColor(0.95f, 0.75f, 0.2f) : FLinearColor(0.95f, 0.2f, 0.15f));
	const FBox2D Bar(FVector2D(X, Y), FVector2D(X + W - 24.f * S, Y + 22.f * S));
	DrawBox(Bar, FLinearColor(0.f, 0.f, 0.f, 0.8f));
	DrawBox(FBox2D(Bar.Min, FVector2D(Bar.Min.X + Bar.GetSize().X * Ratio, Bar.Max.Y)), BarColor);
	DrawFrame(Bar, FLinearColor(0.8f, 0.8f, 0.8f, 0.8f), 1.5f * S);
	Y += 30.f * S;

	DrawTextAt(FString::Printf(TEXT("Здоровье %d / %d"), Hamster->GetHP(), Hamster->GetMaxHP()), TextColor, X, Y, 1.3f * S);
	Y += 30.f * S;
	DrawTextAt(FString::Printf(TEXT("Броня %d"), Hamster->GetArmor()),
		Hamster->GetArmor() > 0 ? FLinearColor(0.45f, 0.75f, 1.f) : DimTextColor, X, Y, 1.3f * S);
}

// ---------------------------------------------------------------------------
// Hand

void ACRDebugHUD::DrawHand(const ACRCombatGameMode* GM, const ACRPlayerController* PC, bool bValidDrop, bool bCursorOverHand)
{
	const float S = UIScale();
	const bool bPlayerTurn = GM->GetTurnState() == ECRTurnState::PlayerTurn;
	const int32 Dragged = PC->GetDraggedCard();

	float MouseX = 0.f;
	float MouseY = 0.f;
	PC->GetMousePosition(MouseX, MouseY);
	const int32 Hovered = (bPlayerTurn && Dragged == INDEX_NONE) ? GetCardIndexAt(FVector2D(MouseX, MouseY)) : INDEX_NONE;

	for (int32 i = 0; i < CardRects.Num() && i < GM->Cards.Num(); ++i)
	{
		const bool bAffordable = bPlayerTurn && GM->CanAffordCard(i);
		const bool bSelected = GM->GetSelectedCardIndex() == i;
		FBox2D Rect = CardRects[i];
		FLinearColor Border = bAffordable ? FLinearColor(0.35f, 0.35f, 0.45f) : FLinearColor(0.2f, 0.2f, 0.22f);

		if (i == Hovered)
		{
			Rect = Rect.ShiftBy(FVector2D(0.f, -16.f * S));
			Border = bAffordable ? FLinearColor(0.95f, 0.95f, 1.f) : FLinearColor(0.5f, 0.3f, 0.3f);
		}
		if (bSelected)
		{
			Rect = CardRects[i].ShiftBy(FVector2D(0.f, -16.f * S));
			Border = FLinearColor(0.2f, 1.f, 1.f);
		}

		// The dragged card leaves a faded ghost in its slot.
		DrawCard(GM->Cards[i], i, Rect, Border, bAffordable, i == Dragged ? 0.3f : 1.f);
	}

	if (Dragged != INDEX_NONE && GM->Cards.IsValidIndex(Dragged) && CardRects.IsValidIndex(Dragged))
	{
		const FVector2D Size = CardRects[Dragged].GetSize() * 1.08f;
		const FVector2D Min(MouseX - Size.X * 0.5f, MouseY - Size.Y - 24.f * S);
		const FLinearColor Border = bCursorOverHand ? FLinearColor(0.95f, 0.95f, 1.f) : (bValidDrop ? ValidColor : InvalidColor);
		DrawCard(GM->Cards[Dragged], Dragged, FBox2D(Min, Min + Size), Border, true, 0.95f);
	}
}

void ACRDebugHUD::DrawCard(const FCRCardDef& Card, int32 Index, const FBox2D& Rect, const FLinearColor& Border, bool bAffordable, float Alpha)
{
	const float S = UIScale();
	const FVector2D Size = Rect.GetSize();
	const float CenterX = Rect.Min.X + Size.X * 0.5f;
	const float TextScale = Size.X / (150.f * S);
	auto Fade = [Alpha](FLinearColor C) { C.A *= Alpha; return C; };

	DrawBox(Rect, Fade(bAffordable ? FLinearColor(0.12f, 0.12f, 0.17f, 0.95f) : FLinearColor(0.07f, 0.07f, 0.08f, 0.85f)));
	DrawFrame(Rect, Fade(Border), 3.f * S);

	// Mana cost badge.
	const float Badge = 34.f * S * TextScale;
	const FVector2D BadgeMin = Rect.Min + FVector2D(8.f * S);
	const FBox2D BadgeBox(BadgeMin, BadgeMin + FVector2D(Badge));
	DrawBox(BadgeBox, Fade(bAffordable ? ManaColor : FLinearColor(0.45f, 0.12f, 0.1f)));
	DrawTextCentered(FString::FromInt(Card.ManaCost), Fade(FLinearColor::White), BadgeBox.GetCenter().X, BadgeBox.Min.Y + 4.f * S, 1.4f * S * TextScale);

	// Russian card titles/descriptions can be longer than the card: shrink to fit, wrap the description.
	const float TextMaxW = Size.X - 14.f * S * TextScale;
	DrawTextFitted(Card.Name, Fade(bAffordable ? TextColor : DimTextColor), CenterX, Rect.Min.Y + Size.Y * 0.32f, 1.6f * S * TextScale, TextMaxW);
	DrawTextFitted(Card.ShortText, Fade(bAffordable ? FLinearColor(0.75f, 0.8f, 0.9f) : DimTextColor), CenterX, Rect.Min.Y + Size.Y * 0.52f, 1.05f * S * TextScale, TextMaxW, 0.9f);
	DrawTextCentered(FString::Printf(TEXT("[%d]"), Index + 1), Fade(DimTextColor), CenterX, Rect.Max.Y - 30.f * S * TextScale, 1.0f * S * TextScale);
}

// ---------------------------------------------------------------------------
// World overlays

void ACRDebugHUD::DrawEnemyOverlays(const ACRCombatGameMode* GM)
{
	const float S = UIScale();
	const bool bDebug = GM->IsDebugView();

	for (const ACREnemy* Enemy : GM->GetEnemies())
	{
		if (!IsValid(Enemy) || Enemy->IsEliminated())
		{
			continue;
		}

		const FVector Screen = Project(Enemy->GetActorLocation() + FVector(0.f, 0.f, 110.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}

		const FCREnemyStats& Stats = Enemy->GetStats();
		const float BarW = 90.f * S;
		const float BarTop = Screen.Y - 44.f * S;
		const FBox2D Bar(FVector2D(Screen.X - BarW * 0.5f, BarTop), FVector2D(Screen.X + BarW * 0.5f, BarTop + 10.f * S));
		const float Ratio = Stats.MaxHP > 0 ? FMath::Clamp(float(Enemy->GetHP()) / Stats.MaxHP, 0.f, 1.f) : 0.f;
		DrawBox(Bar, FLinearColor(0.f, 0.f, 0.f, 0.75f));
		DrawBox(FBox2D(Bar.Min, FVector2D(Bar.Min.X + BarW * Ratio, Bar.Max.Y)), FLinearColor(0.9f, 0.15f, 0.1f));

		const bool bAttack = Enemy->WillAttack();
		DrawTextCentered(bAttack ? FString::Printf(TEXT("АТАКА %d"), Stats.Damage) : FString(TEXT("ДВИЖЕНИЕ")),
			bAttack ? FLinearColor(1.f, 0.35f, 0.25f) : FLinearColor(1.f, 0.9f, 0.35f), Screen.X, Bar.Max.Y + 3.f * S, 1.15f * S);

		if (bDebug)
		{
			DrawTextCentered(FString::Printf(TEXT("%s %s  HP %d/%d"), *Enemy->GetDisplayName(),
				Stats.CombatType == ECRCombatType::Melee ? TEXT("MELEE") : TEXT("RANGED"), Enemy->GetHP(), Stats.MaxHP),
				FLinearColor::White, Screen.X, Bar.Min.Y - 22.f * S, 0.95f * S);
		}
	}
}

void ACRDebugHUD::DrawArenaLabels(const ACRCombatGameMode* GM)
{
	const float S = UIScale();

	// Edge names (Debug View keeps the 3D English label with its cycling key instead).
	if (!GM->IsDebugView())
	{
		for (TActorIterator<ACRBoundarySegment> It(GetWorld()); It; ++It)
		{
			const FVector Screen = Project(It->GetLabelWorldLocation());
			if (Screen.Z > 0.f)
			{
				const FLinearColor Color = FMath::Lerp(CRProto::BoundaryTypeColor(It->GetBoundaryType()), FLinearColor::White, 0.45f);
				DrawTextShadowCentered(CRProto::BoundaryTypeDisplayName(It->GetBoundaryType()), Color, Screen.X, Screen.Y - 12.f * S, 1.25f * S);
			}
		}
	}

	for (TActorIterator<ACRPit> It(GetWorld()); It; ++It)
	{
		const FVector Screen = Project(It->GetLabelWorldLocation());
		if (Screen.Z > 0.f)
		{
			DrawTextShadowCentered(TEXT("ЯМА"), FLinearColor(1.f, 0.45f, 0.4f), Screen.X, Screen.Y - 12.f * S, 1.2f * S);
		}
	}
}

void ACRDebugHUD::DrawTargetingPreview(const ACRCombatGameMode* GM, const ACRPlayerController* PC, bool& bOutValidDrop)
{
	bOutValidDrop = false;
	if (GM->GetTurnState() != ECRTurnState::PlayerTurn)
	{
		return;
	}

	const int32 Dragged = PC->GetDraggedCard();
	const int32 CardIndex = Dragged != INDEX_NONE ? Dragged : GM->GetSelectedCardIndex();
	if (!GM->Cards.IsValidIndex(CardIndex))
	{
		return;
	}

	FHitResult Hit;
	const bool bHit = PC->GetCursorWorldHit(Hit);
	const FVector Ground(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, 3.f);
	const FLinearColor AimColor(0.2f, 1.f, 1.f);

	// PUSH stage 2: direction arrow from the locked target.
	if (Dragged == INDEX_NONE && GM->IsAwaitingPushDirection())
	{
		if (const AActor* Target = GM->GetPendingTarget())
		{
			const FVector From(Target->GetActorLocation().X, Target->GetActorLocation().Y, 40.f);
			DrawWorldCircle(FVector(From.X, From.Y, 3.f), 90.f, AimColor, 3.f);
			const FVector Dir = FVector(Ground.X - From.X, Ground.Y - From.Y, 0.f).GetSafeNormal();
			if (bHit && !Dir.IsNearlyZero())
			{
				DrawWorldArrow(From, From + Dir * 450.f, AimColor, 5.f);
			}
		}
		return;
	}

	const FCRCardDef& Card = GM->Cards[CardIndex];
	const bool bValid = GM->IsValidCardDrop(CardIndex, Hit.GetActor(), Hit.ImpactPoint, bHit);
	bOutValidDrop = bValid;

	switch (Card.Targeting)
	{
	case ECRCardTargeting::GroundPoint:
		if (bHit)
		{
			DrawWorldCircle(Ground, Card.Radius, bValid ? FLinearColor(1.f, 0.55f, 0.1f) : FLinearColor(0.5f, 0.5f, 0.5f, 0.6f), 3.f);
		}
		break;

	case ECRCardTargeting::PhysicsTarget:
	case ECRCardTargeting::PhysicsTargetThenPoint:
		if (bValid)
		{
			const FVector At = Hit.GetActor()->GetActorLocation();
			DrawWorldCircle(FVector(At.X, At.Y, 3.f), 90.f, ValidColor, 3.f);
		}
		break;

	case ECRCardTargeting::None:
		if (bValid && GM->GetHamster())
		{
			const FVector At = GM->GetHamster()->GetActorLocation();
			DrawWorldCircle(FVector(At.X, At.Y, 3.f), 110.f, ValidColor, 3.f);
		}
		break;
	}
}

void ACRDebugHUD::DrawDebugExtras(const ACRCombatGameMode* GM)
{
	const float S = UIScale();
	const float X = 28.f * S;
	float Y = 300.f * S;

	DrawTextAt(TEXT("DEBUG VIEW  (F10)"), FLinearColor(1.f, 0.4f, 1.f), X, Y, 1.2f * S);
	Y += 28.f * S;
	DrawTextAt(GM->IsRunIntegrated()
		? TEXT("1-5 select card | LMB target | RMB cancel | Space end turn | 7/8/9 cycle edge | R restart disabled during run")
		: TEXT("1-5 select card | LMB target | RMB cancel | Space end turn | 7/8/9 cycle edge | R restart"), DimTextColor, X, Y, 0.95f * S);
	Y += 30.f * S;

	if (GM->GetTurnState() == ECRTurnState::Reward)
	{
		FString Offers;
		for (const FName Offer : GM->GetRewardOffers())
		{
			Offers += (Offers.IsEmpty() ? TEXT("") : TEXT(", ")) + Offer.ToString();
		}
		DrawTextAt(FString::Printf(TEXT("Reward: offers [%s]  hand %d cards  +%d/%d/%d  committed: %s  (1-3 pick, Space skip)"), *Offers,
			GM->Cards.Num(), GM->RewardSilver, GM->RewardFood, GM->RewardWood, GM->IsRewardCommitted() ? TEXT("yes") : TEXT("no")),
			FLinearColor(1.f, 0.85f, 0.35f), X, Y, 0.95f * S);
		Y += 26.f * S;
	}

	for (const FString& Event : GM->GetEventLog())
	{
		DrawTextAt(Event, FLinearColor(0.85f, 0.85f, 0.85f), X, Y, 0.95f * S);
		Y += 20.f * S;
	}
}

void ACRDebugHUD::DrawResultBanner(const ACRCombatGameMode* GM)
{
	const ECRTurnState State = GM->GetTurnState();
	if (State != ECRTurnState::Victory && State != ECRTurnState::Defeat)
	{
		return;
	}
	const float S = UIScale();
	const bool bWon = State == ECRTurnState::Victory;
	DrawTextCentered(bWon ? TEXT("ПОБЕДА") : TEXT("ПОРАЖЕНИЕ"), bWon ? FLinearColor::Green : FLinearColor::Red, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.36f, 5.f * S);
	// Run rooms: a won fight goes on to the reward panel; a lost one returns to the map.
	const TCHAR* Subtitle = !GM->IsRunIntegrated() ? TEXT("Нажмите R, чтобы начать заново")
		: (bWon ? TEXT("Получите награду") : TEXT("Возвращение на карту..."));
	DrawTextCentered(Subtitle,
		FLinearColor::White, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.36f + 110.f * S, 1.5f * S);
}

// ---------------------------------------------------------------------------
// Canvas helpers

void ACRDebugHUD::DrawBox(const FBox2D& Box, const FLinearColor& Fill)
{
	const FVector2D Size = Box.GetSize();
	DrawRect(Fill, Box.Min.X, Box.Min.Y, Size.X, Size.Y);
}

void ACRDebugHUD::DrawFrame(const FBox2D& Box, const FLinearColor& Color, float Thickness)
{
	const FVector2D Size = Box.GetSize();
	DrawRect(Color, Box.Min.X, Box.Min.Y, Size.X, Thickness);
	DrawRect(Color, Box.Min.X, Box.Max.Y - Thickness, Size.X, Thickness);
	DrawRect(Color, Box.Min.X, Box.Min.Y, Thickness, Size.Y);
	DrawRect(Color, Box.Max.X - Thickness, Box.Min.Y, Thickness, Size.Y);
}

void ACRDebugHUD::DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale)
{
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
}

void ACRDebugHUD::DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale)
{
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}

float ACRDebugHUD::TextWidth(const FString& Text, float Scale)
{
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetMediumFont(), Scale);
	return W;
}

void ACRDebugHUD::DrawTextFitted(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale, float MaxWidth, float MinFactor)
{
	const float Width = TextWidth(Text, Scale);
	if (Width <= MaxWidth || Width <= 0.f)
	{
		DrawTextCentered(Text, Color, CenterX, Y, Scale);
		return;
	}

	// Shrink a little first; if it still does not fit and has spaces, wrap onto two lines instead.
	const float Fitted = Scale * FMath::Max(MinFactor, MaxWidth / Width);
	int32 Split = INDEX_NONE;
	if (TextWidth(Text, Fitted) > MaxWidth)
	{
		// Break at the space closest to the middle.
		for (int32 i = 0; i < Text.Len(); ++i)
		{
			if (Text[i] == TEXT(' ') && (Split == INDEX_NONE || FMath::Abs(i - Text.Len() / 2) < FMath::Abs(Split - Text.Len() / 2)))
			{
				Split = i;
			}
		}
	}
	if (Split == INDEX_NONE)
	{
		DrawTextCentered(Text, Color, CenterX, Y, Fitted);
		return;
	}

	const FString First = Text.Left(Split);
	const FString Second = Text.Mid(Split + 1);
	const float LineScale = Scale * FMath::Max(MinFactor, FMath::Min(1.f, MaxWidth / FMath::Max(TextWidth(First, Scale), TextWidth(Second, Scale))));
	float W = 0.f;
	float H = 0.f;
	GetTextSize(TEXT("Ay"), W, H, GEngine->GetMediumFont(), LineScale);
	DrawTextCentered(First, Color, CenterX, Y, LineScale);
	DrawTextCentered(Second, Color, CenterX, Y + H * 1.05f, LineScale);
}

void ACRDebugHUD::DrawTextShadowCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale)
{
	const float Offset = FMath::Max(1.f, 2.f * UIScale());
	DrawTextCentered(Text, FLinearColor(0.f, 0.f, 0.f, 0.85f), CenterX + Offset, Y + Offset, Scale);
	DrawTextCentered(Text, Color, CenterX, Y, Scale);
}

void ACRDebugHUD::DrawWorldCircle(const FVector& Center, float Radius, const FLinearColor& Color, float Thickness)
{
	constexpr int32 Segments = 40;
	FVector Prev = Project(Center + FVector(Radius, 0.f, 0.f));
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float Angle = 2.f * PI * i / Segments;
		const FVector Next = Project(Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f));
		if (Prev.Z > 0.f && Next.Z > 0.f)
		{
			DrawLine(Prev.X, Prev.Y, Next.X, Next.Y, Color, Thickness * UIScale());
		}
		Prev = Next;
	}
}

void ACRDebugHUD::DrawWorldArrow(const FVector& From, const FVector& To, const FLinearColor& Color, float Thickness)
{
	const FVector A = Project(From);
	const FVector B = Project(To);
	if (A.Z <= 0.f || B.Z <= 0.f)
	{
		return;
	}
	const float T = Thickness * UIScale();
	DrawLine(A.X, A.Y, B.X, B.Y, Color, T);

	const FVector Dir = (To - From).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
	for (const float Sign : { 1.f, -1.f })
	{
		const FVector Wing = Project(To - Dir * 110.f + Side * 70.f * Sign);
		if (Wing.Z > 0.f)
		{
			DrawLine(B.X, B.Y, Wing.X, Wing.Y, Color, T);
		}
	}
}
