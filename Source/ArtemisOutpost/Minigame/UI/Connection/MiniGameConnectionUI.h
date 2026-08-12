// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
class AMinigameActor;
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "MiniGameConnectionUI.generated.h"

/**
 * 
 */

UCLASS()
class ARTEMISOUTPOST_API UMiniGameConnectionUI : public UUserWidget
{
	GENERATED_BODY()
	
public: 
	void SetOwner(AMinigameActor* Owner);
	
	void SetButtonText(const FString& Text) const;
	
protected:
	UPROPERTY(BlueprintReadOnly, Category = "Owner")
	AMinigameActor* MiniGameOwner;
	
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category = "Widget Content")
	TObjectPtr<UTextBlock> TextBlock;
};
