// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerActionProviderData.h"

#include "Dom/JsonObject.h"

TSharedPtr<FJsonObject> UPlayerActionProviderData::BuildJsonFromData(const EGameEventType GameEventType)
{
	switch (GameEventType)
	{
		case HMDDonned:
		case HMDDoffed:
			return SerializeHmdState(GameEventType);

		case PlayerContextChanged:
		case PlayerActivityChanged:
		case PlayerPointingStart:
		case PlayerPointingUpdate:
		case PlayerPointingFinish:
		case PlayerLookAtStart:
		case PlayerLookAtFinish:
		case PlayerTalkStart:
		case PlayerTalkFinish:
			return SerializeCueEvent(GameEventType);

		default:
			{
				UE_LOG(LogTemp, Error, TEXT("PlayerActionProviderData: BuildJsonFromData received an unhandled GameEventType. Returning null."));
				return nullptr;
			}
	}
}

TSharedPtr<FJsonObject> UPlayerActionProviderData::SerializeHmdState(const EGameEventType GameEventType) const
{
	const TSharedPtr<FJsonObject> DataObject = MakeShared<FJsonObject>();
	DataObject->SetStringField(TEXT("upid"),    UPID);
	DataObject->SetStringField(TEXT("event"),   StaticEnum<EGameEventType>()->GetNameStringByValue(GameEventType));
	DataObject->SetBoolField(TEXT("hmdWorn"),   bWorn);
	return DataObject;
}

TSharedPtr<FJsonObject> UPlayerActionProviderData::SerializeCueEvent(const EGameEventType GameEventType) const
{
	const TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("upid"),         UPID);
	Obj->SetStringField(TEXT("event"),        StaticEnum<EGameEventType>()->GetNameStringByValue(GameEventType));
	Obj->SetBoolField(TEXT("hmdWorn"),        CueState.bHmdWorn);
	Obj->SetStringField(TEXT("context"),      StaticEnum<EPlayerContext>()->GetNameStringByValue(static_cast<int64>(CueState.Context)));
	Obj->SetStringField(TEXT("activity"),     StaticEnum<EPlayerActivity>()->GetNameStringByValue(static_cast<int64>(CueState.GetActivity())));
	Obj->SetBoolField(TEXT("inMinigame"),     CueState.bInMinigame);
	Obj->SetStringField(TEXT("minigameType"), CueState.bInMinigame
		? StaticEnum<EMiniGameType>()->GetNameStringByValue(static_cast<int64>(CueState.MinigameType))
		: FString());
	Obj->SetStringField(TEXT("tool"),         StaticEnum<EToolActivity>()->GetNameStringByValue(static_cast<int64>(CueState.ToolActivity)));
	Obj->SetBoolField(TEXT("walking"),        CueState.bWalking);
	Obj->SetBoolField(TEXT("talking"),        CueState.bTalking);
	Obj->SetBoolField(TEXT("pointing"),       CueState.bPointing);
	Obj->SetStringField(TEXT("hand"),         StaticEnum<EPointingHand>()->GetNameStringByValue(static_cast<int64>(CueState.PointingHand)));
	Obj->SetObjectField(TEXT("pointerTarget"), SerializeTarget(CueState.PointerTarget));
	Obj->SetObjectField(TEXT("gazeTarget"),    SerializeTarget(CueState.GazeTarget));
	return Obj;
}

TSharedPtr<FJsonObject> UPlayerActionProviderData::SerializeTarget(const FPointingTarget& Target)
{
	const TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("kind"), StaticEnum<EPointingTargetKind>()->GetNameStringByValue(static_cast<int64>(Target.Kind)));
	if (Target.Kind == EPointingTargetKind::None)
	{
		return Obj;
	}

	Obj->SetObjectField(TEXT("geo"), SerializeGeo(Target.GeoHit));

	switch (Target.Kind)
	{
	case EPointingTargetKind::Minigame:
		Obj->SetStringField(TEXT("mgid"), Target.MinigameMGID.ToString(EGuidFormats::DigitsWithHyphens));
		Obj->SetStringField(TEXT("minigameType"), StaticEnum<EMiniGameType>()->GetNameStringByValue(static_cast<int64>(Target.MinigameType)));
		break;
	case EPointingTargetKind::Player:
	case EPointingTargetKind::Rover:
		Obj->SetStringField(TEXT("upid"), Target.TargetUPID);
		Obj->SetNumberField(TEXT("playerNumber"), Target.TargetPlayerNumber);
		break;
	default:
		break;
	}
	return Obj;
}

TSharedPtr<FJsonObject> UPlayerActionProviderData::SerializeGeo(const FVector& LonLatHeight)
{
	const TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("lon"),    LonLatHeight.X);
	Obj->SetNumberField(TEXT("lat"),    LonLatHeight.Y);
	Obj->SetNumberField(TEXT("height"), LonLatHeight.Z);
	return Obj;
}
