#include "CRMainMenuHUD.h"

#include "../Meta/CRProfileSubsystem.h"
#include "../UI/CRUIPlayerController.h"
#include "CRMainMenuGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor MenuPanel(0.03f, 0.03f, 0.045f, 0.88f);
	const FLinearColor MenuTitle(1.f, 0.84f, 0.45f);
	const FLinearColor MenuText(0.92f, 0.91f, 0.88f);
	const FLinearColor MenuDim(0.62f, 0.62f, 0.66f);

	const FName ActionSelectProfile = TEXT("SelectProfile");
	const FName ActionContinue = TEXT("Continue");
	const FName ActionNew = TEXT("New");
	const FName ActionDelete = TEXT("Delete");
	const FName ActionExit = TEXT("Exit");

	constexpr int32 MaxVisibleProfiles = 7;

	FString FormatDate(const FDateTime& Time)
	{
		return Time.ToString(TEXT("%d.%m.%Y %H:%M"));
	}
}

void ACRMainMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	if (const ACRMainMenuGameMode* GM = GetWorld()->GetAuthGameMode<ACRMainMenuGameMode>())
	{
		if (const UCRProfileSubsystem* Profiles = GM->GetProfiles())
		{
			SelectedProfileId = Profiles->GetLastSelectedProfileId();
			if (SelectedProfileId.IsEmpty() && Profiles->GetProfiles().Num() > 0)
			{
				SelectedProfileId = Profiles->GetProfiles()[0].ProfileId;
			}
		}
	}
}

void ACRMainMenuHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseNameDialog();
	Super::EndPlay(EndPlayReason);
}

void ACRMainMenuHUD::DrawScreen()
{
	const ACRMainMenuGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRMainMenuGameMode>() : nullptr;
	const UCRProfileSubsystem* Profiles = GM ? GM->GetProfiles() : nullptr;
	if (!Profiles)
	{
		return;
	}
	const TArray<FCRProfileSummary>& List = Profiles->GetProfiles();
	if (!List.ContainsByPredicate([this](const FCRProfileSummary& P) { return P.ProfileId == SelectedProfileId; }))
	{
		SelectedProfileId = List.Num() > 0 ? List[0].ProfileId : FString();
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	DrawBox(FBox2D(FVector2D(0.f, 0.f), FVector2D(Canvas->ClipX, Canvas->ClipY)), FLinearColor(0.f, 0.f, 0.f, 0.35f));
	DrawTextShadowCentered(TEXT("УБЕЖИЩЕ В БУТЫЛКЕ"), MenuTitle, CenterX, 90.f * S, 2.8f * S);
	DrawTextShadowCentered(TEXT("главное меню · прототип"), MenuDim, CenterX, 160.f * S, 1.2f * S);

	// Panel: profile list on the left, actions on the right.
	const float PanelW = 980.f * S;
	const float Left = CenterX - PanelW * 0.5f;
	const float Top = 230.f * S;
	const float Bottom = Top + 640.f * S;
	DrawBox(FBox2D(FVector2D(Left, Top), FVector2D(Left + PanelW, Bottom)), MenuPanel);
	DrawFrame(FBox2D(FVector2D(Left, Top), FVector2D(Left + PanelW, Bottom)), FLinearColor(0.5f, 0.42f, 0.25f), 2.f * S);

	const float ListX = Left + 30.f * S;
	const float ListW = 600.f * S;
	float Y = Top + 24.f * S;
	DrawTextAt(TEXT("ПРОФИЛИ"), MenuTitle, ListX, Y, 1.5f * S);
	Y += 50.f * S;
	if (List.Num() == 0)
	{
		DrawWrapped(TEXT("Профилей пока нет. Создайте новый профиль, чтобы войти в убежище."), MenuDim, ListX, Y, ListW, 1.15f * S);
	}
	for (int32 i = 0; i < List.Num() && i < MaxVisibleProfiles; ++i)
	{
		const FCRProfileSummary& Profile = List[i];
		const bool bSelected = Profile.ProfileId == SelectedProfileId;
		DrawButton(FBox2D(FVector2D(ListX, Y), FVector2D(ListX + ListW, Y + 70.f * S)), Profile.DisplayName, ActionSelectProfile, Profile.ProfileId, true,
			bSelected ? ECRUIButtonStyle::Primary : ECRUIButtonStyle::Normal,
			FString::Printf(TEXT("последняя игра: %s · создан: %s"), *FormatDate(Profile.LastPlayedAt), *Profile.CreatedAt.ToString(TEXT("%d.%m.%Y"))));
		Y += 78.f * S;
	}
	if (List.Num() > MaxVisibleProfiles)
	{
		DrawTextAt(FString::Printf(TEXT("…и ещё %d"), List.Num() - MaxVisibleProfiles), MenuDim, ListX, Y, 1.0f * S);
	}

	const float BtnX = ListX + ListW + 40.f * S;
	const float BtnW = Left + PanelW - 30.f * S - BtnX;
	const bool bHasSelection = !SelectedProfileId.IsEmpty();
	float BY = Top + 74.f * S;
	DrawButton(FBox2D(FVector2D(BtnX, BY), FVector2D(BtnX + BtnW, BY + 70.f * S)), TEXT("ПРОДОЛЖИТЬ"), ActionContinue, SelectedProfileId, bHasSelection,
		ECRUIButtonStyle::Primary, TEXT("войти в убежище"));
	BY += 86.f * S;
	DrawButton(FBox2D(FVector2D(BtnX, BY), FVector2D(BtnX + BtnW, BY + 70.f * S)), TEXT("НОВЫЙ ПРОФИЛЬ"), ActionNew);
	BY += 86.f * S;
	const bool bArmed = bHasSelection && DeleteArmedProfileId == SelectedProfileId;
	DrawButton(FBox2D(FVector2D(BtnX, BY), FVector2D(BtnX + BtnW, BY + 70.f * S)), bArmed ? TEXT("ТОЧНО УДАЛИТЬ?") : TEXT("УДАЛИТЬ ПРОФИЛЬ"),
		ActionDelete, SelectedProfileId, bHasSelection, bArmed ? ECRUIButtonStyle::Danger : ECRUIButtonStyle::Normal,
		bArmed ? TEXT("нажмите ещё раз для подтверждения") : FString());
	DrawButton(FBox2D(FVector2D(BtnX, Bottom - 94.f * S), FVector2D(BtnX + BtnW, Bottom - 24.f * S)), TEXT("ВЫХОД"), ActionExit, FString(), true,
		ECRUIButtonStyle::Subtle, TEXT("закрыть игру"));

	DrawMessage(Bottom + 30.f * S);
}

void ACRMainMenuHUD::OnButton(const FCRUIButton& Button)
{
	ACRMainMenuGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRMainMenuGameMode>() : nullptr;
	UCRProfileSubsystem* Profiles = GM ? GM->GetProfiles() : nullptr;
	if (!GM || !Profiles || NameDialog.IsValid())
	{
		return;
	}

	if (Button.Action != ActionDelete)
	{
		DeleteArmedProfileId.Reset();
	}

	if (Button.Action == ActionSelectProfile)
	{
		SelectedProfileId = Button.Arg;
	}
	else if (Button.Action == ActionContinue)
	{
		if (!GM->ContinueWithProfile(Button.Arg))
		{
			ShowMessage(TEXT("Не удалось загрузить профиль"), true);
		}
	}
	else if (Button.Action == ActionNew)
	{
		OpenNameDialog();
	}
	else if (Button.Action == ActionDelete)
	{
		if (DeleteArmedProfileId != Button.Arg)
		{
			DeleteArmedProfileId = Button.Arg;
			return;
		}
		const FCRProfileSummary* Summary = Profiles->GetProfiles().FindByPredicate([&Button](const FCRProfileSummary& P) { return P.ProfileId == Button.Arg; });
		const FString Name = Summary ? Summary->DisplayName : FString();
		DeleteArmedProfileId.Reset();
		if (Profiles->DeleteProfile(Button.Arg))
		{
			ShowMessage(FString::Printf(TEXT("Профиль «%s» удалён"), *Name));
			SelectedProfileId = Profiles->GetLastSelectedProfileId();
		}
		else
		{
			ShowMessage(TEXT("Не удалось удалить профиль"), true);
		}
	}
	else if (Button.Action == ActionExit)
	{
		GM->QuitGame();
	}
}

void ACRMainMenuHUD::OpenNameDialog()
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const ACRMainMenuGameMode* GM = GetWorld()->GetAuthGameMode<ACRMainMenuGameMode>();
	if (!Viewport || !GM || NameDialog.IsValid())
	{
		return;
	}

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);
	const FString DefaultName = GM->GetProfiles() ? GM->GetProfiles()->MakeDefaultProfileName() : FString();

	NameDialog = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.06f, 0.97f))
			.Padding(FMargin(28.f, 22.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Новый профиль"))).Font(TitleFont).ColorAndOpacity(MenuTitle)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Имя хранителя убежища:"))).Font(BodyFont).ColorAndOpacity(MenuText)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
				[
					SNew(SBox).WidthOverride(460.f)
					[
						SAssignNew(NameBox, SEditableTextBox)
						.Text(FText::FromString(DefaultName))
						.Font(BodyFont)
						.SelectAllTextWhenFocused(true)
						.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &ACRMainMenuHUD::OnNameCommitted))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { CommitNameDialog(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("  Создать  "))).Font(BodyFont)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { CloseNameDialog(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("  Отмена  "))).Font(BodyFont)
						]
					]
				]
			]
		];

	Viewport->AddViewportWidgetContent(NameDialog.ToSharedRef(), 100);
	if (APlayerController* PC = GetOwningPlayerController())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(NameBox);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
	FSlateApplication::Get().SetKeyboardFocus(NameBox, EFocusCause::SetDirectly);
}

void ACRMainMenuHUD::CloseNameDialog()
{
	if (!NameDialog.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(NameDialog.ToSharedRef());
	}
	NameDialog.Reset();
	NameBox.Reset();
	if (ACRUIPlayerController* PC = Cast<ACRUIPlayerController>(GetOwningPlayerController()))
	{
		PC->RestoreDefaultInput();
	}
}

void ACRMainMenuHUD::OnNameCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		CommitNameDialog();
	}
}

void ACRMainMenuHUD::CommitNameDialog()
{
	ACRMainMenuGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACRMainMenuGameMode>() : nullptr;
	const FString Name = NameBox.IsValid() ? NameBox->GetText().ToString() : FString();
	CloseNameDialog();
	if (!GM || !GM->CreateProfileAndEnter(Name))
	{
		ShowMessage(TEXT("Не удалось создать профиль"), true);
	}
}
