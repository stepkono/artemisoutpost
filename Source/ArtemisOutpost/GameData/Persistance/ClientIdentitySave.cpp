// Fill out your copyright notice in the Description page of Project Settings.


#include "ClientIdentitySave.h"

void UClientIdentitySave::WriteUPID(FString NewUPID)
{
	UPID = NewUPID;
}

FString UClientIdentitySave::GetUPID()
{
	return UPID;
}
