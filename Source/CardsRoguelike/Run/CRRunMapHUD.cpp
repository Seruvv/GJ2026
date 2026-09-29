#include "CRRunMapHUD.h"

#include "../Meta/CRProfileSubsystem.h"
#include "CRRunMapActor.h"
#include "CRRunMapGameMode.h"
#include "CRRunMapPlayerController.h"
#include "CRRunNodeActor.h"
#include "CRRunSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	// Readability: more opaque panels and brighter secondary text against the dark map.
	const FLinearColor RunHudPanelColor(0.02f, 0.02f, 0.03f, 0.84f);
	const FLinearColor RunHudTextColor(0.96f, 0.96f, 0.98f);
	const FLinearColor RunHudDimTextColor(0.8f, 0.8f, 0.84f);
}

void ACRRunMapHUD::DrawHUD()
{
	Super::DrawHUD();

	const UGameInstance* GameInstance = GetGameInstance();
	const UCRRunSubsystem* Run = GameInstance ? GameInstance->GetSubsystem<UCRRunSubsystem>() : nullptr;
	if (!Run || !Canvas)
	{
		return;
	}

	const float S = Canvas->ClipY / 1080.f;
	const FCRRunState& State = Run->GetRunState();
	ReturnRect = FBox2D(ForceInit);
	LeaveRect = FBox2D(ForceInit);

	if (!Run->HasRun())
	{
		DrawTextCentered(TEXT("Нет активного забега  (R — начать забег)"), RunHudTextColor, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.45f, 1.6f * S);
		return;
	}
	const bool bFailed = State.Status == ECRRunStatus::Failed;

	// Room labels: drawn flat on the canvas (bright, with a drop shadow) instead of lit 3D text.
	if (const ACRRunMapGameMode* LabelGM = GetWorld()->GetAuthGameMode<ACRRunMapGameMode>())
	{
		if (const ACRRunMapActor* LabelMap = LabelGM->GetMapActor())
		{
			for (const TPair<FName, TObjectPtr<ACRRunNodeActor>>& Pair : LabelMap->GetNodeActors())
			{
				const ACRRunNodeActor* Node = Pair.Value;
				const FVector Screen = Node ? Project(Node->GetLabelWorldLocation()) : FVector::ZeroVector;
				if (Node && Screen.Z > 0.f && !Node->GetLabelText().IsEmpty())
				{
					const float LabelScale = 1.35f * S;
					const float Shadow = FMath::Max(1.f, 2.f * S);
					DrawTextCentered(Node->GetLabelText(), FLinearColor(0.f, 0.f, 0.f, 0.9f), Screen.X + Shadow, Screen.Y - 30.f * S + Shadow, LabelScale);
					DrawTextCentered(Node->GetLabelText(), Node->GetLabelColor(), Screen.X, Screen.Y - 30.f * S, LabelScale);
				}
			}
		}
	}

	// Top left: run and hamster.
	const FCRRunNodeData* Current = Run->GetCurrentNode();
	const FString RoomName = Current ? CRRun::RoomTypeDisplayName(Current->RoomType) : FString(TEXT("-"));
	float X = 28.f * S;
	float Y = 24.f * S;
	DrawPanel(X - 12.f * S, Y - 10.f * S, 330.f * S, 150.f * S);
	DrawTextAt(bFailed ? TEXT("ЗАБЕГ ПРОВАЛЕН") : TEXT("ЗАБЕГ"), bFailed ? FLinearColor(1.f, 0.3f, 0.25f) : FLinearColor(0.45f, 1.f, 0.55f), X, Y, 2.0f * S);
	Y += 44.f * S;
	DrawTextAt(FString::Printf(TEXT("Хомяк: %s"), *State.Hamster.Name), RunHudTextColor, X, Y, 1.25f * S);
	Y += 30.f * S;
	DrawTextAt(FString::Printf(TEXT("Здоровье: %d / %d"), State.Hamster.CurrentHP, State.Hamster.MaxHP), RunHudTextColor, X, Y, 1.25f * S);
	Y += 30.f * S;
	DrawTextAt(FString::Printf(TEXT("Текущая комната: %s"), *RoomName), RunHudTextColor, X, Y, 1.25f * S);

	// Top right: currencies carried this run.
	const float W = 220.f * S;
	X = Canvas->ClipX - W - 28.f * S;
	Y = 24.f * S;
	DrawPanel(X - 12.f * S, Y - 10.f * S, W, 112.f * S);
	DrawTextAt(FString::Printf(TEXT("Серебро  %d"), State.Carried.Silver), FLinearColor(0.85f, 0.88f, 0.95f), X, Y, 1.3f * S);
	Y += 32.f * S;
	DrawTextAt(FString::Printf(TEXT("Еда  %d"), State.Carried.Food), FLinearColor(0.95f, 0.75f, 0.4f), X, Y, 1.3f * S);
	Y += 32.f * S;
	DrawTextAt(FString::Printf(TEXT("Дерево  %d"), State.Carried.Wood), FLinearColor(0.75f, 0.55f, 0.35f), X, Y, 1.3f * S);

	// Center banner: run failure takes priority over room arrival messages.
	const ACRRunMapGameMode* GM = GetWorld()->GetAuthGameMode<ACRRunMapGameMode>();
	const ACRRunMapActor* Map = GM ? GM->GetMapActor() : nullptr;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float BannerY = Canvas->ClipY * 0.14f;
	if (bFailed)
	{
		DrawPanel(CenterX - 300.f * S, BannerY - 14.f * S, 600.f * S, 110.f * S);
		DrawTextCentered(TEXT("ЗАБЕГ ПРОВАЛЕН"), FLinearColor(1.f, 0.3f, 0.25f), CenterX, BannerY, 2.2f * S);
		DrawTextCentered(FString::Printf(TEXT("%s погиб. Комната: %s"), *State.Hamster.Name, *RoomName), RunHudDimTextColor, CenterX, BannerY + 52.f * S, 1.2f * S);
	}
	else if (Map && !Map->GetArrivalTitle().IsEmpty())
	{
		DrawPanel(CenterX - 300.f * S, BannerY - 14.f * S, 600.f * S, 110.f * S);
		DrawTextCentered(Map->GetArrivalTitle(), FLinearColor(1.f, 0.85f, 0.35f), CenterX, BannerY, 2.2f * S);
		DrawTextCentered(Map->GetArrivalSubtitle(), RunHudDimTextColor, CenterX, BannerY + 52.f * S, 1.2f * S);
	}

	// Bottom hint. Runs from the hub go back to the sanctuary; developer runs restart with R.
	const bool bFinished = Current && Current->ConnectedNodeIds.Num() == 0;
	const bool bProfileRun = Run->IsProfileRun();
	const TCHAR* Hint = bProfileRun
		? (bFailed ? TEXT("Хомяк погиб — вернитесь в убежище") : (bFinished ? TEXT("Конец маршрута — вернитесь в убежище") : TEXT("Выберите подсвеченную комнату")))
		: (bFailed ? TEXT("R — начать новый забег") : (bFinished ? TEXT("Конец маршрута — R начинает новый забег") : TEXT("Выберите подсвеченную комнату")));
	DrawTextCentered(Hint, bFailed ? RunHudTextColor : RunHudDimTextColor, CenterX, Canvas->ClipY - 50.f * S, 1.2f * S);
	if (bProfileRun)
	{
		DrawProfileRunControls(State, S);
	}

	const ACRRunMapPlayerController* PC = Cast<ACRRunMapPlayerController>(GetOwningPlayerController());
	if (PC && PC->IsDebugView())
	{
		X = 28.f * S;
		Y = 200.f * S;
		DrawTextAt(TEXT("DEBUG VIEW  (F10)   R restart test run"), FLinearColor(1.f, 0.4f, 1.f), X, Y, 1.1f * S);
		Y += 26.f * S;
		DrawTextAt(FString::Printf(TEXT("Current node: %s   resolved: %s   status: %s"), *State.CurrentNodeId.ToString(),
			State.bCurrentRoomResolved ? TEXT("yes") : TEXT("no"), *UEnum::GetDisplayValueAsText(State.Status).ToString()), RunHudTextColor, X, Y, 1.0f * S);
		Y += 22.f * S;
		FString Path;
		for (const FName Id : State.VisitedNodeIds)
		{
			Path += (Path.IsEmpty() ? TEXT("") : TEXT(" > ")) + Id.ToString();
		}
		DrawTextAt(FString::Printf(TEXT("Path: %s"), *Path), RunHudTextColor, X, Y, 1.0f * S);
		Y += 22.f * S;
		DrawTextAt(FString::Printf(TEXT("Deck: %d cards   Artifacts: %d"), State.DeckCardIds.Num(), State.ArtifactIds.Num()), RunHudTextColor, X, Y, 1.0f * S);
	}
}

void ACRRunMapHUD::DrawProfileRunControls(const FCRRunState& State, float S)
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetMousePosition(MouseX, MouseY);
	}

	if (State.Status == ECRRunStatus::Completed || State.Status == ECRRunStatus::Failed)
	{
		// The run already delivered its loot to the profile (see UCRProfileSubsystem); show what arrived.
		FString Sub;
		const UCRProfileSubsystem* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCRProfileSubsystem>() : nullptr;
		const UCRProfileSaveGame* Profile = Profiles ? Profiles->GetActiveProfile() : nullptr;
		if (Profile && Profile->LastRun.bValid && Profile->LastRun.RunSeed == State.RunSeed)
		{
			Sub = FString::Printf(TEXT("доставлено: %s"), *CRMeta::FormatGain(Profile->LastRun.Delivered));
		}
		const float CenterX = Canvas->ClipX * 0.5f;
		ReturnRect = FBox2D(FVector2D(CenterX - 230.f * S, Canvas->ClipY - 170.f * S), FVector2D(CenterX + 230.f * S, Canvas->ClipY - 88.f * S));
		DrawButtonBox(ReturnRect, TEXT("ВЕРНУТЬСЯ В УБЕЖИЩЕ"), Sub, ReturnRect.IsInside(FVector2D(MouseX, MouseY)), S);
		return;
	}

	if (State.Status == ECRRunStatus::Active)
	{
		const bool bArmed = GetWorld() && GetWorld()->GetRealTimeSeconds() - LeaveArmedTime < 4.0;
		const float W = 220.f * S;
		const float X = Canvas->ClipX - W - 40.f * S;
		LeaveRect = FBox2D(FVector2D(X, 140.f * S), FVector2D(X + W, 196.f * S));
		DrawButtonBox(LeaveRect, bArmed ? TEXT("ТОЧНО ПОКИНУТЬ?") : TEXT("ПОКИНУТЬ ПОХОД"),
			bArmed ? TEXT("добыча будет потеряна") : TEXT("вернуться в убежище"), LeaveRect.IsInside(FVector2D(MouseX, MouseY)), S);
	}
}

void ACRRunMapHUD::DrawButtonBox(const FBox2D& Rect, const FString& Label, const FString& SubLabel, bool bHovered, float S)
{
	const FVector2D Size = Rect.GetSize();
	DrawRect(bHovered ? FLinearColor(0.55f, 0.4f, 0.16f, 0.97f) : FLinearColor(0.4f, 0.28f, 0.1f, 0.94f), Rect.Min.X, Rect.Min.Y, Size.X, Size.Y);
	const FLinearColor Border = bHovered ? FLinearColor::White : FLinearColor(0.95f, 0.78f, 0.4f);
	const float T = 2.f * S;
	DrawRect(Border, Rect.Min.X, Rect.Min.Y, Size.X, T);
	DrawRect(Border, Rect.Min.X, Rect.Max.Y - T, Size.X, T);
	DrawRect(Border, Rect.Min.X, Rect.Min.Y, T, Size.Y);
	DrawRect(Border, Rect.Max.X - T, Rect.Min.Y, T, Size.Y);
	const float LabelScale = FMath::Min(1.4f * S, Size.Y / 40.f);
	const float LabelY = SubLabel.IsEmpty() ? Rect.GetCenter().Y - 12.f * LabelScale : Rect.Min.Y + 6.f * S;
	DrawTextCentered(Label, RunHudTextColor, Rect.GetCenter().X, LabelY, LabelScale);
	if (!SubLabel.IsEmpty())
	{
		DrawTextCentered(SubLabel, RunHudDimTextColor, Rect.GetCenter().X, Rect.Max.Y - 26.f * S, 0.95f * S);
	}
}

ECRRunMapButton ACRRunMapHUD::HitTest(const FVector2D& ScreenPos) const
{
	if (ReturnRect.bIsValid && ReturnRect.IsInside(ScreenPos))
	{
		return ECRRunMapButton::ReturnToHub;
	}
	if (LeaveRect.bIsValid && LeaveRect.IsInside(ScreenPos))
	{
		return ECRRunMapButton::LeaveRun;
	}
	return ECRRunMapButton::None;
}

bool ACRRunMapHUD::ConfirmLeave()
{
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	if (Now - LeaveArmedTime < 4.0)
	{
		LeaveArmedTime = -100.0;
		return true;
	}
	LeaveArmedTime = Now;
	return false;
}

void ACRRunMapHUD::DrawPanel(float X, float Y, float W, float H)
{
	DrawRect(RunHudPanelColor, X, Y, W, H);
}

void ACRRunMapHUD::DrawTextAt(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale)
{
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
}

void ACRRunMapHUD::DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale)
{
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}
