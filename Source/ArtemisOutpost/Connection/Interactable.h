// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class UConnectionComponent;

// Marker + accessor for anything a player can connect to (minigames today, simple world
// interactables later). A targeting probe on the VR character uses it to reach the connection
// registry without knowing the concrete task. C++-only interface.
UINTERFACE(MinimalAPI)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class ARTEMISOUTPOST_API IInteractable
{
	GENERATED_BODY()

public:
	virtual UConnectionComponent* GetConnectionComponent() const = 0;
};
