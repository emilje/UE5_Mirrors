#include "CVrMirror.h"
#include "VrMirrorSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"
#include "Engine/TriggerBox.h"

ACVrMirror::ACVrMirror()
{
	PrimaryActorTick.bCanEverTick = true;
	SetTickGroup(TG_PostUpdateWork);
	SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	SetRootComponent(SceneRoot);

	MirrorMesh = CreateDefaultSubobject<UStaticMeshComponent>("MirrorMesh");
	MirrorMesh->SetupAttachment(GetRootComponent());

	SceneCaptureLeftEye = CreateDefaultSubobject<USceneCaptureComponent2D>("SceneCaptureLeftEye");
	SceneCaptureLeftEye->SetupAttachment(GetRootComponent());
	SceneCaptureLeftEye->bEnableClipPlane = true;
	SceneCaptureLeftEye->bCaptureEveryFrame = false;
	SceneCaptureLeftEye->bCaptureOnMovement = false;

	SceneCaptureRightEye = CreateDefaultSubobject<USceneCaptureComponent2D>("SceneCaptureRightEye");
	SceneCaptureRightEye->SetupAttachment(GetRootComponent());
	SceneCaptureRightEye->bEnableClipPlane = true;
	SceneCaptureRightEye->bCaptureEveryFrame = false;
	SceneCaptureRightEye->bCaptureOnMovement = false;

	InitialCaptureQuality = CaptureQuality;
}

void ACVrMirror::OnViewportResize(FViewport* Viewport, uint32)
{
	Init();
}


void ACVrMirror::Destroyed()
{
	Super::Destroyed();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UVrMirrorSubsystem* MirrorSubsystem = GameInstance->GetSubsystem<UVrMirrorSubsystem>())
		{
			MirrorSubsystem->OnMirrorDestroyed(this);
		}
	}
}

void ACVrMirror::BeginPlay()
{
	Super::BeginPlay();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UVrMirrorSubsystem* MirrorSubsystem = GameInstance->GetSubsystem<UVrMirrorSubsystem>())
		{
			MirrorSubsystem->OnMirrorCreated(this);
		}
	}

	FindActiveCamera();
	SetupCaptureTriggers();

	static const auto CvarMultiView = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.MobileMultiView"));
	bIsMobileMultiView = CvarMultiView->GetInt() == 1;

	if (bCullingEnabled)
	{
		SceneCaptureLeftEye->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		SceneCaptureRightEye->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	}

	if (const UWorld* World = GetWorld())
	{
		// Viewport size is not ready on BeginPlay, so run Init shortly after the game starts.
		FTimerHandle TimerHandleInit;
		World->GetTimerManager().SetTimer(TimerHandleInit, this, &ACVrMirror::Init, 0.2f, false);

		// Reinitialize on viewport resize. 
		FViewport::ViewportResizedEvent.AddUObject(this, &ACVrMirror::OnViewportResize);

		if (bEnableDynamicCaptureResolution)
		{
			FTimerHandle TimerHandleDynamicCaptureResolution;
			World->GetTimerManager().SetTimer(TimerHandleDynamicCaptureResolution, this,
			                                  &ACVrMirror::CheckDynamicResolution,
			                                  DynamicCaptureCheckInterval, true, 1.f);
		}
	}
}

void ACVrMirror::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

void ACVrMirror::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void ACVrMirror::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	CaptureScene();
	FString Bla = GetActorNameOrLabel() + ", " + FString::FromInt(NumActiveCaptureTriggers);
	// GEngine->AddOnScreenDebugMessage(FMath::Rand(), -1, FColor::Purple, Bla);
}

void ACVrMirror::Init()
{
	Resolution = GetHmdResolution();
	IpdHalfDistanceCm = GetIpdCm() / 2;
	HorizontalFov = FMath::RoundToInt(GetHmdFov().X);

	const int32 RenderTargetWidth = Resolution.X * CaptureQuality * (bIsMobileMultiView ? 1 : 0.5);
	const int32 RenderTargetHeight = Resolution.Y * CaptureQuality;

	RenderTargetLeftEye = UKismetRenderingLibrary::CreateRenderTarget2D(this, RenderTargetWidth, RenderTargetHeight);
	RenderTargetRightEye = UKismetRenderingLibrary::CreateRenderTarget2D(this, RenderTargetWidth, RenderTargetHeight);

	GEngine->AddOnScreenDebugMessage(15, 5, FColor::Red, Resolution.ToString());
	GEngine->AddOnScreenDebugMessage(16, 5, FColor::Red, FString::FromInt(RenderTargetWidth * RenderTargetHeight));
	GEngine->AddOnScreenDebugMessage(17, 5, FColor::Red, bIsMobileMultiView ? "Multi view on" : "Multi view off");

	SceneCaptureLeftEye->TextureTarget = RenderTargetLeftEye;
	SceneCaptureLeftEye->FOVAngle = HorizontalFov;

	SceneCaptureRightEye->TextureTarget = RenderTargetRightEye;
	SceneCaptureRightEye->FOVAngle = HorizontalFov;

	if (MirrorMaterial)
	{
		MaterialInstanceDynamic = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, MirrorMaterial);
		MaterialInstanceDynamic->SetTextureParameterValue("LeftEyeRenderTarget", RenderTargetLeftEye);
		MaterialInstanceDynamic->SetTextureParameterValue("RightEyeRenderTarget", RenderTargetRightEye);
		MaterialInstanceDynamic->SetScalarParameterValue("ResolutionX", Resolution.X);
		MaterialInstanceDynamic->SetScalarParameterValue("ResolutionY", Resolution.Y);
		MaterialInstanceDynamic->SetScalarParameterValue("Fov", HorizontalFov);
		MaterialInstanceDynamic->SetScalarParameterValue("bIsStereoscopic", bIsStereoscopic);
		MaterialInstanceDynamic->SetScalarParameterValue("bIsMobileMultiView", bIsMobileMultiView);

		MirrorMesh->SetMaterial(0, MaterialInstanceDynamic);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(1, 5, FColor::Red, "MirrorMaterial not set?");
	}
}

void ACVrMirror::FindActiveCamera()
{
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PlayerController = PC;
			if (const APawn* PlayerPawn = PlayerController->GetPawn())
			{
				TArray<UCameraComponent*> Cameras;
				PlayerPawn->GetComponents(Cameras);

				if (UCameraComponent** FoundCamera = Cameras.FindByPredicate([](const UCameraComponent* Camera)
				{
					return Camera->IsActive();
				}))
				{
					ActiveCamera = *FoundCamera;
				}
				else
				{
					GEngine->AddOnScreenDebugMessage(2, 5, FColor::Red, "Could not find active camera.");
				}
			}
		}
	}
}

void ACVrMirror::SetupCaptureTriggers()
{
	if (CaptureTriggers.Num() > 0)
	{
		bIsUsingCaptureTriggers = true;
		for (ATriggerBox* const CaptureTrigger : CaptureTriggers)
		{
			CaptureTrigger->OnActorBeginOverlap.AddDynamic(this, &ACVrMirror::OnCaptureTriggerBeginOverlap);
			CaptureTrigger->OnActorEndOverlap.AddDynamic(this, &ACVrMirror::OnCaptureTriggerEndOverlap);


			// If the player has already been spawned before we bound the functions above to the delegates we won't get notified.
			// This check makes sure we count a capture trigger as active if there is a player already inside it.
			TArray<AActor*> OverlappingActors;
			CaptureTrigger->GetOverlappingActors(OverlappingActors);

			if (PlayerController)
			{
				for (const AActor* OverlappingActor : OverlappingActors)
				{
					if (PlayerController->GetPawn() == OverlappingActor)
					{
						NumActiveCaptureTriggers++;
					}
				}
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(4, 5, FColor::Red,
				                                 "No PlayerController during SetupCaptureTriggers()");
			}
		}
	}
}

void ACVrMirror::CaptureScene()
{
	if (ShouldSkipCapture())
	{
		return;
	}

	MaterialInstanceDynamic->SetVectorParameterValue("XCameraToWorldVector", ActiveCamera->GetForwardVector());
	MaterialInstanceDynamic->SetVectorParameterValue("YCameraToWorldVector", ActiveCamera->GetRightVector());
	MaterialInstanceDynamic->SetVectorParameterValue("ZCameraToWorldVector", ActiveCamera->GetUpVector());

	const FVector MirrorForwardVector = GetActorForwardVector();
	const FVector ClipPlaneBase = GetActorLocation()-MirrorForwardVector;
	const FVector ClipPlaneNormal = MirrorForwardVector;

	const FTransform MirroredCameraTransform = MirrorCamera(ActiveCamera->GetComponentTransform());
	MirrorCulling(MirroredCameraTransform);
	TArray<FTransform> MirroredCameras;

	if (bIsStereoscopic)
	{
		MirroredCameras = CreateEyeOffsets(MirroredCameraTransform);
		SceneCaptureRightEye->ClipPlaneBase = ClipPlaneBase;
		SceneCaptureRightEye->ClipPlaneNormal = ClipPlaneNormal;
		SceneCaptureRightEye->SetWorldTransform(MirroredCameras[1]);
		SceneCaptureRightEye->CaptureScene();
	}

	SceneCaptureLeftEye->ClipPlaneBase = ClipPlaneBase;
	SceneCaptureLeftEye->ClipPlaneNormal = ClipPlaneNormal;
	SceneCaptureLeftEye->SetWorldTransform(bIsStereoscopic ? MirroredCameras[0] : MirroredCameraTransform);
	SceneCaptureLeftEye->CaptureScene();
}

FTransform ACVrMirror::MirrorCamera(const FTransform& CameraTransform) const
{
	const FTransform ActorTransform = GetActorTransform();
	FVector NewCameraLocation = ActorTransform.InverseTransformPosition(CameraTransform.GetLocation());
	NewCameraLocation.X *= -1;
	NewCameraLocation = ActorTransform.TransformPosition(NewCameraLocation);

	const FVector ActorForward = ActorTransform.GetRotation().GetForwardVector();
	const FVector CameraForward = CameraTransform.GetRotation().GetForwardVector();
	const FVector CameraRight = CameraTransform.GetRotation().GetRightVector();

	const FVector MirroredCamForward = FMath::GetReflectionVector(CameraForward, ActorForward);
	const FVector MirroredCamRight = FMath::GetReflectionVector(CameraRight, ActorForward);
	const FRotator NewCameraRotation = UKismetMathLibrary::MakeRotFromXY(MirroredCamForward, MirroredCamRight);
	return FTransform(NewCameraRotation, NewCameraLocation);
}

FVector2D ACVrMirror::GetHmdResolution()
{
	if (GEngine && GEngine->XRSystem)
	{
		if (const IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice())
		{
			const FVector2D HMDResolution = HMD->GetIdealRenderTargetSize();
			return HMDResolution;
		}
	}

	return FVector2D::Zero();
}

void ACVrMirror::CheckDynamicResolution()
{
	if (!bEnableDynamicCaptureResolution || !ActiveCamera)
	{
		return;
	}

	const float DistanceSquared = FVector::DistSquared(GetActorLocation(), ActiveCamera->GetComponentLocation());
	float NewCaptureQuality = UKismetMathLibrary::MapRangeClamped(DistanceSquared,
	                                                              FMath::Square(DynamicCaptureRangeStart),
	                                                              FMath::Square(DynamicCaptureRangeEnd),
	                                                              InitialCaptureQuality,
	                                                              LowestDynamicCaptureQuality);


	NewCaptureQuality = FMath::TruncToFloat(NewCaptureQuality * 10) / 10;
	if (CaptureQuality != NewCaptureQuality)
	{
		CaptureQuality = NewCaptureQuality;

		const int32 RenderTargetWidth = Resolution.X * CaptureQuality * (bIsMobileMultiView ? 1 : 0.5);
		const int32 RenderTargetHeight = Resolution.Y * CaptureQuality;

		RenderTargetLeftEye =
			UKismetRenderingLibrary::CreateRenderTarget2D(this, RenderTargetWidth, RenderTargetHeight);
		RenderTargetLeftEye->AddressX = TA_Clamp;
		RenderTargetLeftEye->AddressY = TA_Clamp;
		RenderTargetRightEye = UKismetRenderingLibrary::CreateRenderTarget2D(
			this, RenderTargetWidth, RenderTargetHeight);
		RenderTargetRightEye->AddressX = TA_Clamp;
		RenderTargetRightEye->AddressY = TA_Clamp;

		SceneCaptureLeftEye->TextureTarget = RenderTargetLeftEye;
		SceneCaptureRightEye->TextureTarget = RenderTargetRightEye;
		if (MirrorMaterial)
		{
			MaterialInstanceDynamic->SetTextureParameterValue("LeftEyeRenderTarget", RenderTargetLeftEye);
			MaterialInstanceDynamic->SetTextureParameterValue("RightEyeRenderTarget", RenderTargetRightEye);
		}

		if (GEngine)
		{
			GEngine->ForceGarbageCollection();
		}
	}
}

bool ACVrMirror::ShouldSkipCapture() const
{
	if ((bIsUsingCaptureTriggers && NumActiveCaptureTriggers == 0) || !MirrorMesh->WasRecentlyRendered() || !
		ActiveCamera
		|| !
		MaterialInstanceDynamic)
	{
		return true;
	}

	// Stop capturing if we are beyond specified max distance.
	const float DistanceSquared = FVector::DistSquared(ActiveCamera->GetComponentLocation(), GetActorLocation());
	if (DistanceSquared >= CaptureMaxDistance * CaptureMaxDistance)
	{
		return true;
	}

	const FVector MirrorToCameraLocal = GetActorTransform().InverseTransformPositionNoScale(
		ActiveCamera->GetComponentLocation());

	// Stop capturing if we are far enough behind the mirror that neither eye can see it even when perpendicular.
	if (MirrorToCameraLocal.X <= -IpdHalfDistanceCm)
	{
		return true;
	}

	return false;
}

void ACVrMirror::MirrorCulling(const FTransform& MirroredCameraTransform)
{
	if (!bCullingEnabled || !MirrorCullingTraceChannel)
	{
		return;
	}

	FVector MirrorLocation = MirrorMesh->GetComponentLocation();
	FVector MirroredCameraLocation = MirroredCameraTransform.GetLocation();
	FVector Min;
	FVector Max;
	MirrorMesh->GetLocalBounds(Min, Max);

	float MirrorHalfWidth = (Max.Y * MirrorMesh->GetComponentScale().Y * MirrorCullingBufferMultiplier) +
		IpdHalfDistanceCm;
	float MirrorHalfHeight = (Max.Z * MirrorMesh->GetComponentScale().Z * MirrorCullingBufferMultiplier) +
		IpdHalfDistanceCm;

	FVector MirrorTop = MirrorLocation + GetActorUpVector() * MirrorHalfHeight;
	FVector MirrorBottom = MirrorLocation + GetActorUpVector() * -1 * MirrorHalfHeight;

	FVector MirrorTopLeft = MirrorTop - GetActorRightVector() * MirrorHalfWidth;
	FVector MirrorTopRight = MirrorTop + GetActorRightVector() * MirrorHalfWidth;
	FVector MirrorBottomLeft = MirrorBottom - GetActorRightVector() * MirrorHalfWidth;
	FVector MirrorBottomRight = MirrorBottom + GetActorRightVector() * MirrorHalfWidth;

	FVector CaptureToTopLeft = (MirrorTopLeft - MirroredCameraLocation).GetSafeNormal();
	FVector CaptureToTopRight = (MirrorTopRight - MirroredCameraLocation).GetSafeNormal();
	FVector CaptureToBottomLeft = (MirrorBottomLeft - MirroredCameraLocation).GetSafeNormal();
	FVector CaptureToBottomRight = (MirrorBottomRight - MirroredCameraLocation).GetSafeNormal();
	FVector CaptureToMirror = (MirrorLocation - MirroredCameraLocation).GetSafeNormal();

	//Close plane
	FVector ClosePlaneNormal = MirrorMesh->GetForwardVector();
	float ClosePlaneW = FVector::DotProduct(ClosePlaneNormal, MirrorTopLeft);
	FPlane FrustumClosePlane = FPlane(ClosePlaneNormal.X, ClosePlaneNormal.Y, ClosePlaneNormal.Z, ClosePlaneW);

	//Far plane
	float FrustumDistance = MirrorCullingTraceDistance;
	FVector FarPlanePoint = MirrorLocation + CaptureToMirror * FrustumDistance;
	FVector FarPlaneNormal = CaptureToMirror * -1;
	float FarPlaneW = FVector::DotProduct(FarPlaneNormal, FarPlanePoint);
	FPlane FrustumFarPlane = FPlane(FarPlaneNormal.X, FarPlaneNormal.Y, FarPlaneNormal.Z, FarPlaneW);

	// Top Plane
	FVector TopPlaneNormal = FVector::CrossProduct(CaptureToTopRight, CaptureToTopLeft).GetSafeNormal();
	float TopPlaneW = FVector::DotProduct(TopPlaneNormal, MirrorTopLeft);
	FPlane FrustumTopPlane = FPlane(TopPlaneNormal.X, TopPlaneNormal.Y, TopPlaneNormal.Z, TopPlaneW);

	// // Bottom Plane
	FVector BottomPlaneNormal = FVector::CrossProduct(CaptureToBottomLeft, CaptureToBottomRight).GetSafeNormal();
	float BottomPlaneW = FVector::DotProduct(BottomPlaneNormal, MirrorBottomLeft);
	FPlane FrustumBottomPlane = FPlane(BottomPlaneNormal.X, BottomPlaneNormal.Y, BottomPlaneNormal.Z, BottomPlaneW);

	// Left Plane
	FVector LeftPlaneNormal = FVector::CrossProduct(CaptureToTopLeft, CaptureToBottomLeft).GetSafeNormal();
	float LeftPlaneW = FVector::DotProduct(LeftPlaneNormal, MirrorTopLeft);
	FPlane FrustumLeftPlane = FPlane(LeftPlaneNormal.X, LeftPlaneNormal.Y, LeftPlaneNormal.Z, LeftPlaneW);

	// Right Plane
	FVector RightPlaneNormal = FVector::CrossProduct(CaptureToBottomRight, CaptureToTopRight).GetSafeNormal();
	float RightPlaneW = FVector::DotProduct(RightPlaneNormal, MirrorTopRight);
	FPlane FrustumRightPlane = FPlane(RightPlaneNormal.X, RightPlaneNormal.Y, RightPlaneNormal.Z, RightPlaneW);

	TArray<FPlane*> FrustumPlanes{
		&FrustumClosePlane, &FrustumFarPlane, &FrustumTopPlane, &FrustumBottomPlane, &FrustumLeftPlane,
		&FrustumRightPlane
	};

	float CaptureToMirrorDist = FVector::Dist(MirroredCameraLocation, MirrorLocation);
	float WidthAtFarPlane = UKismetMathLibrary::DegTan(HorizontalFov / 2) * (CaptureToMirrorDist +
		FrustumDistance) * 2;

	if (bShowCullingPlanes)
	{
		for (const FPlane* Plane : FrustumPlanes)
		{
			DrawDebugSolidPlane(GetWorld(), *Plane, MirrorLocation, 2000,
			                    FColor::Red.WithAlpha(60));
		}
	}

	TArray<FHitResult> HitResults;
	TArray<AActor*> ActorsToIgnore;
	FQuat MirroredCameraRotation = MirroredCameraTransform.GetRotation();
	FVector End = MirrorLocation + MirroredCameraRotation.GetForwardVector() * FrustumDistance;

	LastTraceCameraPosition = ActiveCamera->GetComponentLocation();
	UKismetSystemLibrary::BoxTraceMulti(this, MirrorLocation,
	                                    End, FVector(100, WidthAtFarPlane / 2, WidthAtFarPlane / 2),
	                                    MirroredCameraRotation.Rotator(),
	                                    UEngineTypes::ConvertToTraceType(MirrorCullingTraceChannel), false,
	                                    ActorsToIgnore,
	                                    EDrawDebugTrace::None, HitResults, true);

	SceneCaptureLeftEye->ShowOnlyActors.Empty();
	SceneCaptureRightEye->ShowOnlyActors.Empty();
	TSet<AActor*> HandledActors;
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* Actor = HitResult.GetActor();
		if (HandledActors.Contains(Actor))
		{
			continue;
		}

		const auto& Sphere = HitResult.GetComponent()->Bounds.GetSphere();
		bool ShouldRender = true;

		for (const FPlane* Plane : FrustumPlanes)
		{
			const float Distance = Plane->PlaneDot(Sphere.Center);

			// If a component of an actor is outside of at least one plane by more than its bound's sphere radius, then we assume the actor is not visible in the reflection for now.
			if (Distance < -Sphere.W)
			{
				ShouldRender = false;
				break;
			}
		}

		// If a component is visible, mark the owning actor as visible and add it to HandledActors to skip further checks for it.
		if (ShouldRender)
		{
			SceneCaptureLeftEye->ShowOnlyActors.Add(Actor);
			SceneCaptureRightEye->ShowOnlyActors.Add(Actor);
			HandledActors.Add(Actor);
		}
	}

	for (auto Actor : DontCullActors)
	{
		SceneCaptureLeftEye->ShowOnlyActors.Add(Actor);
		SceneCaptureRightEye->ShowOnlyActors.Add(Actor);
	}
}

FVector2D ACVrMirror::GetHmdFov()
{
	if (GEngine && GEngine->XRSystem)
	{
		if (const IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice())
		{
			float FovHorizontal;
			float FovVertical;
			HMD->GetFieldOfView(FovHorizontal, FovVertical);

			return FVector2D(FovHorizontal, FovVertical);
		}
	}

	return FVector2D(0, 0);
}

float ACVrMirror::GetIpdCm() const
{
	if (CustomIpdCm > 0)
	{
		return CustomIpdCm;
	}

	if (GEngine && GEngine->XRSystem)
	{
		if (const IHeadMountedDisplay* Hmd = GEngine->XRSystem->GetHMDDevice())
		{
			// GetInterpupillaryDistance returns Ipd in meters.
			return Hmd->GetInterpupillaryDistance() * 100;
		}
	}

	GEngine->AddOnScreenDebugMessage(3, 5, FColor::Red,
	                                 "Could not get correct Ipd distance, defaulting to 6.4. Set CustomIpdDistance instead?");
	return 6.4;
}

TArray<FTransform> ACVrMirror::CreateEyeOffsets(const FTransform& CameraTransform) const
{
	const FVector CameraLocation = CameraTransform.GetLocation();
	const FQuat CameraRotation = CameraTransform.GetRotation();
	const FVector RightVector = CameraRotation.GetRightVector();
	const FVector LeftVector = CameraRotation.GetRightVector() * -1;

	const FVector EyeLocationLeft = CameraLocation + LeftVector * IpdHalfDistanceCm;
	const FVector EyeLocationRight = CameraLocation + RightVector * IpdHalfDistanceCm;

	const FTransform LeftEyeTransform = FTransform(CameraTransform.GetRotation(), EyeLocationLeft);
	const FTransform RightEyeTransform = FTransform(CameraTransform.GetRotation(), EyeLocationRight);

	TArray EyeTransforms{LeftEyeTransform, RightEyeTransform};

	return EyeTransforms;
}

void ACVrMirror::OnCaptureTriggerBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (ActiveCamera && PlayerController->GetPawn() == OtherActor)
	{
		NumActiveCaptureTriggers++;
	}
}

void ACVrMirror::OnCaptureTriggerEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (ActiveCamera && PlayerController->GetPawn() == OtherActor)
	{
		NumActiveCaptureTriggers--;
	}
}

// Editor only functions
#if WITH_EDITOR

void ACVrMirror::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	LowestDynamicCaptureQuality = FMath::Clamp(LowestDynamicCaptureQuality, 0.1, CaptureQuality);
}

#endif
