#include "CMirror.h"
#include "MirrorSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TriggerBox.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"

ACMirror::ACMirror()
{
	PrimaryActorTick.bCanEverTick = true;
	SetTickGroup(TG_PostUpdateWork);
	SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	SetRootComponent(SceneRoot);

	MirrorMesh = CreateDefaultSubobject<UStaticMeshComponent>("MirrorMesh");
	MirrorMesh->SetupAttachment(GetRootComponent());

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>("SceneCaptureLeftEye");
	SceneCapture->SetupAttachment(GetRootComponent());
	SceneCapture->bEnableClipPlane = true;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;

	InitialCaptureQuality = CaptureQuality;
}

void ACMirror::OnViewportResize(FViewport* Viewport, uint32)
{
	Init();
}

void ACMirror::Destroyed()
{
	Super::Destroyed();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMirrorSubsystem* MirrorSubsystem = GameInstance->GetSubsystem<UMirrorSubsystem>())
		{
			MirrorSubsystem->OnMirrorDestroyed(this);
		}
	}
}

void ACMirror::BeginPlay()
{
	Super::BeginPlay();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMirrorSubsystem* MirrorSubsystem = GameInstance->GetSubsystem<UMirrorSubsystem>())
		{
			MirrorSubsystem->OnMirrorCreated(this);
		}
	}

	FindActiveCamera();
	SetupCaptureTriggers();

	if (bCullingEnabled)
	{
		SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	}
	else
	{
		SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	}

	if (const UWorld* World = GetWorld())
	{
		// Viewport size is not ready on BeginPlay, so run Init shortly after the game starts.
		FTimerHandle TimerHandleInit;
		World->GetTimerManager().SetTimer(TimerHandleInit, this, &ACMirror::Init, 0.2f, false);

		// Reinitialize on viewport resize. 
		FViewport::ViewportResizedEvent.AddUObject(this, &ACMirror::OnViewportResize);

		if (bEnableDynamicCaptureResolution)
		{
			FTimerHandle TimerHandleDynamicCaptureResolution;
			World->GetTimerManager().SetTimer(TimerHandleDynamicCaptureResolution, this,
			                                  &ACMirror::CheckDynamicResolution,
			                                  DynamicCaptureCheckInterval, true, 1.f);
		}
	}
}

void ACMirror::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	CaptureScene();
	FString Bla = GetActorNameOrLabel() + ", " + FString::FromInt(NumActiveCaptureTriggers);
	GEngine->AddOnScreenDebugMessage(FMath::Rand(), -1, FColor::Purple, Bla);
}

void ACMirror::Init()
{
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Resolution);
		if (ActiveCamera)
		{
			HorizontalFov = ActiveCamera->FieldOfView;
			if (!ActiveCamera->bConstrainAspectRatio)
			{
				ActiveCamera->SetAspectRatio(Resolution.X / Resolution.Y);
			}
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(1, 5, FColor::Red, "Active camera not valid during init.");
		}
	}

	const FVector2D RenderTargetResolution = CalcRenderTargetResolution();
	RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, RenderTargetResolution.X,
	                                                             RenderTargetResolution.Y);
	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->FOVAngle = HorizontalFov;

	if (MirrorMaterial)
	{
		MaterialInstanceDynamic = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, MirrorMaterial);
		MaterialInstanceDynamic->SetTextureParameterValue("RenderTarget", RenderTarget);
		MirrorMesh->SetMaterial(0, MaterialInstanceDynamic);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(2, 5, FColor::Red, "MirrorMaterial not set?");
	}
}

void ACMirror::FindActiveCamera()
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

void ACMirror::SetupCaptureTriggers()
{
	if (CaptureTriggers.Num() > 0)
	{
		bIsUsingCaptureTriggers = true;
		for (ATriggerBox* const CaptureTrigger : CaptureTriggers)
		{
			CaptureTrigger->OnActorBeginOverlap.AddDynamic(this, &ACMirror::OnCaptureTriggerBeginOverlap);
			CaptureTrigger->OnActorEndOverlap.AddDynamic(this, &ACMirror::OnCaptureTriggerEndOverlap);


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

void ACMirror::CaptureScene()
{
	if (ShouldSkipCapture())
	{
		return;
	}

	const FTransform MirroredCameraTransform = MirrorCamera(ActiveCamera->GetComponentTransform());
	FVector MirroredCameraLocation = MirroredCameraTransform.GetLocation();
	MirrorCulling(MirroredCameraLocation);

	SceneCapture->ClipPlaneBase = GetActorLocation();
	SceneCapture->ClipPlaneNormal = GetActorForwardVector();
	SceneCapture->SetWorldTransform(MirroredCameraTransform);
	SceneCapture->CaptureScene();
}

FTransform ACMirror::MirrorCamera(const FTransform& CameraTransform) const
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

FVector2D ACMirror::CalcRenderTargetResolution() const
{
	float RenderTargetWidth;
	float RenderTargetHeight;

	if (ActiveCamera && ActiveCamera->bConstrainAspectRatio)
	{
		if (ActiveCamera->AspectRatio > 1)
		{
			RenderTargetWidth = Resolution.X * CaptureQuality;
			RenderTargetHeight = RenderTargetWidth / ActiveCamera->AspectRatio;
		}
		else
		{
			RenderTargetHeight = Resolution.Y * CaptureQuality;
			RenderTargetWidth = RenderTargetHeight * ActiveCamera->AspectRatio;
		}
	}
	else
	{
		RenderTargetWidth = Resolution.X * CaptureQuality;
		RenderTargetHeight = Resolution.Y * CaptureQuality;
	}

	return FVector2D(RenderTargetWidth, RenderTargetHeight);
}

void ACMirror::CheckDynamicResolution()
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
		const FVector2D RenderTargetResolution = CalcRenderTargetResolution();
		RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, RenderTargetResolution.X,
		                                                             RenderTargetResolution.Y);
		SceneCapture->TextureTarget = RenderTarget;
		if (MirrorMaterial)
		{
			MaterialInstanceDynamic->SetTextureParameterValue("RenderTarget", RenderTarget);
		}

		if (GEngine)
		{
			GEngine->ForceGarbageCollection();
		}
	}
}

bool ACMirror::ShouldSkipCapture() const
{
	if ((bIsUsingCaptureTriggers && NumActiveCaptureTriggers == 0) || !MirrorMesh->WasRecentlyRendered() || !
		ActiveCamera || !MaterialInstanceDynamic)
	{
		return true;
	}

	// Stop capturing if we are beyond specified max distance.
	const float DistanceSquared = FVector::DistSquared(ActiveCamera->GetComponentLocation(), GetActorLocation());
	if (DistanceSquared >= CaptureMaxDistance * CaptureMaxDistance)
	{
		return true;
	}

	// Stop capturing if the camera is behind the mirror.
	const FVector MirrorToCameraLocal = GetActorTransform().InverseTransformPositionNoScale(
		ActiveCamera->GetComponentLocation());
	if (MirrorToCameraLocal.X <= 0)
	{
		return true;
	}

	return false;
}

void ACMirror::MirrorCulling(FVector& MirroredCameraLocation)
{
	if (!bCullingEnabled)
	{
		return;
	}

	FVector MirrorLocation = MirrorMesh->GetComponentLocation();
	FVector Min;
	FVector Max;
	MirrorMesh->GetLocalBounds(Min, Max);

	float MirrorHalfWidth = Max.Y * MirrorMesh->GetComponentScale().Y * MirrorCullingBufferMultiplier;
	float MirrorHalfHeight = Max.Z * MirrorMesh->GetComponentScale().Z * MirrorCullingBufferMultiplier;

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
	FVector End = MirrorLocation + SceneCapture->GetForwardVector() * FrustumDistance;

	LastTraceCameraPosition = ActiveCamera->GetComponentLocation();
	UKismetSystemLibrary::BoxTraceMulti(this, MirrorLocation,
	                                    End, FVector(100, WidthAtFarPlane / 2, WidthAtFarPlane / 2),
	                                    SceneCapture->GetComponentRotation(),
	                                    UEngineTypes::ConvertToTraceType(MirrorCullingTraceChannel), false,
	                                    ActorsToIgnore,
	                                    EDrawDebugTrace::None, HitResults, true);

	SceneCapture->ShowOnlyActors.Empty();
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
			SceneCapture->ShowOnlyActors.Add(Actor);
			HandledActors.Add(Actor);
		}
	}

	for (auto Actor : DontCullActors)
	{
		SceneCapture->ShowOnlyActors.Add(Actor);
	}
}

void ACMirror::OnCaptureTriggerBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (ActiveCamera && PlayerController->GetPawn() == OtherActor)
	{
		NumActiveCaptureTriggers++;
	}
}

void ACMirror::OnCaptureTriggerEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (ActiveCamera && PlayerController->GetPawn() == OtherActor)
	{
		NumActiveCaptureTriggers--;
	}
}

// Editor only functions
#if WITH_EDITOR
void ACMirror::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	LowestDynamicCaptureQuality = FMath::Clamp(LowestDynamicCaptureQuality, 0.1, CaptureQuality);
	if (!bCullingEnabled)
	{
		bShowCullingPlanes = false;
	}
}
#endif
