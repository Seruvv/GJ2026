#include "CREventHUD.h"

#include "../Combat/CRCardLibrary.h"
#include "../Run/CRRunTypes.h"
#include "CREventDefinition.h"
#include "CREventGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor EventHudPanelColor(0.02f, 0.02f, 0.025f, 0.88f);
	const FLinearColor EventHudTitleColor(1.f, 0.84f, 0.45f);
	const FLinearColor EventHudTextColor(0.92f, 0.9f, 0.86f);
	const FLinearColor EventHudDimTextColor(0.55f, 0.54f, 0.52f);
	const FLinearColor EventHudGoodColor(0.45f, 0.9f, 0.5f);
	const FLinearColor EventHudBadColor(1.f, 0.45f, 0.35f);
	const FLinearColor EventHudButtonColor(0.1f, 0.1f, 0.12f, 0.95f);
	const FLinearColor EventHudButtonHotColor(0.18f, 0.16f, 0.12f, 0.98f);
	const FLinearColor EventHudDisabledColor(0.06f, 0.06f, 0.065f, 0.9f);

	// Narrative text is the point of this screen: it is drawn larger than the other rooms' HUD text.
	const float EventHudTitleScale = 2.6f;
	const float EventHudBodyScale = 1.75f;
	const float EventHudChoiceScale = 1.6f;
	const float EventHudSmallScale = 1.2f;

	// Effect summary lines are colored by sign so gains and losses read at a glance.
	FLinearColor ResultLineColor(const FString& Line)
	{
		if (Line.Contains(TEXT(" -")) || Line.StartsWith(TEXT("Потеряна карта")))
		{
			return EventHudBadColor;
		}
		return EventHudGoodColor;
	}
}

UFont* ACREventHUD::GetFont() const
{
	return GEngine->GetMediumFont();
}

float ACREventHUD::LineHeight(float Scale) const
{
	float W = 0.f;
	float H = 0.f;
	// Includes tall Cyrillic glyphs (Й) and descenders so lines never overlap.
	const_cast<ACREventHUD*>(this)->GetTextSize(TEXT("AgЙу"), W, H, GetFont(), Scale);
	return H * 1.18f;
}

float ACREventHUD::TextWidth(const FString& Text, float Scale) const
{
	float W = 0.f;
	float H = 0.f;
	const_cast<ACREventHUD*>(this)->GetTextSize(Text, W, H, GetFont(), Scale);
	return W;
}

const TArray<FString>& ACREventHUD::WrapText(const FString& Text, float Scale, float MaxWidth)
{
	const FString Key = FString::Printf(TEXT("%d|%d|%s"), FMath::RoundToInt(Scale * 1000.f), FMath::RoundToInt(MaxWidth), *Text);
	if (const TArray<FString>* Cached = WrapCache.Find(Key))
	{
		return *Cached;
	}
	if (WrapCache.Num() > 512)
	{
		WrapCache.Reset();
	}

	TArray<FString> Lines;
	FString Normalized = Text.Replace(TEXT("\r\n"), TEXT("\n"));
	TArray<FString> Paragraphs;
	Normalized.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	for (const FString& Paragraph : Paragraphs)
	{
		TArray<FString> Words;
		Paragraph.ParseIntoArrayWS(Words);
		if (Words.Num() == 0)
		{
			// Authored blank line: keep it as paragraph spacing.
			Lines.Add(FString());
			continue;
		}

		FString Line;
		for (FString Word : Words)
		{
			// Overlong single words are split so nothing runs past the panel.
			while (TextWidth(Word, Scale) > MaxWidth && Word.Len() > 1)
			{
				int32 Fit = 1;
				while (Fit < Word.Len() && TextWidth(Word.Left(Fit + 1), Scale) <= MaxWidth)
				{
					++Fit;
				}
				if (!Line.IsEmpty())
				{
					Lines.Add(Line);
					Line.Reset();
				}
				Lines.Add(Word.Left(Fit));
				Word.RightChopInline(Fit);
			}

			const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (Line.IsEmpty() || TextWidth(Candidate, Scale) <= MaxWidth)
			{
				Line = Candidate;
			}
			else
			{
				Lines.Add(Line);
				Line = Word;
			}
		}
		Lines.Add(Line);
	}
	return WrapCache.Add(Key, MoveTemp(Lines));
}

void ACREventHUD::AppendWrapped(TArray<FTextLine>& Out, const FString& Text, const FLinearColor& Color, float Scale, float MaxWidth)
{
	for (const FString& Line : WrapText(Text, Scale, MaxWidth))
	{
		Out.Add({ Line, Color, Scale });
	}
}

float ACREventHUD::DrawScrollRegion(const TArray<FTextLine>& Lines, const FBox2D& Region, float S)
{
	float Total = 0.f;
	for (const FTextLine& Line : Lines)
	{
		Total += LineHeight(Line.Scale);
	}
	const float Overflow = FMath::Max(0.f, Total - Region.GetSize().Y);
	ScrollOffset = FMath::Clamp(ScrollOffset, 0.f, Overflow);

	// Line-level clipping: only lines fully inside the region are drawn.
	float Y = Region.Min.Y - ScrollOffset;
	for (const FTextLine& Line : Lines)
	{
		const float H = LineHeight(Line.Scale);
		if (Y >= Region.Min.Y - 1.f && Y + H <= Region.Max.Y + 1.f && !Line.Text.IsEmpty())
		{
			DrawTextAt(Line.Text, Line.Color, Region.Min.X, Y, Line.Scale);
		}
		Y += H;
	}

	if (Overflow > 0.f)
	{
		const float HintScale = 0.95f * S;
		if (ScrollOffset > 0.f)
		{
			const FString Above = TEXT("^ выше есть ещё текст");
			DrawTextAt(Above, EventHudDimTextColor, Region.Max.X - TextWidth(Above, HintScale), Region.Min.Y - 22.f * S, HintScale);
		}
		if (ScrollOffset < Overflow)
		{
			const FString Below = TEXT("v дальше — колесо мыши");
			DrawTextAt(Below, EventHudDimTextColor, Region.Max.X - TextWidth(Below, HintScale), Region.Max.Y + 2.f * S, HintScale);
		}
	}
	return Overflow;
}

void ACREventHUD::Scroll(float Lines)
{
	ScrollOffset = FMath::Max(0.f, ScrollOffset + Lines * ScrollLineStep);
}

void ACREventHUD::DrawHUD()
{
	Super::DrawHUD();
	Hits.Reset();

	const ACREventGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACREventGameMode>() : nullptr;
	if (!GM || !Canvas)
	{
		return;
	}

	const float S = Canvas->ClipY / 1080.f;
	ScrollLineStep = 32.f * S;
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetMousePosition(MouseX, MouseY);
	}
	const FVector2D Mouse(MouseX, MouseY);

	const ECREventScreen Screen = GM->GetScreen();
	if (static_cast<uint8>(Screen) != LastScreen)
	{
		// Every screen starts scrolled to the top.
		LastScreen = static_cast<uint8>(Screen);
		ScrollOffset = 0.f;
	}

	if (Screen == ECREventScreen::Invalid)
	{
		// Direct sandbox open (or broken data): show the room, nothing is interactive, no run data touched.
		const float CenterX = Canvas->ClipX * 0.5f;
		DrawBox(FBox2D(FVector2D(CenterX - 320.f * S, 40.f * S), FVector2D(CenterX + 320.f * S, 150.f * S)), EventHudPanelColor);
		DrawTextCentered(TEXT("СОБЫТИЕ: ТЕСТОВАЯ КАРТА"), EventHudTitleColor, CenterX, 56.f * S, 2.f * S);
		DrawTextCentered(GM->GetInvalidReason(), EventHudDimTextColor, CenterX, 104.f * S, 1.3f * S);
		return;
	}

	// The narrative panel owns the left of the screen; the illustration stays visible on the right.
	const FBox2D Panel(FVector2D(40.f * S, 40.f * S), FVector2D(FMath::Max(Canvas->ClipX * 0.58f, 720.f * S), Canvas->ClipY - 40.f * S));
	DrawBox(Panel, EventHudPanelColor);
	DrawFrame(Panel, FLinearColor(0.3f, 0.26f, 0.18f), 2.f * S);
	const FBox2D Content(Panel.Min + FVector2D(40.f * S, 34.f * S), Panel.Max - FVector2D(40.f * S, 30.f * S));

	switch (Screen)
	{
	case ECREventScreen::Choices:    DrawChoicesScreen(GM, Content, S, Mouse); break;
	case ECREventScreen::CardSelect: DrawCardSelectScreen(GM, Content, S, Mouse); break;
	case ECREventScreen::Result:     DrawResultScreen(GM, Content, S, Mouse); break;
	default: break;
	}
	DrawStatus(GM, S);

	const FString Message = GM->GetActiveMessage();
	if (!Message.IsEmpty())
	{
		const float CenterX = (Panel.Max.X + Canvas->ClipX) * 0.5f;
		DrawTextCentered(Message, FLinearColor(1.f, 0.75f, 0.3f), CenterX, Canvas->ClipY - 90.f * S, 1.4f * S);
	}
}

void ACREventHUD::DrawChoicesScreen(const ACREventGameMode* GM, const FBox2D& Content, float S, const FVector2D& Mouse)
{
	const UCREventDefinition* Event = GM->GetEvent();
	const float Width = Content.GetSize().X;

	// Title (wrapped, can span lines).
	float Y = Content.Min.Y;
	const float TitleScale = EventHudTitleScale * S;
	for (const FString& Line : WrapText(Event->Title.ToString(), TitleScale, Width))
	{
		DrawTextAt(Line, EventHudTitleColor, Content.Min.X, Y, TitleScale);
		Y += LineHeight(TitleScale);
	}
	Y += 8.f * S;
	DrawBox(FBox2D(FVector2D(Content.Min.X, Y), FVector2D(Content.Max.X, Y + 2.f * S)), FLinearColor(0.35f, 0.3f, 0.2f));
	Y += 18.f * S;

	// Choice buttons are laid out bottom-up so the body gets whatever height is left.
	const float ChoiceScale = EventHudChoiceScale * S;
	const float ReasonScale = EventHudSmallScale * S;
	const float LabelW = 56.f * S;
	const float Pad = 14.f * S;
	const float Gap = 12.f * S;
	const float TextW = Width - LabelW - 2.f * Pad;

	TArray<float> Heights;
	float ChoicesH = 0.f;
	for (int32 i = 0; i < Event->Choices.Num(); ++i)
	{
		const int32 LineCount = WrapText(Event->Choices[i].ChoiceText.ToString(), ChoiceScale, TextW).Num();
		const bool bBlocked = !GM->GetChoiceBlockReason(i).IsEmpty();
		const float H = FMath::Max(58.f * S, LineCount * LineHeight(ChoiceScale) + (bBlocked ? LineHeight(ReasonScale) : 0.f) + 2.f * Pad);
		Heights.Add(H);
		ChoicesH += H + (i > 0 ? Gap : 0.f);
	}

	float ChoiceY = Content.Max.Y - ChoicesH;
	const FBox2D BodyRegion(FVector2D(Content.Min.X, Y), FVector2D(Content.Max.X, ChoiceY - 30.f * S));

	TArray<FTextLine> Body;
	AppendWrapped(Body, Event->NarrativeBody.ToString(), EventHudTextColor, EventHudBodyScale * S, Width);
	DrawScrollRegion(Body, BodyRegion, S);

	for (int32 i = 0; i < Event->Choices.Num(); ++i)
	{
		const FBox2D Rect(FVector2D(Content.Min.X, ChoiceY), FVector2D(Content.Max.X, ChoiceY + Heights[i]));
		const FString Reason = GM->GetChoiceBlockReason(i);
		const bool bEnabled = Reason.IsEmpty();
		const bool bHovered = bEnabled && Rect.IsInside(Mouse);
		AddHit(Rect, ECREventHitKind::Choice, i);

		DrawBox(Rect, bEnabled ? (bHovered ? EventHudButtonHotColor : EventHudButtonColor) : EventHudDisabledColor);
		DrawFrame(Rect, bHovered ? FLinearColor::White : (bEnabled ? FLinearColor(0.55f, 0.47f, 0.3f) : FLinearColor(0.22f, 0.22f, 0.23f)), 2.f * S);
		DrawTextAt(FString::Printf(TEXT("%d."), i + 1), bEnabled ? EventHudTitleColor : EventHudDimTextColor, Rect.Min.X + Pad, Rect.Min.Y + Pad, ChoiceScale);

		float LineY = Rect.Min.Y + Pad;
		for (const FString& Line : WrapText(Event->Choices[i].ChoiceText.ToString(), ChoiceScale, TextW))
		{
			DrawTextAt(Line, bEnabled ? EventHudTextColor : EventHudDimTextColor, Rect.Min.X + Pad + LabelW, LineY, ChoiceScale);
			LineY += LineHeight(ChoiceScale);
		}
		if (!bEnabled)
		{
			DrawTextAt(Reason, EventHudBadColor, Rect.Min.X + Pad + LabelW, LineY, ReasonScale);
		}
		ChoiceY += Heights[i] + Gap;
	}
}

void ACREventHUD::DrawCardSelectScreen(const ACREventGameMode* GM, const FBox2D& Content, float S, const FVector2D& Mouse)
{
	const UCREventDefinition* Event = GM->GetEvent();
	const float Width = Content.GetSize().X;

	float Y = Content.Min.Y;
	for (const FString& Line : WrapText(TEXT("ВЫБЕРИТЕ КАРТУ, КОТОРОЙ ПРИДЁТСЯ ПОЖЕРТВОВАТЬ"), EventHudTitleScale * S, Width))
	{
		DrawTextAt(Line, EventHudTitleColor, Content.Min.X, Y, EventHudTitleScale * S);
		Y += LineHeight(EventHudTitleScale * S);
	}
	Y += 6.f * S;
	if (Event->Choices.IsValidIndex(GM->GetPendingChoice()))
	{
		for (const FString& Line : WrapText(Event->Choices[GM->GetPendingChoice()].ChoiceText.ToString(), EventHudSmallScale * S, Width))
		{
			DrawTextAt(Line, EventHudDimTextColor, Content.Min.X, Y, EventHudSmallScale * S);
			Y += LineHeight(EventHudSmallScale * S);
		}
	}
	DrawTextAt(TEXT("Нажмите на карту. Пока карта не выбрана, ничего не произойдёт."), EventHudDimTextColor, Content.Min.X, Y + 4.f * S, 1.0f * S);
	Y += LineHeight(1.0f * S) + 22.f * S;

	// BACK (bottom-left) returns to the choices; nothing has been committed yet.
	const FBox2D BackRect(FVector2D(Content.Min.X, Content.Max.Y - 56.f * S), FVector2D(Content.Min.X + 200.f * S, Content.Max.Y));
	const bool bBackHot = BackRect.IsInside(Mouse);
	AddHit(BackRect, ECREventHitKind::Back);
	DrawBox(BackRect, bBackHot ? EventHudButtonHotColor : EventHudButtonColor);
	DrawFrame(BackRect, bBackHot ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.5f), 2.f * S);
	DrawTextCentered(TEXT("НАЗАД"), EventHudTextColor, BackRect.GetCenter().X, BackRect.Min.Y + 8.f * S, 1.3f * S);
	DrawTextCentered(TEXT("Backspace"), EventHudDimTextColor, BackRect.GetCenter().X, BackRect.Min.Y + 34.f * S, 0.85f * S);

	// Every deck entry as its own card (duplicates separately), in a wrapped grid that scrolls if needed.
	const TArray<FName>& Deck = GM->GetDeck();
	const FBox2D GridRegion(FVector2D(Content.Min.X, Y), FVector2D(Content.Max.X, BackRect.Min.Y - 24.f * S));
	const float CardW = 128.f * S;
	const float CardH = 168.f * S;
	const float Gap = 16.f * S;
	const int32 Columns = FMath::Max(1, FMath::FloorToInt((Width + Gap) / (CardW + Gap)));
	const int32 Rows = FMath::DivideAndRoundUp(Deck.Num(), Columns);
	const float GridH = Rows * CardH + FMath::Max(0, Rows - 1) * Gap;
	const float Overflow = FMath::Max(0.f, GridH - GridRegion.GetSize().Y);
	ScrollOffset = FMath::Clamp(ScrollOffset, 0.f, Overflow);

	for (int32 i = 0; i < Deck.Num(); ++i)
	{
		const int32 Col = i % Columns;
		const int32 Row = i / Columns;
		const FVector2D Min(GridRegion.Min.X + Col * (CardW + Gap), GridRegion.Min.Y + Row * (CardH + Gap) - ScrollOffset);
		const FBox2D Rect(Min, Min + FVector2D(CardW, CardH));
		if (Rect.Min.Y < GridRegion.Min.Y - 1.f || Rect.Max.Y > GridRegion.Max.Y + 1.f)
		{
			continue;
		}

		const bool bHovered = Rect.IsInside(Mouse);
		AddHit(Rect, ECREventHitKind::Card, i);
		DrawBox(Rect, bHovered ? FLinearColor(0.3f, 0.1f, 0.08f, 0.97f) : FLinearColor(0.12f, 0.12f, 0.16f, 0.95f));
		DrawFrame(Rect, bHovered ? EventHudBadColor : FLinearColor(0.45f, 0.45f, 0.55f), 3.f * S);

		const FCRCardDef* Card = CRCardLibrary::FindCard(Deck[i]);
		const FVector2D Badge = Rect.Min + FVector2D(8.f * S);
		DrawBox(FBox2D(Badge, Badge + FVector2D(28.f * S)), FLinearColor(0.25f, 0.6f, 1.f));
		DrawTextCentered(Card ? FString::FromInt(Card->ManaCost) : TEXT("?"), FLinearColor::White, Badge.X + 14.f * S, Badge.Y + 3.f * S, 1.1f * S);
		DrawTextCentered(Card ? Card->Name : Deck[i].ToString(), EventHudTextColor, Rect.GetCenter().X, Rect.Min.Y + 58.f * S, 1.35f * S);
		if (Card)
		{
			float LineY = Rect.Min.Y + 92.f * S;
			for (const FString& Line : WrapText(Card->ShortText, 0.95f * S, CardW - 14.f * S))
			{
				DrawTextCentered(Line, FLinearColor(0.75f, 0.78f, 0.88f), Rect.GetCenter().X, LineY, 0.95f * S);
				LineY += LineHeight(0.95f * S);
			}
		}
	}
	if (Overflow > 0.f && ScrollOffset < Overflow)
	{
		DrawTextAt(TEXT("v ещё карты — колесо мыши"), EventHudDimTextColor, GridRegion.Min.X, GridRegion.Max.Y + 2.f * S, 0.95f * S);
	}
}

void ACREventHUD::DrawResultScreen(const ACREventGameMode* GM, const FBox2D& Content, float S, const FVector2D& Mouse)
{
	const UCREventDefinition* Event = GM->GetEvent();
	const FCREventNodeState* State = GM->GetEventState();
	const float Width = Content.GetSize().X;

	float Y = Content.Min.Y;
	DrawTextAt(TEXT("РЕЗУЛЬТАТ"), EventHudTitleColor, Content.Min.X, Y, EventHudTitleScale * S);
	Y += LineHeight(EventHudTitleScale * S);
	DrawTextAt(Event->Title.ToString(), EventHudDimTextColor, Content.Min.X, Y, EventHudSmallScale * S);
	Y += LineHeight(EventHudSmallScale * S) + 6.f * S;
	DrawBox(FBox2D(FVector2D(Content.Min.X, Y), FVector2D(Content.Max.X, Y + 2.f * S)), FLinearColor(0.35f, 0.3f, 0.2f));
	Y += 18.f * S;

	// CONTINUE (bottom-right): the player decides when the result has been read.
	const FBox2D ContinueRect(FVector2D(Content.Max.X - 280.f * S, Content.Max.Y - 76.f * S), Content.Max);
	const bool bActive = !GM->IsContinuing();
	const bool bHot = bActive && ContinueRect.IsInside(Mouse);
	AddHit(ContinueRect, ECREventHitKind::Continue);
	DrawBox(ContinueRect, bActive ? (bHot ? FLinearColor(0.5f, 0.36f, 0.14f, 0.97f) : FLinearColor(0.38f, 0.26f, 0.1f, 0.95f)) : EventHudDisabledColor);
	DrawFrame(ContinueRect, bHot ? FLinearColor::White : FLinearColor(0.9f, 0.7f, 0.4f), 3.f * S);
	DrawTextCentered(TEXT("ПРОДОЛЖИТЬ"), EventHudTextColor, ContinueRect.GetCenter().X, ContinueRect.Min.Y + 10.f * S, 1.6f * S);
	DrawTextCentered(TEXT("Вернуться на карту  (Пробел)"), EventHudDimTextColor, ContinueRect.GetCenter().X, ContinueRect.Min.Y + 46.f * S, 0.9f * S);

	// Result narrative, the actual consequences, then the event's closing line: one scrollable column.
	TArray<FTextLine> Lines;
	if (State && Event->Choices.IsValidIndex(State->SelectedChoiceIndex))
	{
		AppendWrapped(Lines, FString::Printf(TEXT("\"%s\""), *Event->Choices[State->SelectedChoiceIndex].ChoiceText.ToString()), EventHudDimTextColor, EventHudSmallScale * S, Width);
		Lines.Add({ FString(), EventHudTextColor, 0.8f * S });
		AppendWrapped(Lines, Event->Choices[State->SelectedChoiceIndex].ResultText.ToString(), EventHudTextColor, EventHudBodyScale * S, Width);
	}
	if (State && State->ResultLines.Num() > 0)
	{
		Lines.Add({ FString(), EventHudTextColor, EventHudBodyScale * S });
		for (const FString& ResultLine : State->ResultLines)
		{
			AppendWrapped(Lines, ResultLine, ResultLineColor(ResultLine), EventHudChoiceScale * S, Width);
		}
	}
	const FString Continuation = Event->ContinuationText.ToString();
	if (!Continuation.IsEmpty())
	{
		Lines.Add({ FString(), EventHudTextColor, EventHudBodyScale * S });
		AppendWrapped(Lines, Continuation, FLinearColor(0.75f, 0.7f, 0.6f), EventHudChoiceScale * S, Width);
	}
	DrawScrollRegion(Lines, FBox2D(FVector2D(Content.Min.X, Y), FVector2D(Content.Max.X, ContinueRect.Min.Y - 28.f * S)), S);
}

void ACREventHUD::DrawStatus(const ACREventGameMode* GM, float S)
{
	// Compact run status (top-right) so costs can be judged while reading.
	const float W = 270.f * S;
	const FVector2D Min(Canvas->ClipX - W - 28.f * S, 28.f * S);
	DrawBox(FBox2D(Min, Min + FVector2D(W, 178.f * S)), EventHudPanelColor);
	float Y = Min.Y + 10.f * S;
	const float X = Min.X + 14.f * S;
	DrawTextAt(FString::Printf(TEXT("Здоровье: %d / %d"), GM->GetHP(), GM->GetMaxHP()), EventHudTextColor, X, Y, 1.2f * S);
	Y += 32.f * S;
	DrawTextAt(FString::Printf(TEXT("Серебро: %d"), GM->GetSilver()), FLinearColor(0.85f, 0.88f, 0.95f), X, Y, 1.2f * S);
	Y += 32.f * S;
	DrawTextAt(FString::Printf(TEXT("Еда: %d"), GM->GetFood()), FLinearColor(0.95f, 0.75f, 0.4f), X, Y, 1.2f * S);
	Y += 32.f * S;
	DrawTextAt(FString::Printf(TEXT("Дерево: %d"), GM->GetWood()), FLinearColor(0.75f, 0.55f, 0.35f), X, Y, 1.2f * S);
	Y += 32.f * S;
	DrawTextAt(FString::Printf(TEXT("Колода: %d"), GM->GetDeck().Num()), EventHudDimTextColor, X, Y, 1.2f * S);
}

FCREventHit ACREventHUD::HitTest(const FVector2D& ScreenPos) const
{
	for (const TPair<FBox2D, FCREventHit>& Hit : Hits)
	{
		if (Hit.Key.IsInside(ScreenPos))
		{
			return Hit.Value;
		}
	}
	return FCREventHit();
}

void ACREventHUD::AddHit(const FBox2D& Box, ECREventHitKind Kind, int32 Index)
{
	FCREventHit Hit;
	Hit.Kind = Kind;
	Hit.Index = Index;
	Hits.Add(TPair<FBox2D, FCREventHit>(Box, Hit));
}

void ACREventHUD::DrawBox(const FBox2D& Box, const FLinearColor& Fill)
{
	const FVector2D Size = Box.GetSize();
	DrawRect(Fill, Box.Min.X, Box.Min.Y, Size.X, Size.Y);
}

void ACREventHUD::DrawFrame(const FBox2D& Box, const FLinearColor& Color, float Thickness)
{
	const FVector2D Size = Box.GetSize();
	DrawRect(Color, Box.Min.X, Box.Min.Y, Size.X, Thickness);
	DrawRect(Color, Box.Min.X, Box.Max.Y - Thickness, Size.X, Thickness);
	DrawRect(Color, Box.Min.X, Box.Min.Y, Thickness, Size.Y);
	DrawRect(Color, Box.Max.X - Thickness, Box.Min.Y, Thickness, Size.Y);
}

void ACREventHUD::DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale)
{
	DrawText(Text, Color, X, Y, GetFont(), Scale);
}

void ACREventHUD::DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale)
{
	DrawText(Text, Color, CenterX - TextWidth(Text, Scale) * 0.5f, Y, GetFont(), Scale);
}
