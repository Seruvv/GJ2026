#include "CRMenuHUDBase.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

void ACRMenuHUDBase::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	S = Canvas->ClipY / 1080.f;
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetMousePosition(MouseX, MouseY);
	}
	Mouse = FVector2D(MouseX, MouseY);

	Buttons.Reset();
	DrawScreen();
}

bool ACRMenuHUDBase::HandleClick(const FVector2D& ScreenPos)
{
	// Topmost (last drawn) first.
	for (int32 i = Buttons.Num() - 1; i >= 0; --i)
	{
		const FCRUIButton& Button = Buttons[i];
		if (Button.Rect.IsInside(ScreenPos))
		{
			if (Button.bEnabled)
			{
				const FCRUIButton Copy = Button; // OnButton may change screens
				OnButton(Copy);
			}
			return true;
		}
	}
	return false;
}

void ACRMenuHUDBase::ShowMessage(const FString& InMessage, bool bError)
{
	Message = InMessage;
	bMessageError = bError;
	MessageTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
}

void ACRMenuHUDBase::DrawMessage(float CenterY)
{
	const double Age = GetWorld() ? GetWorld()->GetRealTimeSeconds() - MessageTime : 100.0;
	if (Message.IsEmpty() || Age > 3.5)
	{
		return;
	}
	const float Alpha = Age > 2.5 ? float(3.5 - Age) : 1.f;
	const float Scale = 1.4f * S;
	const float W = TextWidth(Message, Scale) + 48.f * S;
	const float CenterX = Canvas->ClipX * 0.5f;
	const FBox2D Box(FVector2D(CenterX - W * 0.5f, CenterY - 8.f * S), FVector2D(CenterX + W * 0.5f, CenterY + TextHeight(Scale) + 8.f * S));
	DrawBox(Box, FLinearColor(0.02f, 0.02f, 0.03f, 0.85f * Alpha));
	const FLinearColor Color = bMessageError ? FLinearColor(1.f, 0.45f, 0.35f, Alpha) : FLinearColor(1.f, 0.85f, 0.4f, Alpha);
	DrawTextCentered(Message, Color, CenterX, CenterY, Scale);
}

void ACRMenuHUDBase::DrawBox(const FBox2D& Box, const FLinearColor& Fill)
{
	const FVector2D Size = Box.GetSize();
	DrawRect(Fill, Box.Min.X, Box.Min.Y, Size.X, Size.Y);
}

void ACRMenuHUDBase::DrawFrame(const FBox2D& Box, const FLinearColor& Color, float Thickness)
{
	const FVector2D Size = Box.GetSize();
	DrawRect(Color, Box.Min.X, Box.Min.Y, Size.X, Thickness);
	DrawRect(Color, Box.Min.X, Box.Max.Y - Thickness, Size.X, Thickness);
	DrawRect(Color, Box.Min.X, Box.Min.Y, Thickness, Size.Y);
	DrawRect(Color, Box.Max.X - Thickness, Box.Min.Y, Thickness, Size.Y);
}

void ACRMenuHUDBase::DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale)
{
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
}

void ACRMenuHUDBase::DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale)
{
	DrawText(Text, Color, CenterX - TextWidth(Text, Scale) * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}

void ACRMenuHUDBase::DrawTextShadowCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale)
{
	const float Offset = FMath::Max(1.f, 2.f * S);
	DrawTextCentered(Text, FLinearColor(0.f, 0.f, 0.f, 0.85f * Color.A), CenterX + Offset, Y + Offset, Scale);
	DrawTextCentered(Text, Color, CenterX, Y, Scale);
}

float ACRMenuHUDBase::TextWidth(const FString& Text, float Scale)
{
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetMediumFont(), Scale);
	return W;
}

float ACRMenuHUDBase::TextHeight(float Scale)
{
	float W = 0.f;
	float H = 0.f;
	GetTextSize(TEXT("Ay"), W, H, GEngine->GetMediumFont(), Scale);
	return H;
}

float ACRMenuHUDBase::FitScale(const FString& Text, float Scale, float MaxWidth)
{
	const float W = TextWidth(Text, Scale);
	return W > MaxWidth && W > 0.f ? Scale * MaxWidth / W : Scale;
}

float ACRMenuHUDBase::DrawWrapped(const FString& Text, const FLinearColor& Color, float X, float Y, float MaxWidth, float Scale)
{
	const float LineH = TextHeight(Scale) * 1.1f;
	TArray<FString> Paragraphs;
	Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	for (const FString& Paragraph : Paragraphs)
	{
		TArray<FString> Words;
		Paragraph.ParseIntoArrayWS(Words);
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (!Line.IsEmpty() && TextWidth(Candidate, Scale) > MaxWidth)
			{
				DrawTextAt(Line, Color, X, Y, Scale);
				Y += LineH;
				Line = Word;
			}
			else
			{
				Line = Candidate;
			}
		}
		DrawTextAt(Line, Color, X, Y, Scale);
		Y += LineH;
	}
	return Y;
}

bool ACRMenuHUDBase::RegisterButton(const FBox2D& Rect, FName Action, const FString& Arg, bool bEnabled)
{
	FCRUIButton& Button = Buttons.AddDefaulted_GetRef();
	Button.Rect = Rect;
	Button.Action = Action;
	Button.Arg = Arg;
	Button.bEnabled = bEnabled;
	return bEnabled && Rect.IsInside(Mouse);
}

bool ACRMenuHUDBase::DrawButton(const FBox2D& Rect, const FString& Label, FName Action, const FString& Arg, bool bEnabled,
	ECRUIButtonStyle Style, const FString& SubLabel)
{
	FCRUIButton& Button = Buttons.AddDefaulted_GetRef();
	Button.Rect = Rect;
	Button.Action = Action;
	Button.Arg = Arg;
	Button.bEnabled = bEnabled;

	const bool bHovered = bEnabled && Rect.IsInside(Mouse);
	FLinearColor Fill(0.16f, 0.16f, 0.2f, 0.95f);
	FLinearColor Border(0.45f, 0.45f, 0.55f);
	switch (Style)
	{
	case ECRUIButtonStyle::Primary: Fill = FLinearColor(0.45f, 0.32f, 0.12f, 0.96f); Border = FLinearColor(0.95f, 0.78f, 0.4f); break;
	case ECRUIButtonStyle::Danger:  Fill = FLinearColor(0.38f, 0.1f, 0.08f, 0.95f);  Border = FLinearColor(0.95f, 0.45f, 0.35f); break;
	case ECRUIButtonStyle::Subtle:  Fill = FLinearColor(0.08f, 0.08f, 0.1f, 0.85f);  Border = FLinearColor(0.3f, 0.3f, 0.36f); break;
	default: break;
	}
	if (!bEnabled)
	{
		Fill = FLinearColor(0.07f, 0.07f, 0.08f, 0.85f);
		Border = FLinearColor(0.22f, 0.22f, 0.24f);
	}
	else if (bHovered)
	{
		Fill = Fill * 1.35f;
		Fill.A = 0.98f;
		Border = FLinearColor::White;
	}
	DrawBox(Rect, Fill);
	DrawFrame(Rect, Border, 2.f * S);

	const FLinearColor LabelColor = bEnabled ? FLinearColor(0.95f, 0.93f, 0.88f) : FLinearColor(0.45f, 0.45f, 0.48f);
	const float MaxW = Rect.GetSize().X - 16.f * S;
	const float Height = Rect.GetSize().Y;
	const float LabelScale = FitScale(Label, FMath::Min(1.5f * S, Height / 36.f), MaxW);
	if (SubLabel.IsEmpty())
	{
		DrawTextCentered(Label, LabelColor, Rect.GetCenter().X, Rect.GetCenter().Y - TextHeight(LabelScale) * 0.5f, LabelScale);
	}
	else
	{
		const float SubScale = FitScale(SubLabel, 0.95f * S, MaxW);
		const float Total = TextHeight(LabelScale) + TextHeight(SubScale);
		const float Top = Rect.GetCenter().Y - Total * 0.5f;
		DrawTextCentered(Label, LabelColor, Rect.GetCenter().X, Top, LabelScale);
		DrawTextCentered(SubLabel, LabelColor * FLinearColor(0.8f, 0.8f, 0.8f, 1.f), Rect.GetCenter().X, Top + TextHeight(LabelScale), SubScale);
	}
	return bHovered;
}
