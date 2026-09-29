#include "CRHubHUD.h"

#include "../Meta/CRProfileSubsystem.h"
#include "CRHubGameMode.h"
#include "CRHubRoomActor.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor HubPanel(0.03f, 0.03f, 0.045f, 0.84f);
	const FLinearColor HubTitle(1.f, 0.84f, 0.45f);
	const FLinearColor HubText(0.92f, 0.91f, 0.88f);
	const FLinearColor HubDim(0.62f, 0.62f, 0.66f);
	const FLinearColor HubGood(0.45f, 0.95f, 0.55f);
	const FLinearColor HubBad(1.f, 0.45f, 0.35f);
	const FLinearColor HubSilver(0.85f, 0.88f, 0.95f);
	const FLinearColor HubFood(0.95f, 0.75f, 0.4f);
	const FLinearColor HubWoodColor(0.8f, 0.6f, 0.38f);

	const FName ActionSelect = TEXT("Select");
	const FName ActionUpgrade = TEXT("Upgrade");
	const FName ActionConvert = TEXT("Convert");
	const FName ActionStartRun = TEXT("StartRun");
	const FName ActionMenu = TEXT("MainMenu");
	const FName ActionPlaceholder = TEXT("Placeholder");

	FString LevelText(int32 Level, int32 MaxLevel)
	{
		return Level <= 0 ? FString::Printf(TEXT("Не построено (0 из %d)"), MaxLevel) : FString::Printf(TEXT("Уровень %d из %d"), Level, MaxLevel);
	}
}

void ACRHubHUD::DrawScreen()
{
	ACRHubGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRHubGameMode>() : nullptr;
	const UCRProfileSubsystem* Profiles = GM ? GM->GetProfiles() : nullptr;
	if (!GM || !Profiles)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	if (!Profiles->HasActiveProfile() || !Profiles->GetCatalog())
	{
		DrawBox(FBox2D(FVector2D(CenterX - 360.f * S, 380.f * S), FVector2D(CenterX + 360.f * S, 620.f * S)), HubPanel);
		DrawTextCentered(TEXT("УБЕЖИЩЕ"), HubTitle, CenterX, 410.f * S, 2.f * S);
		DrawTextCentered(Profiles->HasActiveProfile() ? TEXT("Не найдены данные убежища (DA_HubCatalog)") : TEXT("Профиль не выбран"),
			HubBad, CenterX, 470.f * S, 1.3f * S);
		DrawButton(FBox2D(FVector2D(CenterX - 160.f * S, 530.f * S), FVector2D(CenterX + 160.f * S, 590.f * S)), TEXT("В ГЛАВНОЕ МЕНЮ"), ActionMenu);
		return;
	}

	DrawMarkers(*Profiles, *GM);
	DrawTopBar(*Profiles);
	if (const UCRHubBuildingDefinition* Selected = Profiles->GetCatalog()->FindBuilding(SelectedBuildingId))
	{
		DrawDetails(*Profiles, *Selected);
	}
	DrawBottomBar(*Profiles);
	// Feedback just above the expedition button, clear of the building markers.
	DrawMessage(Canvas->ClipY - 205.f * S);
}

void ACRHubHUD::DrawMarkers(const UCRProfileSubsystem& Profiles, ACRHubGameMode& GM)
{
	ACRHubRoomActor* Room = GM.GetRoom();
	if (!Room)
	{
		return;
	}
	FName NewHover;
	for (const UCRHubBuildingDefinition* Building : Profiles.GetCatalog()->Buildings)
	{
		if (!Building)
		{
			continue;
		}
		const FVector Screen = Project(Room->GetAnchorLocation(Building->AnchorId));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const int32 Level = Profiles.GetBuildingLevel(*Building);
		const FString Title = Building->DisplayName.ToUpper();
		const FString Sub = Level <= 0 ? FString(TEXT("не построено")) : FString::Printf(TEXT("уровень %d"), Level);
		const float W = FMath::Max(TextWidth(Title, 1.35f * S), TextWidth(Sub, 1.0f * S)) + 36.f * S;
		const FBox2D Rect(FVector2D(Screen.X - W * 0.5f, Screen.Y - 70.f * S), FVector2D(Screen.X + W * 0.5f, Screen.Y));
		const bool bSelected = Building->BuildingId == SelectedBuildingId;
		const bool bHovered = DrawButton(Rect, Title, ActionSelect, Building->BuildingId.ToString(), true,
			bSelected ? ECRUIButtonStyle::Primary : ECRUIButtonStyle::Normal, Sub);
		if (bHovered)
		{
			NewHover = Building->BuildingId;
		}
		Room->SetAnchorHighlight(Building->AnchorId, bSelected ? 2 : (bHovered ? 1 : 0));
	}
	HoveredBuildingId = NewHover;
}

void ACRHubHUD::DrawTopBar(const UCRProfileSubsystem& Profiles)
{
	const UCRProfileSaveGame* Profile = Profiles.GetActiveProfile();

	// Top left: title and the last run's outcome.
	float X = 28.f * S;
	float Y = 22.f * S;
	const bool bHasRun = Profile->LastRun.bValid;
	DrawBox(FBox2D(FVector2D(X - 12.f * S, Y - 10.f * S), FVector2D(X + 620.f * S, Y + (bHasRun ? 150.f : 70.f) * S)), HubPanel);
	DrawTextAt(TEXT("УБЕЖИЩЕ В БУТЫЛКЕ"), HubTitle, X, Y, 2.0f * S);
	Y += 48.f * S;
	if (bHasRun)
	{
		const FCRRunEndSummary& Run = Profile->LastRun;
		const TCHAR* Outcome = Run.Reason == ECRRunEndReason::Completed ? TEXT("Последний поход: хомяк вернулся в убежище")
			: (Run.Reason == ECRRunEndReason::Failed ? TEXT("Последний поход: хомяк погиб") : TEXT("Последний поход: прерван"));
		DrawTextAt(Outcome, Run.Reason == ECRRunEndReason::Completed ? HubGood : HubBad, X, Y, 1.15f * S);
		Y += 30.f * S;
		DrawTextAt(FString::Printf(TEXT("Доставлено в убежище: %s"), *CRMeta::FormatGain(Run.Delivered)), HubText, X, Y, 1.05f * S);
		Y += 28.f * S;
		if (Run.Delivered.Silver != Run.Carried.Silver || Run.Delivered.Food != Run.Carried.Food || Run.Delivered.Wood != Run.Carried.Wood)
		{
			DrawTextAt(FString::Printf(TEXT("Добыто в походе: %s"), *CRMeta::FormatResources(Run.Carried)), HubDim, X, Y, 1.0f * S);
		}
		else
		{
			DrawTextAt(FString::Printf(TEXT("Походов: %d · вернулся: %d · погиб: %d"), Profile->Stats.RunsStarted, Profile->Stats.RunsCompleted,
				Profile->Stats.RunsFailed), HubDim, X, Y, 1.0f * S);
		}
	}

	// Top right: persistent resources.
	const float W = 250.f * S;
	X = Canvas->ClipX - W - 28.f * S;
	Y = 22.f * S;
	DrawBox(FBox2D(FVector2D(X - 14.f * S, Y - 10.f * S), FVector2D(X + W, Y + 112.f * S)), HubPanel);
	DrawTextAt(FString::Printf(TEXT("Серебро  %d"), Profile->Resources.Silver), HubSilver, X, Y, 1.35f * S);
	DrawTextAt(FString::Printf(TEXT("Еда  %d"), Profile->Resources.Food), HubFood, X, Y + 34.f * S, 1.35f * S);
	DrawTextAt(FString::Printf(TEXT("Дерево  %d"), Profile->Resources.Wood), HubWoodColor, X, Y + 68.f * S, 1.35f * S);
}

float ACRHubHUD::DrawCost(const FCRMetaResources& Cost, const FCRMetaResources& Have, float X, float Y, float Scale)
{
	const auto Part = [&](const TCHAR* Name, int32 Need, int32 Owned)
	{
		if (Need <= 0)
		{
			return;
		}
		const FString Text = FString::Printf(TEXT("%s %d   "), Name, Need);
		DrawTextAt(Text, Owned >= Need ? HubText : HubBad, X, Y, Scale);
		X += TextWidth(Text, Scale);
	};
	if (Cost.IsZero())
	{
		DrawTextAt(TEXT("бесплатно"), HubText, X, Y, Scale);
		return X;
	}
	Part(TEXT("Серебро"), Cost.Silver, Have.Silver);
	Part(TEXT("Еда"), Cost.Food, Have.Food);
	Part(TEXT("Дерево"), Cost.Wood, Have.Wood);
	return X;
}

void ACRHubHUD::DrawDetails(const UCRProfileSubsystem& Profiles, const UCRHubBuildingDefinition& Building)
{
	const UCRProfileSaveGame* Profile = Profiles.GetActiveProfile();
	const float PanelW = 560.f * S;
	const float Left = Canvas->ClipX - PanelW - 28.f * S;
	const float Top = 160.f * S;
	const float Bottom = Canvas->ClipY - 130.f * S;
	const float Pad = 22.f * S;
	const float X = Left + Pad;
	const float MaxW = PanelW - Pad * 2.f;
	DrawBox(FBox2D(FVector2D(Left, Top), FVector2D(Left + PanelW, Bottom)), HubPanel);
	DrawFrame(FBox2D(FVector2D(Left, Top), FVector2D(Left + PanelW, Bottom)), FLinearColor(0.5f, 0.42f, 0.25f), 2.f * S);

	const int32 Level = Profiles.GetBuildingLevel(Building);
	float Y = Top + 18.f * S;
	DrawTextAt(Building.DisplayName.ToUpper(), HubTitle, X, Y, FitScale(Building.DisplayName.ToUpper(), 1.8f * S, MaxW));
	Y += 44.f * S;
	DrawTextAt(LevelText(Level, Building.GetMaxLevel()), HubText, X, Y, 1.2f * S);
	Y += 36.f * S;
	Y = DrawWrapped(Building.Description, HubDim, X, Y, MaxW, 1.0f * S) + 10.f * S;

	// What the building gives now (stacked effects of every reached level).
	DrawTextAt(TEXT("Сейчас даёт:"), HubTitle, X, Y, 1.1f * S);
	Y += 30.f * S;
	bool bAny = false;
	for (int32 L = 1; L <= Level; ++L)
	{
		for (const FCRHubEffect& Effect : Building.Levels[L - 1].Effects)
		{
			const FString Line = CRMeta::DescribeEffect(Effect);
			if (!Line.IsEmpty())
			{
				Y = DrawWrapped(TEXT("• ") + Line, HubText, X, Y, MaxW, 1.0f * S);
				bAny = true;
			}
		}
	}
	if (const FCRHubBuildingLevel* Current = Building.FindLevel(Level); Current && !bAny && !Current->Summary.IsEmpty())
	{
		Y = DrawWrapped(TEXT("• ") + Current->Summary, HubText, X, Y, MaxW, 1.0f * S);
		bAny = true;
	}
	if (!bAny)
	{
		DrawTextAt(TEXT("• пока ничего"), HubDim, X, Y, 1.0f * S);
		Y += 26.f * S;
	}
	Y += 12.f * S;

	// Next upgrade.
	const FCRHubBuildingLevel* Next = nullptr;
	const ECRUpgradeStatus Status = Profiles.GetUpgradeStatus(Building, &Next);
	if (Next)
	{
		DrawTextAt(FString::Printf(TEXT("Следующий уровень (%d):"), Level + 1), HubTitle, X, Y, 1.1f * S);
		Y += 30.f * S;
		Y = DrawWrapped(Next->Summary, HubText, X, Y, MaxW, 1.0f * S) + 4.f * S;
		DrawTextAt(TEXT("Стоимость:"), HubDim, X, Y, 1.0f * S);
		DrawCost(Next->Cost, Profile->Resources, X + TextWidth(TEXT("Стоимость:  "), 1.0f * S), Y, 1.0f * S);
		Y += 28.f * S;
		if (Status == ECRUpgradeStatus::Locked)
		{
			const UCRHubBuildingDefinition* Required = Profiles.GetCatalog()->FindBuilding(Next->RequiredBuildingId);
			Y = DrawWrapped(FString::Printf(TEXT("Требуется: «%s», уровень %d"), Required ? *Required->DisplayName : TEXT("?"), Next->RequiredLevel),
				HubBad, X, Y, MaxW, 1.0f * S);
		}
		Y += 8.f * S;
		const FString Label = Level <= 0 ? FString(TEXT("ПОСТРОИТЬ")) : FString::Printf(TEXT("УЛУЧШИТЬ ДО УРОВНЯ %d"), Level + 1);
		DrawButton(FBox2D(FVector2D(X, Y), FVector2D(X + MaxW, Y + 56.f * S)), Label, ActionUpgrade, Building.BuildingId.ToString(), true,
			Status == ECRUpgradeStatus::Available ? ECRUIButtonStyle::Primary : ECRUIButtonStyle::Normal);
		Y += 72.f * S;
	}
	else
	{
		DrawTextAt(TEXT("Достигнут максимальный уровень"), HubGood, X, Y, 1.1f * S);
		Y += 40.f * S;
	}

	// Conversions of the current level (the Storage's Silver -> Wood).
	if (const TArray<FCRHubConversion>* Conversions = CRMeta::GetConversions(Profile->BuildingLevels, Building); Conversions && Conversions->Num() > 0)
	{
		DrawTextAt(TEXT("Обмен ресурсов:"), HubTitle, X, Y, 1.1f * S);
		Y += 32.f * S;
		for (const FCRHubConversion& Conversion : *Conversions)
		{
			const bool bAffordable = Profile->Resources.CanAfford(Conversion.Input);
			const FString Sub = FString::Printf(TEXT("%s → %s"), *CRMeta::FormatResources(Conversion.Input), *CRMeta::FormatResources(Conversion.Output));
			DrawButton(FBox2D(FVector2D(X, Y), FVector2D(X + MaxW, Y + 58.f * S)), Conversion.DisplayName, ActionConvert,
				Building.BuildingId.ToString() + TEXT("|") + Conversion.ConversionId.ToString(), true,
				bAffordable ? ECRUIButtonStyle::Normal : ECRUIButtonStyle::Subtle, Sub);
			Y += 66.f * S;
		}
	}

	if (!Building.FutureNote.IsEmpty() && Y < Bottom - 60.f * S)
	{
		DrawWrapped(TEXT("В будущем: ") + Building.FutureNote, HubDim, X, FMath::Max(Y + 6.f * S, Bottom - 70.f * S), MaxW, 0.9f * S);
	}
}

void ACRHubHUD::DrawBottomBar(const UCRProfileSubsystem& Profiles)
{
	const UCRProfileSaveGame* Profile = Profiles.GetActiveProfile();
	const float H = Canvas->ClipY;

	// Bottom center (of the scene area): the expedition button and what the run will get from the hub.
	const float SceneCenterX = (Canvas->ClipX - 600.f * S) * 0.5f;
	const float BtnW = 380.f * S;
	DrawButton(FBox2D(FVector2D(SceneCenterX - BtnW * 0.5f, H - 150.f * S), FVector2D(SceneCenterX + BtnW * 0.5f, H - 62.f * S)),
		TEXT("В ПОХОД"), ActionStartRun, FString(), true, ECRUIButtonStyle::Primary, TEXT("начать новый забег"));

	const FCRRunStartBonuses Bonuses = Profiles.GetRunStartBonuses();
	TArray<FString> Parts;
	Parts.Add(FString::Printf(TEXT("здоровье %d"), CRRun::BaseHamsterMaxHP + Bonuses.BonusMaxHP));
	Parts.Add(FString::Printf(TEXT("колода: %d карт"), CRRun::StarterDeck().Num() + Bonuses.ExtraCardIds.Num()));
	if (Bonuses.StartSilver > 0)
	{
		Parts.Add(FString::Printf(TEXT("серебро %d"), Bonuses.StartSilver));
	}
	if (Bonuses.StartFood > 0)
	{
		Parts.Add(FString::Printf(TEXT("еда %d"), Bonuses.StartFood));
	}
	DrawTextShadowCentered(TEXT("В поход: ") + FString::Join(Parts, TEXT(" · ")), HubText, SceneCenterX, H - 46.f * S, 1.0f * S);

	// Bottom left: active profile.
	const float X = 28.f * S;
	DrawBox(FBox2D(FVector2D(X - 12.f * S, H - 150.f * S), FVector2D(X + 330.f * S, H - 24.f * S)), HubPanel);
	DrawTextAt(TEXT("Профиль"), HubDim, X, H - 142.f * S, 1.0f * S);
	DrawTextAt(Profile->DisplayName, HubText, X, H - 116.f * S, FitScale(Profile->DisplayName, 1.35f * S, 310.f * S));
	DrawButton(FBox2D(FVector2D(X, H - 78.f * S), FVector2D(X + 310.f * S, H - 34.f * S)), TEXT("СМЕНИТЬ ПРОФИЛЬ"), ActionMenu);

	// Bottom right: secondary screens (placeholders for now) and exit to the main menu.
	const TCHAR* Labels[] = { TEXT("Дневник"), TEXT("Достижения"), TEXT("Настройки"), TEXT("Выход") };
	const float SmallW = 128.f * S;
	const float Gap = 10.f * S;
	float BX = Canvas->ClipX - 28.f * S - 4.f * SmallW - 3.f * Gap;
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bExit = i == 3;
		DrawButton(FBox2D(FVector2D(BX, H - 100.f * S), FVector2D(BX + SmallW, H - 50.f * S)), Labels[i], bExit ? ActionMenu : ActionPlaceholder,
			Labels[i], true, bExit ? ECRUIButtonStyle::Normal : ECRUIButtonStyle::Subtle);
		BX += SmallW + Gap;
	}
}

void ACRHubHUD::OnButton(const FCRUIButton& Button)
{
	ACRHubGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRHubGameMode>() : nullptr;
	UCRProfileSubsystem* Profiles = GM ? GM->GetProfiles() : nullptr;
	if (!GM || !Profiles)
	{
		return;
	}

	if (Button.Action == ActionSelect)
	{
		SelectedBuildingId = FName(*Button.Arg);
	}
	else if (Button.Action == ActionUpgrade)
	{
		FString Feedback;
		const bool bOk = Profiles->TryUpgrade(FName(*Button.Arg), Feedback);
		ShowMessage(Feedback, !bOk);
	}
	else if (Button.Action == ActionConvert)
	{
		FString BuildingId;
		FString ConversionId;
		Button.Arg.Split(TEXT("|"), &BuildingId, &ConversionId);
		FString Feedback;
		const bool bOk = Profiles->TryConvert(FName(*BuildingId), FName(*ConversionId), Feedback);
		ShowMessage(Feedback, !bOk);
	}
	else if (Button.Action == ActionStartRun)
	{
		if (!GM->StartRun())
		{
			ShowMessage(TEXT("Не удалось начать поход"), true);
		}
	}
	else if (Button.Action == ActionMenu)
	{
		GM->OpenMainMenu();
	}
	else if (Button.Action == ActionPlaceholder)
	{
		ShowMessage(FString::Printf(TEXT("Раздел «%s» появится позже"), *Button.Arg));
	}
}
