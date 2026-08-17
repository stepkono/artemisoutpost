// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MiniGameConnectionUIComponent.h"
class AMinigameActor;
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "MiniGameConnectionUI.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConnectClicked); 

UCLASS()
class ARTEMISOUTPOST_API UMiniGameConnectionUI : public UUserWidget
{
	GENERATED_BODY()
	
public: 
	void SetButtonText(const FString& Text) const;
	
public:
	UPROPERTY(BlueprintCallable, Category = "Connection Widget")
	FOnConnectClicked OnConnectClicked;
	
protected:
	UPROPERTY(BlueprintReadOnly, Category = "Connection Widget")
	UMiniGameConnectionUIComponent* WidgetComponent;
	
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category = "Connection Widget")
	TObjectPtr<UTextBlock> TextBlock;
};
