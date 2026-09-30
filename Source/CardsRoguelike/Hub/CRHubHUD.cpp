#include "CRHubHUD.h"

#include "../Meta/CRProfileSubsystem.h"
#include "CRHubGameMode.h"
#include "CRHubRoomActor.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
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
	const FName ActionSelectHamster = TEXT("SelectHamster");
	const FName ActionOpenGraveyard = TEXT("OpenGraveyard");
	const FName ActionOpenRecruitment = TEXT("OpenRecruitment");
	const FName ActionBack = TEXT("Back");
	const FName ActionSelectGrave = TEXT("SelectGrave");
	const FName ActionRecruit = TEXT("Recruit");

	const FLinearColor HubEpitaph(0.96f, 0.88f, 0.7f);
	const FLinearColor HubMana(0.55f, 0.75f, 1.f);

	// Layout at 1080p (scaled by S): scene | details panel | roster, right to left.
	constexpr float HubMargin = 28.f;
	constexpr float HubRosterW = 300.f;
	constexpr float HubDetailsW = 400.f;
	constexpr float HubPanelGap = 16.f;
	constexpr float HubPanelTop = 160.f;
	constexpr float HubPanelBottomInset = 130.f;
	constexpr float HubRosterRowH = 92.f;
	constexpr float HubRosterRowGap = 8.f;
	constexpr float HubRosterHeaderH = 56.f;
	/** Recruitment button at the bottom of the roster panel. */
	constexpr float HubRosterFooterH = 74.f;

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

	// Overlays replace the sanctuary UI entirely, so nothing underneath stays clickable.
	if (View == EHubView::Graveyard)
	{
		DrawGraveyard(*Profiles);
		DrawMessage(Canvas->ClipY - 120.f * S);
		return;
	}
	if (View == EHubView::Recruitment)
	{
		DrawRecruitment(*Profiles);
		DrawMessage(Canvas->ClipY - 120.f * S);
		return;
	}

	const FHubLayout Layout = ComputeLayout();
	DrawMarkers(*Profiles, *GM);
	DrawTopBar(*Profiles);
	if (const UCRHubBuildingDefinition* Selected = Profiles->GetCatalog()->FindBuilding(SelectedBuildingId))
	{
		DrawDetails(*Profiles, *Selected, Layout.Details);
	}
	DrawRoster(*Profiles, Layout.Roster);
	DrawBottomBar(*Profiles, Layout.SceneCenterX);
	// Feedback just above the expedition button, clear of the building markers.
	DrawMessage(Canvas->ClipY - 205.f * S);
}

ACRHubHUD::FHubLayout ACRHubHUD::ComputeLayout() const
{
	FHubLayout Layout;
	const float Top = HubPanelTop * S;
	const float Bottom = Canvas->ClipY - HubPanelBottomInset * S;
	const float RosterLeft = Canvas->ClipX - (HubMargin + HubRosterW) * S;
	const float DetailsLeft = RosterLeft - (HubPanelGap + HubDetailsW) * S;
	Layout.Roster = FBox2D(FVector2D(RosterLeft, Top), FVector2D(RosterLeft + HubRosterW * S, Bottom));
	Layout.Details = FBox2D(FVector2D(DetailsLeft, Top), FVector2D(DetailsLeft + HubDetailsW * S, Bottom));
	// Centered in the scene area, but never over the profile box at the bottom left (narrow aspect ratios).
	Layout.SceneCenterX = FMath::Max(DetailsLeft * 0.5f, (HubMargin + 330.f + 20.f + 190.f) * S);
	return Layout;
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
	// Kept narrow so the Heart marker behind it stays clear.
	const float TopW = 440.f * S;
	DrawBox(FBox2D(FVector2D(X - 12.f * S, Y - 10.f * S), FVector2D(X + TopW, Y + (bHasRun ? 150.f : 70.f) * S)), HubPanel);
	DrawTextAt(TEXT("УБЕЖИЩЕ В БУТЫЛКЕ"), HubTitle, X, Y, 2.0f * S);
	Y += 48.f * S;
	if (bHasRun)
	{
		const FCRRunEndSummary& Run = Profile->LastRun;
		// Names come from version-3 summaries; older ones fall back to the generic wording.
		FString Outcome;
		switch (Run.Reason)
		{
		case ECRRunEndReason::Completed:
			Outcome = Run.HamsterName.IsEmpty() ? FString(TEXT("Последний поход: хомяк вернулся в убежище")) : FString::Printf(TEXT("Хомяк вернулся из похода: %s"), *Run.HamsterName);
			break;
		case ECRRunEndReason::Failed:
			Outcome = Run.HamsterName.IsEmpty() ? FString(TEXT("Последний поход: хомяк погиб")) : FString::Printf(TEXT("Хомяк погиб в походе: %s"), *Run.HamsterName);
			break;
		default:
			Outcome = Run.HamsterName.IsEmpty() ? FString(TEXT("Последний поход: прерван")) : FString::Printf(TEXT("Поход прерван, хомяк жив: %s"), *Run.HamsterName);
			break;
		}
		DrawTextAt(Outcome, Run.Reason == ECRRunEndReason::Completed ? HubGood : HubBad, X, Y, FitScale(Outcome, 1.15f * S, TopW - 12.f * S));
		Y += 30.f * S;
		const FString Delivered = FString::Printf(TEXT("Доставлено в убежище: %s"), Run.Delivered.IsZero() ? TEXT("ничего") : *CRMeta::FormatGain(Run.Delivered));
		DrawTextAt(Delivered, HubText, X, Y, FitScale(Delivered, 1.05f * S, TopW - 12.f * S));
		Y += 28.f * S;
		if (Run.Delivered.Silver != Run.Carried.Silver || Run.Delivered.Food != Run.Carried.Food || Run.Delivered.Wood != Run.Carried.Wood)
		{
			const FString Lost = FString::Printf(TEXT("%s: %s"), Run.Reason == ECRRunEndReason::Completed ? TEXT("Добыто в походе") : TEXT("Потеряно в походе"),
				*CRMeta::FormatResources(Run.Carried));
			DrawTextAt(Lost, HubDim, X, Y, FitScale(Lost, 1.0f * S, TopW - 12.f * S));
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

void ACRHubHUD::DrawDetails(const UCRProfileSubsystem& Profiles, const UCRHubBuildingDefinition& Building, const FBox2D& Panel)
{
	const UCRProfileSaveGame* Profile = Profiles.GetActiveProfile();
	const float PanelW = Panel.GetSize().X;
	const float Left = Panel.Min.X;
	const float Top = Panel.Min.Y;
	const float Bottom = Panel.Max.Y;
	const float Pad = 18.f * S;
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

void ACRHubHUD::DrawBottomBar(const UCRProfileSubsystem& Profiles, float SceneCenterX)
{
	const UCRProfileSaveGame* Profile = Profiles.GetActiveProfile();
	const float H = Canvas->ClipY;

	// Bottom center (of the scene area): the expedition button and who goes, with the effective run stats
	// (hamster base stats + hub bonuses).
	FCRRunStartConfig Next;
	const bool bCanStart = Profiles.GetNextRunStartConfig(Next);
	FCRRunState Preview;
	if (bCanStart)
	{
		CRRun::ApplyStartConfig(Preview, Next);
	}
	const float BtnW = 380.f * S;
	const bool bNobodyAlive = Profiles.GetLivingHamsterCount() == 0;
	DrawButton(FBox2D(FVector2D(SceneCenterX - BtnW * 0.5f, H - 150.f * S), FVector2D(SceneCenterX + BtnW * 0.5f, H - 66.f * S)),
		TEXT("В ПОХОД"), ActionStartRun, FString(), bCanStart, ECRUIButtonStyle::Primary,
		bCanStart ? TEXT("начать новый забег") : (bNobodyAlive ? TEXT("некому идти в поход") : TEXT("выберите хомяка")));
	if (!bCanStart && bNobodyAlive)
	{
		const FString Line = TEXT("Некому идти в поход. Примите нового хомяка: «Пополнение».");
		DrawTextShadowCentered(Line, HubBad, SceneCenterX, H - 54.f * S, FitScale(Line, 1.15f * S, SceneCenterX * 2.f - 700.f * S));
	}
	if (bCanStart)
	{
		TArray<FString> Parts;
		Parts.Add(Preview.Hamster.Name);
		Parts.Add(FString::Printf(TEXT("Здоровье %d"), Preview.Hamster.MaxHP));
		Parts.Add(FString::Printf(TEXT("Мана %d"), Preview.Hamster.ManaPerTurn));
		Parts.Add(FString::Printf(TEXT("Колода %d"), Preview.DeckCardIds.Num()));
		if (Preview.Carried.Silver > 0)
		{
			Parts.Add(FString::Printf(TEXT("Серебро %d"), Preview.Carried.Silver));
		}
		if (Preview.Carried.Food > 0)
		{
			Parts.Add(FString::Printf(TEXT("Еда %d"), Preview.Carried.Food));
		}
		const FString Line = FString::Join(Parts, TEXT(" • "));
		DrawTextShadowCentered(Line, HubText, SceneCenterX, H - 54.f * S, FitScale(Line, 1.2f * S, SceneCenterX * 2.f - 700.f * S));
	}

	// Bottom left: active profile.
	const float X = 28.f * S;
	DrawBox(FBox2D(FVector2D(X - 12.f * S, H - 150.f * S), FVector2D(X + 330.f * S, H - 24.f * S)), HubPanel);
	DrawTextAt(TEXT("Профиль"), HubDim, X, H - 142.f * S, 1.0f * S);
	DrawTextAt(Profile->DisplayName, HubText, X, H - 116.f * S, FitScale(Profile->DisplayName, 1.35f * S, 310.f * S));
	DrawButton(FBox2D(FVector2D(X, H - 78.f * S), FVector2D(X + 310.f * S, H - 34.f * S)), TEXT("СМЕНИТЬ ПРОФИЛЬ"), ActionMenu);

	// Bottom right: the graveyard, secondary screens (placeholders for now) and exit to the main menu.
	const float SmallW = 128.f * S;
	const float Gap = 10.f * S;
	float BX = Canvas->ClipX - 28.f * S - 5.f * SmallW - 4.f * Gap;
	{
		// New deaths since the last visit are marked so the consequence is discoverable.
		const int32 Dead = Profile->Hamsters.Num() - Profiles.GetLivingHamsterCount();
		const int32 Unseen = Profiles.GetUnseenGraveCount();
		const FString Label = Dead > 0 ? FString::Printf(TEXT("Кладбище · %d"), Dead) : FString(TEXT("Кладбище"));
		DrawButton(FBox2D(FVector2D(BX, H - 100.f * S), FVector2D(BX + SmallW, H - 50.f * S)), Label, ActionOpenGraveyard, FString(), true,
			Unseen > 0 ? ECRUIButtonStyle::Danger : ECRUIButtonStyle::Normal, Unseen > 0 ? TEXT("новая запись") : FString());
		BX += SmallW + Gap;
	}
	const TCHAR* Labels[] = { TEXT("Дневник"), TEXT("Достижения"), TEXT("Настройки"), TEXT("Выход") };
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bExit = i == 3;
		DrawButton(FBox2D(FVector2D(BX, H - 100.f * S), FVector2D(BX + SmallW, H - 50.f * S)), Labels[i], bExit ? ActionMenu : ActionPlaceholder,
			Labels[i], true, bExit ? ECRUIButtonStyle::Normal : ECRUIButtonStyle::Subtle);
		BX += SmallW + Gap;
	}
}

void ACRHubHUD::DrawRoster(const UCRProfileSubsystem& Profiles, const FBox2D& Panel)
{
	const UCRProfileSaveGame* Profile = Profiles.GetActiveProfile();
	RosterRect = Panel;
	DrawBox(Panel, HubPanel);
	DrawFrame(Panel, FLinearColor(0.5f, 0.42f, 0.25f), 2.f * S);
	const float Pad = 12.f * S;

	// Living hamsters only (dead ones are in the graveyard; the epitaph is secret while alive and never shown here).
	const TArray<const FCRHamsterPersistentState*> Living = CRMeta::GetLivingHamsters(*Profile);
	const FString Header = FString::Printf(TEXT("ЖИВЫЕ ХОМЯКИ · %d"), Living.Num());
	DrawTextAt(Header, HubTitle, Panel.Min.X + Pad, Panel.Min.Y + 14.f * S, FitScale(Header, 1.45f * S, Panel.GetSize().X - Pad * 2.f));

	// Footer: recruitment (always reachable, also with an empty roster).
	const UCRRecruitmentDefinition* Recruitment = Profiles.GetRecruitment();
	const FBox2D RecruitRect(FVector2D(Panel.Min.X + Pad, Panel.Max.Y - (HubRosterFooterH - 10.f) * S), FVector2D(Panel.Max.X - Pad, Panel.Max.Y - Pad));
	const int32 Target = Recruitment ? Recruitment->TargetLivingRosterSize : 0;
	const bool bCanRecruit = Profiles.CanRecruit();
	DrawButton(RecruitRect, TEXT("ПОПОЛНЕНИЕ"), ActionOpenRecruitment, FString(), Recruitment != nullptr,
		bCanRecruit ? ECRUIButtonStyle::Primary : ECRUIButtonStyle::Normal,
		!Recruitment ? TEXT("недоступно") : (bCanRecruit ? FString::Printf(TEXT("свободных мест: %d"), Target - Living.Num()) : FString(TEXT("убежище заполнено"))));

	const float ListTop = Panel.Min.Y + HubRosterHeaderH * S;
	const float ListBottom = RecruitRect.Min.Y - 8.f * S;
	const float RowStep = (HubRosterRowH + HubRosterRowGap) * S;
	const int32 Visible = FMath::Max(1, FMath::FloorToInt((ListBottom - ListTop) / RowStep));
	RosterMaxScroll = FMath::Max(0, Living.Num() - Visible);
	RosterScroll = FMath::Clamp(RosterScroll, 0, RosterMaxScroll);
	if (Living.Num() == 0)
	{
		float Y = DrawWrapped(TEXT("Некому идти в поход."), HubBad, Panel.Min.X + Pad, ListTop + 10.f * S, Panel.GetSize().X - Pad * 2.f, 1.25f * S);
		DrawWrapped(TEXT("В убежище не осталось живых хомяков. Примите нового хомяка через «Пополнение»."), HubDim, Panel.Min.X + Pad, Y + 8.f * S,
			Panel.GetSize().X - Pad * 2.f, 1.0f * S);
		return;
	}

	for (int32 Row = 0; Row < Visible && RosterScroll + Row < Living.Num(); ++Row)
	{
		const FCRHamsterPersistentState& Hamster = *Living[RosterScroll + Row];
		const FBox2D Rect(FVector2D(Panel.Min.X + Pad, ListTop + Row * RowStep), FVector2D(Panel.Max.X - Pad, ListTop + Row * RowStep + HubRosterRowH * S));
		const bool bSelected = Hamster.HamsterId == Profile->SelectedHamsterId;
		const bool bHovered = RegisterButton(Rect, ActionSelectHamster, Hamster.HamsterId.ToString());

		// Selected: warm fill, gold frame and a marker bar; hover: lighter fill and a white frame.
		DrawBox(Rect, bSelected ? FLinearColor(0.34f, 0.24f, 0.09f, 0.95f) : (bHovered ? FLinearColor(0.2f, 0.2f, 0.25f, 0.95f) : FLinearColor(0.1f, 0.1f, 0.13f, 0.9f)));
		DrawFrame(Rect, bSelected ? FLinearColor(1.f, 0.8f, 0.35f) : (bHovered ? FLinearColor::White : FLinearColor(0.3f, 0.3f, 0.36f)), (bSelected ? 3.f : 2.f) * S);
		if (bSelected)
		{
			DrawBox(FBox2D(Rect.Min, FVector2D(Rect.Min.X + 6.f * S, Rect.Max.Y)), FLinearColor(1.f, 0.8f, 0.35f));
		}

		const float PortraitSize = HubRosterRowH * S - 16.f * S;
		const FBox2D Portrait(FVector2D(Rect.Min.X + 12.f * S, Rect.Min.Y + 8.f * S), FVector2D(Rect.Min.X + 12.f * S + PortraitSize, Rect.Min.Y + 8.f * S + PortraitSize));
		DrawPortrait(Hamster, Portrait);

		const float TextX = Portrait.Max.X + 12.f * S;
		const float TextW = Rect.Max.X - TextX - 8.f * S;
		DrawTextAt(Hamster.DisplayName, bSelected ? HubTitle : HubText, TextX, Rect.Min.Y + 8.f * S, FitScale(Hamster.DisplayName, 1.3f * S, TextW));
		DrawTextAt(FString::Printf(TEXT("Здоровье %d"), Hamster.BaseMaxHP), HubText, TextX, Rect.Min.Y + 40.f * S, 1.0f * S);
		DrawTextAt(FString::Printf(TEXT("Мана %d"), Hamster.BaseManaPerTurn), HubMana, TextX, Rect.Min.Y + 64.f * S, 1.0f * S);
		if (bSelected)
		{
			DrawTextAt(TEXT("в поход"), HubTitle, Rect.Max.X - TextWidth(TEXT("в поход"), 0.85f * S) - 10.f * S, Rect.Min.Y + 66.f * S, 0.85f * S);
		}
	}
	if (RosterMaxScroll > 0)
	{
		DrawTextCentered(FString::Printf(TEXT("%d–%d из %d · колесо мыши"), RosterScroll + 1, FMath::Min(Living.Num(), RosterScroll + Visible), Living.Num()),
			HubDim, Panel.GetCenter().X, ListBottom - 20.f * S, 0.85f * S);
	}
}

void ACRHubHUD::DrawPortrait(const FCRHamsterPersistentState& Hamster, const FBox2D& Rect)
{
	const FVector2D Size = Rect.GetSize();
	if (UTexture2D* Texture = GetAvatarTexture(Hamster))
	{
		DrawTexture(Texture, Rect.Min.X, Rect.Min.Y, Size.X, Size.Y, 0.f, 0.f, 1.f, 1.f);
		DrawFrame(Rect, Hamster.AvatarTint, 2.f * S);
		return;
	}
	// Placeholder: tinted frame, darker inner face and the initial.
	DrawBox(Rect, Hamster.AvatarTint);
	const FBox2D Inner(Rect.Min + FVector2D(4.f * S), Rect.Max - FVector2D(4.f * S));
	DrawBox(Inner, FLinearColor::LerpUsingHSV(Hamster.AvatarTint, FLinearColor::Black, 0.55f));
	const FString Initial = Hamster.DisplayName.Left(1).ToUpper();
	const float Scale = 2.4f * S;
	DrawTextCentered(Initial, Hamster.AvatarTint * 1.4f, Rect.GetCenter().X, Rect.GetCenter().Y - TextHeight(Scale) * 0.5f, Scale);
}

UTexture2D* ACRHubHUD::GetAvatarTexture(const FCRHamsterPersistentState& Hamster)
{
	if (const TObjectPtr<UTexture2D>* Cached = AvatarCache.Find(Hamster.HamsterId))
	{
		return *Cached;
	}
	UTexture2D* Texture = Hamster.AvatarTexture.IsNull() ? nullptr : Hamster.AvatarTexture.LoadSynchronous();
	AvatarCache.Add(Hamster.HamsterId, Texture);
	return Texture;
}

bool ACRHubHUD::HandleScroll(const FVector2D& ScreenPos, float Delta)
{
	if (!RosterRect.bIsValid || !RosterRect.IsInside(ScreenPos))
	{
		return false;
	}
	RosterScroll = FMath::Clamp(RosterScroll - (Delta > 0.f ? 1 : -1), 0, RosterMaxScroll);
	return true;
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
	else if (Button.Action == ActionSelectHamster)
	{
		Profiles->SelectHamster(FName(*Button.Arg));
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
