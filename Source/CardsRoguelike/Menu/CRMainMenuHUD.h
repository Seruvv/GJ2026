// Main menu screen: profile list (select), Continue, New profile (name dialog), Delete (click twice), Exit.

#pragma once

#include "CoreMinimal.h"
#include "../UI/CRMenuHUDBase.h"
#include "Types/SlateEnums.h"
#include "CRMainMenuHUD.generated.h"

class SEditableTextBox;
class SWidget;

UCLASS()
class CARDSROGUELIKE_API ACRMainMenuHUD : public ACRMenuHUDBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawScreen() override;
	virtual void OnButton(const FCRUIButton& Button) override;

private:
	void OpenNameDialog();
	void CloseNameDialog();
	void CommitNameDialog();
	void OnNameCommitted(const FText& Text, ETextCommit::Type CommitType);

	FString SelectedProfileId;
	/** Profile whose delete button was clicked once (a second click deletes). */
	FString DeleteArmedProfileId;

	TSharedPtr<SWidget> NameDialog;
	TSharedPtr<SEditableTextBox> NameBox;
};
