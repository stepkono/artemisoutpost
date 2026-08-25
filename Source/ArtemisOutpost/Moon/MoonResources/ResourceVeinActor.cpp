// Fill out your copyright notice in the Description page of Project Settings.


#include "ResourceVeinActor.h"

#include "ResourceVeinSpline.h"


AResourceVeinActor::AResourceVeinActor()
{
	// Purely event-driven (OnConstruction) — no per-frame work.
	PrimaryActorTick.bCanEverTick = false;

	// The vein's Mined state is server-authoritative and replicated to clients (drives the
	// mining visual). The actor itself never moves, so skip movement replication. Always
	// relevant: veins are sparse, static and must stay correct regardless of distance.
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	VeinSpline = CreateDefaultSubobject<UResourceVeinSpline>(TEXT("VeinSpline"));
	SetRootComponent(VeinSpline);
}

void AResourceVeinActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (VeinSpline)
	{
		VeinSpline->BuildMesh();
	}
}
