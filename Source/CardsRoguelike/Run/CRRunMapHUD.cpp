#include "CRRunMapHUD.h"

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

	if (!Run->HasRun())
	{
		DrawTextCentered(TEXT("No active run  (R to start the test run)"), RunHudTextColor, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.45f, 1.6f * S);
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
	const FString RoomName = Current ? CRRun::RoomTypeName(Current->RoomType) : FString(TEXT("-"));
	float X = 28.f * S;
	float Y = 24.f * S;
	DrawPanel(X - 12.f * S, Y - 10.f * S, 330.f * S, 150.f * S);
	DrawTextAt(bFailed ? TEXT("RUN FAILED") : TEXT("RUN"), bFailed ? FLinearColor(1.f, 0.3f, 0.25f) : FLinearColor(0.45f, 1.f, 0.55f), X, Y, 2.0f * S);
	Y += 44.f * S;
	DrawTextAt(FString::Printf(TEXT("Hamster: %s"), *State.Hamster.Name), RunHudTextColor, X, Y, 1.25f * S);
	Y += 30.f * S;
	DrawTextAt(FString::Printf(TEXT("HP: %d / %d"), State.Hamster.CurrentHP, State.Hamster.MaxHP), RunHudTextColor, X, Y, 1.25f * S);
	Y += 30.f * S;
	DrawTextAt(FString::Printf(TEXT("Current room: %s"), *RoomName), RunHudTextColor, X, Y, 1.25f * S);

	// Top right: currencies carried this run.
	const float W = 220.f * S;
	X = Canvas->ClipX - W - 28.f * S;
	Y = 24.f * S;
	DrawPanel(X - 12.f * S, Y - 10.f * S, W, 112.f * S);
	DrawTextAt(FString::Printf(TEXT("Silver  %d"), State.Carried.Silver), FLinearColor(0.85f, 0.88f, 0.95f), X, Y, 1.3f * S);
	Y += 32.f * S;
	DrawTextAt(FString::Printf(TEXT("Food    %d"), State.Carried.Food), FLinearColor(0.95f, 0.75f, 0.4f), X, Y, 1.3f * S);
	Y += 32.f * S;
	DrawTextAt(FString::Printf(TEXT("Wood    %d"), State.Carried.Wood), FLinearColor(0.75f, 0.55f, 0.35f), X, Y, 1.3f * S);

	// Center banner: run failure takes priority over room arrival messages.
	const ACRRunMapGameMode* GM = GetWorld()->GetAuthGameMode<ACRRunMapGameMode>();
	const ACRRunMapActor* Map = GM ? GM->GetMapActor() : nullptr;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float BannerY = Canvas->ClipY * 0.14f;
	if (bFailed)
	{
		DrawPanel(CenterX - 300.f * S, BannerY - 14.f * S, 600.f * S, 110.f * S);
		DrawTextCentered(TEXT("RUN FAILED"), FLinearColor(1.f, 0.3f, 0.25f), CenterX, BannerY, 2.2f * S);
		DrawTextCentered(FString::Printf(TEXT("%s fell in %s"), *State.Hamster.Name, *RoomName), RunHudDimTextColor, CenterX, BannerY + 52.f * S, 1.2f * S);
	}
	else if (Map && !Map->GetArrivalTitle().IsEmpty())
	{
		DrawPanel(CenterX - 300.f * S, BannerY - 14.f * S, 600.f * S, 110.f * S);
		DrawTextCentered(Map->GetArrivalTitle(), FLinearColor(1.f, 0.85f, 0.35f), CenterX, BannerY, 2.2f * S);
		DrawTextCentered(Map->GetArrivalSubtitle(), RunHudDimTextColor, CenterX, BannerY + 52.f * S, 1.2f * S);
	}

	// Bottom hint.
	const bool bFinished = Current && Current->ConnectedNodeIds.Num() == 0;
	const TCHAR* Hint = bFailed ? TEXT("Press R to start a new prototype run")
		: (bFinished ? TEXT("End of the route  -  R restarts the test run") : TEXT("Click a highlighted room to travel"));
	DrawTextCentered(Hint, bFailed ? RunHudTextColor : RunHudDimTextColor, CenterX, Canvas->ClipY - 50.f * S, 1.2f * S);

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
