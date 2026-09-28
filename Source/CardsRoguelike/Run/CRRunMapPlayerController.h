// Run map input: hover highlights Available rooms, left click travels.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CRRunMapPlayerController.generated.h"

class ACRRunMapActor;
class ACRRunNodeActor;
class UCRRunSubsystem;

UCLASS()
class CARDSROGUELIKE_API ACRRunMapPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACRRunMapPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	bool IsDebugView() const { return bDebugView; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	ACRRunNodeActor* GetNodeUnderCursor() const;
	ACRRunMapActor* GetMapActor() const;
	UCRRunSubsystem* GetRunSubsystem() const;

	void OnLeftClick();
	void OnRestartRun();
	void OnToggleDebugView();

	bool bDebugView = false;
};
