// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceVeinActor.generated.h"

class UResourceVeinSpline;

/**
 * Level actor that carries one resource vein. Root is a UResourceVeinSpline you draw
 * directly in the level; OnConstruction rebuilds the vein mesh every time a spline
 * point moves, so authoring is fully visual (assign a mesh, drag the points).
 *
 * Discovery/mining state and its replication are intentionally NOT here yet — this
 * actor is the authoring + rendering half of the vein.
 */
UCLASS()
class ARTEMISOUTPOST_API AResourceVeinActor : public AActor
{
	GENERATED_BODY()

public:
	AResourceVeinActor();

	// Runs in the editor on every spline edit (and on spawn) -> live mesh rebuild.
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vein")
	UResourceVeinSpline* VeinSpline;
};
