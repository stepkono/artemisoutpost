// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerCuesManager.h"

#include "EnhancedInputComponent.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/MinigameActor.h"
#include "ArtemisOutpost/MiniGames/MiniGamePuppet/MinigamePuppet.h"
#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/Player/PawnAR/PawnAR.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "ArtemisOutpost/Player/PawnVR/VRCharPuppet.h"
#include "ArtemisOutpost/Player/PawnVR/ControllerRays/ControllerRayComponent.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "ArtemisOutpost/Player/Rover/AMasterRover.h"
#include "ArtemisOutpost/Player/Rover/PuppetRover.h"

namespace
{
	EControllerRayHand ToRayHand(EPointingHand Hand)
	{
		return Hand == EPointingHand::Left ? EControllerRayHand::Left : EControllerRayHand::Right;
	}
}

UPlayerCuesManager::UPlayerCuesManager()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Nothing on this component replicates: the shared state lives on AArtemisPlayerState, the
	// client -> server path is the APawnController RPCs.
	SetIsReplicatedByDefault(false);
}

void UPlayerCuesManager::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayerCuesManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AArtemisPlayerState* PS = CachedPlayerState.Get())
	{
		PS->OnCueStateChanged.RemoveDynamic(this, &UPlayerCuesManager::HandleCueStateChanged);
	}
	CachedPlayerState.Reset();
	bBoundToPlayerState = false;

	Super::EndPlay(EndPlayReason);
}

void UPlayerCuesManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	EnsurePlayerState();

	// Server role (listen server AND standalone, which is its own authority): derive Walking.
	if (Owner->HasAuthority())
	{
		TickServerWalking(DeltaTime);
	}

	// The listen-server host has no headset and no local player of its own: nothing else to do.
	if (ArtemisNet::IsServerHost(GetNetMode()))
	{
		return;
	}

	if (IsLocalActivePawn())
	{
		TickLocalPointer(DeltaTime);
		TickLocalGaze(DeltaTime);
	}
	else if (bLocalPointing)
	{
		// The player switched context while holding the pointer: end it cleanly.
		SetPointing(LocalPointingHand, false);
	}
}

// ---- Local-player identity ----

APawnController* UPlayerCuesManager::GetLocalController() const
{
	return Cast<APawnController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
}

bool UPlayerCuesManager::IsLocalActivePawn() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return false;
	}

	// Possession is the primary signal (the mode switch re-possesses on every AR/VR change). The mode
	// check only covers the frames between the variable flipping and the possession landing.
	const APawnController* PC = Cast<APawnController>(Pawn->GetController());
	if (!PC)
	{
		return true; // test level without our controller
	}
	return bUsesARMoon ? (PC->GetXRMode() == EXRMode::AR) : (PC->GetXRMode() == EXRMode::VR);
}

bool UPlayerCuesManager::MatchesContext(EPlayerContext Context) const
{
	return bUsesARMoon ? (Context == EPlayerContext::AR) : (Context == EPlayerContext::VR);
}

// ---- Input ----

void UPlayerCuesManager::BindInput(UEnhancedInputComponent* EIC)
{
	if (!EIC)
	{
		return;
	}

	if (PointerLeftAction)
	{
		EIC->BindAction(PointerLeftAction, ETriggerEvent::Started,   this, &UPlayerCuesManager::HandlePointerLeftStarted);
		EIC->BindAction(PointerLeftAction, ETriggerEvent::Completed, this, &UPlayerCuesManager::HandlePointerLeftCompleted);
	}
	if (PointerRightAction)
	{
		EIC->BindAction(PointerRightAction, ETriggerEvent::Started,   this, &UPlayerCuesManager::HandlePointerRightStarted);
		EIC->BindAction(PointerRightAction, ETriggerEvent::Completed, this, &UPlayerCuesManager::HandlePointerRightCompleted);
	}
	if (TalkAction)
	{
		EIC->BindAction(TalkAction, ETriggerEvent::Started,   this, &UPlayerCuesManager::HandleTalkStarted);
		EIC->BindAction(TalkAction, ETriggerEvent::Completed, this, &UPlayerCuesManager::HandleTalkCompleted);
	}

	UE_LOG(LogTemp, Log, TEXT("[Cues] %s bound cue input (pointerL=%s pointerR=%s talk=%s)."),
		*GetNameSafe(GetOwner()),
		PointerLeftAction ? TEXT("yes") : TEXT("no"), PointerRightAction ? TEXT("yes") : TEXT("no"), TalkAction ? TEXT("yes") : TEXT("no"));
}

void UPlayerCuesManager::HandlePointerInput(EPointingHand Hand, bool bOn)
{
	UE_LOG(LogTemp, Log, TEXT("[Cues] %s: pointer input %s=%d, activePawn=%d, rayComp=%s."),
		*GetNameSafe(GetOwner()), Hand == EPointingHand::Left ? TEXT("Left") : TEXT("Right"), bOn ? 1 : 0,
		IsLocalActivePawn() ? 1 : 0, GetRayComponent() ? TEXT("yes") : TEXT("NONE"));

	if (!IsLocalActivePawn())
	{
		// A release must still end a press that started on this pawn.
		if (!bOn && bLocalPointing)
		{
			SetPointing(Hand, false);
		}
		return;
	}
	SetPointing(Hand, bOn);
}

void UPlayerCuesManager::HandleTalkInput(bool bOn)
{
	if (!IsLocalActivePawn() && bOn)
	{
		return;
	}
	SetTalking(bOn);
}

// ---- Local intents ----

void UPlayerCuesManager::SetPointing(EPointingHand Hand, bool bOn)
{
	if (bOn)
	{
		if (bLocalPointing && LocalPointingHand == Hand)
		{
			return;
		}
		bLocalPointing = true;
		LocalPointingHand = Hand;
	}
	else
	{
		if (!bLocalPointing || LocalPointingHand != Hand)
		{
			return;
		}
		bLocalPointing = false;
	}

	// Report the first target immediately after a press.
	PointerReportAccum = PointerReportInterval;
	bHasReportedPointer = false;
	LastReportedPointer = FPointingTarget();

	// Local visual right away (the replicated echo is idempotent).
	if (UControllerRayComponent* Ray = GetRayComponent())
	{
		Ray->SetPointerMode(EControllerRayHand::Left,  bLocalPointing && Hand == EPointingHand::Left);
		Ray->SetPointerMode(EControllerRayHand::Right, bLocalPointing && Hand == EPointingHand::Right);
	}

	if (APawnController* PC = GetLocalController())
	{
		PC->ServerSetPointing(Hand, bLocalPointing);
	}
}

void UPlayerCuesManager::SetTalking(bool bOn)
{
	if (bLocalTalking == bOn)
	{
		return;
	}
	bLocalTalking = bOn;

	if (APawnController* PC = GetLocalController())
	{
		PC->ServerSetTalking(bOn);
	}
}

void UPlayerCuesManager::ReportToolActivity(EToolActivity Tool)
{
	if (APawnController* PC = GetLocalController())
	{
		PC->ServerReportToolActivity(Tool);
	}
}

// ---- Local pointer ----

void UPlayerCuesManager::TickLocalPointer(float DeltaTime)
{
	if (!bLocalPointing)
	{
		return;
	}

	PointerReportAccum += DeltaTime;
	if (PointerReportAccum < PointerReportInterval)
	{
		return;
	}
	PointerReportAccum = 0.0f;

	UControllerRayComponent* Ray = GetRayComponent();
	if (!Ray)
	{
		return;
	}

	FVector Start, Direction;
	FHitResult Hit;
	if (!Ray->GetRayHit(ToRayHand(LocalPointingHand), Start, Direction, Hit))
	{
		return;
	}

	// The same ray the laser draws: the WidgetInteractionComponent's last hit, now at pointer range.
	FPointingTarget Target;
	ResolveTarget(Hit, Target);

	if (!bReplicatePointerHit)
	{
		return;
	}

	const bool bChanged = !bHasReportedPointer
		|| !Target.IsSameTarget(LastReportedPointer)
		|| !Target.GeoHit.Equals(LastReportedPointer.GeoHit, 1e-7);
	if (!bChanged)
	{
		return;
	}

	LastReportedPointer = Target;
	bHasReportedPointer = true;

	if (APawnController* PC = GetLocalController())
	{
		PC->ServerReportPointerTarget(Target);
	}
}

// ---- Local gaze ----

void UPlayerCuesManager::TickLocalGaze(float DeltaTime)
{
	GazeTraceAccum += DeltaTime;
	if (GazeTraceAccum < GazeTraceInterval)
	{
		return;
	}
	const float SampleDt = GazeTraceAccum;
	GazeTraceAccum = 0.0f;

	UCameraComponent* Cam = GetGazeCamera();
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!Cam || !World || !Owner)
	{
		return;
	}

	const FVector Start = Cam->GetComponentLocation();
	const FVector End   = Start + Cam->GetForwardVector() * GazeTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlayerGazeTrace), /*bTraceComplex=*/false, Owner);
	TArray<AActor*> Attached;
	Owner->GetAttachedActors(Attached, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
	Params.AddIgnoredActors(Attached);

	FHitResult Hit;
	FPointingTarget Target;
	if (World->LineTraceSingleByChannel(Hit, Start, End, GazeTraceChannel, Params))
	{
		ResolveTarget(Hit, Target);
	}

	// Dwell: the same THING must be looked at for GazeDwellSeconds before it counts.
	if (Target.IsSameTarget(GazeCandidate))
	{
		GazeCandidateHeld += SampleDt;
		GazeCandidate.GeoHit = Target.GeoHit; // keep the freshest position for the report
	}
	else
	{
		GazeCandidate = Target;
		GazeCandidateHeld = 0.0f;
	}

	if (GazeCandidateHeld < GazeDwellSeconds || !bReplicateGazeHit)
	{
		return;
	}

	// Reported on target change only (the agreed rate policy); a Surface gaze that merely slides
	// across the ground is not streamed.
	if (GazeCandidate.IsSameTarget(LastReportedGaze))
	{
		return;
	}
	LastReportedGaze = GazeCandidate;

	if (APawnController* PC = GetLocalController())
	{
		PC->ServerReportGazeTarget(GazeCandidate);
	}
}

// ---- Server walking ----

void UPlayerCuesManager::TickServerWalking(float DeltaTime)
{
	AArtemisPlayerState* PS = CachedPlayerState.Get();
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!PS || !Owner || !World)
	{
		return;
	}

	// Only the VR character walks, and only while the player is in VR.
	if (bUsesARMoon)
	{
		return;
	}
	if (PS->GetCueState().Context != EPlayerContext::VR)
	{
		if (bServerWalking)
		{
			bServerWalking = false;
			PS->ServerSetWalking(false);
		}
		bHasServerSample = false;
		WalkStateTimer = 0.0f;
		return;
	}

	const double Now = World->GetTimeSeconds();
	const FVector Pos = Owner->GetActorLocation();
	if (!bHasServerSample)
	{
		LastServerPos = Pos;
		LastServerSampleTime = Now;
		bHasServerSample = true;
		return;
	}

	const double Dt = Now - LastServerSampleTime;
	if (Dt < WalkSampleInterval)
	{
		return;
	}

	const float Speed = static_cast<float>(FVector::Dist(Pos, LastServerPos) / Dt);
	LastServerPos = Pos;
	LastServerSampleTime = Now;

	const bool bMoving = Speed > WalkSpeedThreshold;
	if (bMoving != bServerWalking)
	{
		WalkStateTimer += static_cast<float>(Dt);
		if (WalkStateTimer >= WalkStateHysteresis)
		{
			bServerWalking = bMoving;
			WalkStateTimer = 0.0f;
			PS->ServerSetWalking(bMoving);
		}
	}
	else
	{
		WalkStateTimer = 0.0f;
	}
}

// ---- Target resolution ----

bool UPlayerCuesManager::ResolveTarget(const FHitResult& Hit, FPointingTarget& OutTarget)
{
	OutTarget = FPointingTarget();

	AActor* Actor = Hit.GetActor();
	const UPrimitiveComponent* Comp = Hit.GetComponent();
	if (!Actor && Comp)
	{
		Actor = Comp->GetOwner();
	}
	if (!Actor && !Comp)
	{
		return false; // nothing hit
	}

	// AR hits puppets, VR hits the real actors: unwrap to the thing they stand for.
	if (const AMinigamePuppet* Puppet = Cast<AMinigamePuppet>(Actor))
	{
		Actor = Puppet->GetOwner(); // spawned with Owner = the master minigame actor
	}
	else if (AVRCharPuppet* CharPuppet = Cast<AVRCharPuppet>(Actor))
	{
		Actor = CharPuppet->GetMaster();
	}
	else if (APuppetRover* RoverPuppet = Cast<APuppetRover>(Actor))
	{
		Actor = RoverPuppet->GetMaster();
	}

	OutTarget.GeoHit = WorldToGeo(FVector(Hit.ImpactPoint));

	if (AMinigameActor* Minigame = Cast<AMinigameActor>(Actor))
	{
		OutTarget.Kind         = EPointingTargetKind::Minigame;
		OutTarget.MinigameMGID = Minigame->GetMGID();
		OutTarget.MinigameType = Minigame->GetType();
		return true;
	}

	if (Actor)
	{
		if (const AArtemisPlayerState* PS = AArtemisPlayerState::FindForActor(GetWorld(), Actor))
		{
			OutTarget.Kind               = Actor->IsA<AMasterRover>() ? EPointingTargetKind::Rover : EPointingTargetKind::Player;
			OutTarget.TargetUPID         = PS->GetUPID();
			OutTarget.TargetPlayerNumber = PS->GetPlayerNumber();
			return true;
		}
	}

	// Moon tiles and anything else static: a place, not a thing.
	OutTarget.Kind = EPointingTargetKind::Surface;
	return true;
}

// ---- PlayerState plumbing ----

bool UPlayerCuesManager::EnsurePlayerState()
{
	if (bBoundToPlayerState)
	{
		if (CachedPlayerState.IsValid())
		{
			return true;
		}
		// The PlayerState was destroyed (reconnect): resolve and bind the new one.
		bBoundToPlayerState = false;
	}

	AArtemisPlayerState* PS = AArtemisPlayerState::FindForActor(GetWorld(), GetOwner());
	if (!PS)
	{
		if (const APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			PS = Pawn->GetPlayerState<AArtemisPlayerState>();
		}
	}
	if (!PS)
	{
		return false;
	}

	CachedPlayerState = PS;
	PS->OnCueStateChanged.AddUniqueDynamic(this, &UPlayerCuesManager::HandleCueStateChanged);
	bBoundToPlayerState = true;

	HandleCueStateChanged(PS->GetCueState());
	return true;
}

AArtemisPlayerState* UPlayerCuesManager::GetLinkedPlayerState() const
{
	return CachedPlayerState.Get();
}

void UPlayerCuesManager::HandleCueStateChanged(const FPlayerCueState& State)
{
	ApplyPointerVisual(State);
}

void UPlayerCuesManager::ApplyPointerVisual(const FPlayerCueState& State)
{
	// The host draws nothing, so do not even relay the state there (keeps the server log quiet).
	if (ArtemisNet::IsServerHost(GetNetMode()))
	{
		return;
	}

	UControllerRayComponent* Ray = GetRayComponent();
	if (!Ray)
	{
		return;
	}

	// The local active pawn already applied its own intent in SetPointing; the replicated echo may
	// carry a context that lags the local mode switch, so do not let it flicker the local laser.
	if (IsLocalActivePawn())
	{
		return;
	}

	const bool bShow = State.bPointing && MatchesContext(State.Context);
	Ray->SetPointerMode(EControllerRayHand::Left,  bShow && State.PointingHand == EPointingHand::Left);
	Ray->SetPointerMode(EControllerRayHand::Right, bShow && State.PointingHand == EPointingHand::Right);
}

// ---- Helpers ----

UControllerRayComponent* UPlayerCuesManager::GetRayComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UControllerRayComponent>() : nullptr;
}

UCameraComponent* UPlayerCuesManager::GetGazeCamera()
{
	if (CachedGazeCamera)
	{
		return CachedGazeCamera;
	}
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<UCameraComponent*> Cameras;
	Owner->GetComponents<UCameraComponent>(Cameras);
	for (UCameraComponent* Cam : Cameras)
	{
		if (Cam && Cam->ComponentHasTag(GazeCameraTag))
		{
			CachedGazeCamera = Cam;
			return Cam;
		}
	}
	if (Cameras.Num() > 0)
	{
		CachedGazeCamera = Cameras[0];
	}
	return CachedGazeCamera;
}

AGeoRefsManager* UPlayerCuesManager::GetGeoRefsManager()
{
	if (CachedGeoRefs)
	{
		return CachedGeoRefs;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AGeoRefsManager> It(World); It; ++It)
		{
			CachedGeoRefs = *It;
			break;
		}
	}
	return CachedGeoRefs;
}

FVector UPlayerCuesManager::WorldToGeo(const FVector& World)
{
	AGeoRefsManager* GRM = GetGeoRefsManager();
	if (!GRM)
	{
		return FVector::ZeroVector;
	}
	return bUsesARMoon ? GRM->UECoordsToARMoonCoords(World) : GRM->UECoordsToVRMoonCoords(World);
}
