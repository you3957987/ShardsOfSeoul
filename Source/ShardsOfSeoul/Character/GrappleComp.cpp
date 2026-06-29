#include "GrappleComp.h"
#include "ShardsOfSeoulCharacter.h"
#include "Blueprint/UserWidget.h"
#include "UI/InteractionHUDWidget.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "Components/PrimitiveComponent.h"

UGrappleComp::UGrappleComp()
{
	// 틱 컴포넌트 활성화
	PrimaryComponentTick.bCanEverTick = true;
}

void UGrappleComp::BeginPlay()
{
	Super::BeginPlay();
	Owner = Cast<ACharacter>(GetOwner());
	
	if (Owner)
	{
		APlayerController* PC = Cast<APlayerController>(Owner->GetController());
		if (PC && PC->PlayerCameraManager)
		{
			CameraManager = PC->PlayerCameraManager;
		}
	}
}

void UGrappleComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 그래플 비행(런치 상태) 중일 때 처리
	if (bIsGrappling && Owner)
	{
		GrappleActiveTime += DeltaTime;

		// 1. 타겟 지점 도달 여부 체크 (200cm 이내)
		float Distance = FVector::Dist(Owner->GetActorLocation(), GrappleTargetLocation);

		// 2. 착지 여부 체크
		bool bLanded = false;
		if (Owner->GetCharacterMovement())
		{
			bLanded = Owner->GetCharacterMovement()->IsMovingOnGround();
		}

		// 해제 조건 만족 시 그래플 비행 종료
		if (Distance <= 200.f || bLanded || GrappleActiveTime >= 1.5f)
		{
			bIsGrappling = false;
		}
	}

	// 최적의 자동 조준 대상을 찾아 캐싱
	SetCurrentTargetActor(FindBestGrappleTarget());

	// 그래플 조준 타겟이 있는 동안 실시간 UI 화면 좌표 매핑 틱 작동
	if (CurrentTargetActor && Owner)
	{
		AShardsOfSeoulCharacter* Character = Cast<AShardsOfSeoulCharacter>(Owner);
		if (Character && Character->InteractionHUDInstance)
		{
			APlayerController* PC = Cast<APlayerController>(Character->GetController());
			if (PC)
			{
				FVector TargetWorldLoc = CurrentTargetActor->GetActorLocation();
				FVector2D ScreenPosition;

				// 3D 좌표를 2D 화면 픽셀 좌표로 변환
				if (PC->ProjectWorldLocationToScreen(TargetWorldLoc, ScreenPosition))
				{
					// 조준선 가림 방지 오프셋 보정
					ScreenPosition.X += 30.f;
					ScreenPosition.Y -= 15.f;

					FText TargetDesc;
					FProperty* DescProp = CurrentTargetActor->GetClass()->FindPropertyByName(FName("DescriptionText"));
					if (DescProp)
					{
						DescProp->GetValue_InContainer(CurrentTargetActor, &TargetDesc);
					}
					else
					{
						TargetDesc = FText::FromString(TEXT("E"));
					}

					Character->InteractionHUDInstance->UpdateTargetUI(ScreenPosition, TargetDesc);
				}
			}
		}
	}
}

void UGrappleComp::Grapple()
{
	// 이미 그래플 비행 중이거나 몽타주 대기 준비 상태인 경우 중복 입력 방지
	if (bIsGrappling || (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(GrappleDelayTimerHandle))) return;

	if (!Owner) return;

	// 비행 시작 전 혹시 타겟이 비어 있다면 즉시 재탐색
	if (!CurrentTargetActor)
	{
		SetCurrentTargetActor(FindBestGrappleTarget());
	}

	// 자동 조준된 타겟이 존재하면 애니메이션 재생 및 타이머 설정
	if (CurrentTargetActor)
	{
		// 1. 애니메이션 몽타주 재생
		if (GrappleMontage)
		{
			if (UAnimInstance* AnimInstance = Owner->GetMesh()->GetAnimInstance())
			{
				AnimInstance->Montage_Play(GrappleMontage);
			}
		}

		bShouldHoldInAir = false;

		// 2. 비행할 목표 위치 설정
		GrappleTargetLocation = CurrentTargetActor->GetActorLocation();

		// 3. 지연 이동 시작 (FTimerHandle 사용)
		GetWorld()->GetTimerManager().SetTimer(
			GrappleDelayTimerHandle,
			this,
			&UGrappleComp::StartGrappleMove,
			GrappleDelay,
			false
		);
	}
}

void UGrappleComp::StartGrappleMove()
{
	if (!Owner) return;

	bIsGrappling = true;
	GrappleActiveTime = 0.f;
	bShouldHoldInAir = false;

	FVector TargetLoc = GrappleTargetLocation;
	if (CurrentTargetActor)
	{
		TargetLoc = CurrentTargetActor->GetActorLocation();
	}

	FVector CurrentLocation = Owner->GetActorLocation();
	float Distance = FVector::Dist(CurrentLocation, TargetLoc);
	if (Distance < 100.f)
	{
		Distance = 100.f;
	}

	FVector Direction = (TargetLoc - CurrentLocation).GetSafeNormal();

	if (Owner->GetCharacterMovement())
	{
		// 발사 직전 기존 관성을 0으로 소거 (Freeze & Go 연출 및 정확도 확보)
		Owner->GetCharacterMovement()->StopMovementImmediately();
		Owner->GetCharacterMovement()->Velocity = FVector::ZeroVector;

		// 낙하(Falling) 상태로 전환하여 물리 법칙을 적용받게 함
		Owner->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

		// 최초 와이어 이동 거리가 멀수록 슬링샷 효과를 더 강력하게 적용 (10m 기준 0.6배 ~ 1.8배)
		float DistanceFactor = FMath::Clamp(Distance / 1500.f, 0.6f, 1.8f);

		// Z축 보정 없이 대상을 향한 방향으로 속도를 즉시 대입
		FVector LaunchVelocity = Direction * LaunchSpeed * DistanceFactor;

		Owner->GetCharacterMovement()->Velocity = LaunchVelocity;
	}

	// 런치 완료 후 대상 포인터 정리
	SetCurrentTargetActor(nullptr);
}

bool UGrappleComp::IsGrapplingOrPreparing() const
{
	return bIsGrappling || (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(GrappleDelayTimerHandle));
}

void UGrappleComp::AddGrappleTarget(AActor* TargetActor)
{
	if (TargetActor && !GrappleTargets.Contains(TargetActor))
	{
		GrappleTargets.Add(TargetActor);
	}
}

void UGrappleComp::RemoveGrappleTarget(AActor* TargetActor)
{
	if (TargetActor)
	{
		GrappleTargets.Remove(TargetActor);
		if (CurrentTargetActor == TargetActor)
		{
			SetCurrentTargetActor(nullptr);
		}
	}
}

AActor* UGrappleComp::FindBestGrappleTarget() const
{
	if (!Owner || !CameraManager || GrappleTargets.Num() == 0) return nullptr;

	AActor* BestTarget = nullptr;
	float ClosestDistance = MaxGrappleDistance + 100.f; // 아주 큰 값으로 초기화
	
	// Degree 시야각 제한을 Cosine 값으로 미리 계산 (기본적으로 화면 영역 안에 있는지 판별)
	float MinAngleCos = FMath::Cos(FMath::DegreesToRadians(AutoTargetMaxAngle));

	FVector CameraLoc = CameraManager->GetCameraLocation();
	FVector CameraForward = CameraManager->GetCameraRotation().Vector();

	for (AActor* Actor : GrappleTargets)
	{
		// IsValid 검증을 거쳐 가비지 컬렉션 대상이 되었거나 파괴된 액터 방지
		if (!IsValid(Actor) || Actor == Owner) continue;

		FVector ActorLoc = Actor->GetActorLocation();
		float Distance = FVector::Dist(ActorLoc, Owner->GetActorLocation());

		// 1. 최대 사거리 이내인지 확인
		if (Distance <= MaxGrappleDistance)
		{
			FVector ToTarget = (ActorLoc - CameraLoc).GetSafeNormal();
			float Dot = FVector::DotProduct(CameraForward, ToTarget);

			// 2. 화면(시야각) 내에 존재하는지 확인
			if (Dot >= MinAngleCos)
			{
				// 3. 시야 내 후보 중 "가장 가까운 거리"의 타겟 선정
				if (Distance < ClosestDistance)
				{
					// 4. 캐릭터와 타겟 사이에 시야 장애물이 없는지 검증 (Line of Sight)
					FHitResult SightHit;
					FVector StartLoc = Owner->GetActorLocation() + FVector(0.f, 0.f, Owner->BaseEyeHeight);
					
					TArray<AActor*> ActorsToIgnore;
					ActorsToIgnore.Add(Owner);
					ActorsToIgnore.Add(Actor); // 타겟 자체는 충돌 검사에서 무시

					// 디버그 드로잉이 내장된 Kismet 라인 트레이스로 교체하여 선이 보이도록 설정
					bool bHit = UKismetSystemLibrary::LineTraceSingle(
						GetWorld(),
						StartLoc,
						ActorLoc,
						UEngineTypes::ConvertToTraceType(ECC_Visibility),
						false,
						ActorsToIgnore,
						EDrawDebugTrace::ForOneFrame, // 매 프레임 틱에서 돌기 때문에 ForOneFrame이 가장 부드럽고 깔끔합니다
						SightHit,
						true,
						FLinearColor::Red,
						FLinearColor::Green,
						0.f
					);

					// 무언가에 부딪히지 않았다면(!bHit) 시야가 장애물에 막히지 않은 것
					if (!bHit)
					{
						ClosestDistance = Distance;
						BestTarget = Actor;
					}
				}
			}
		}
	}

	return BestTarget;
}

void UGrappleComp::SetCurrentTargetActor(AActor* NewTarget)
{
	if (CurrentTargetActor == NewTarget) return;

	if (CurrentTargetActor)
	{
		SetActorCustomDepth(CurrentTargetActor, false);

		AShardsOfSeoulCharacter* Character = Cast<AShardsOfSeoulCharacter>(Owner);
		if (Character && Character->InteractionHUDInstance && Character->CurrentInteractionHUDOwner == CurrentTargetActor)
		{
			Character->InteractionHUDInstance->HideTargetUI();
			Character->CurrentInteractionHUDOwner = nullptr;
		}
	}

	CurrentTargetActor = NewTarget;

	if (CurrentTargetActor)
	{
		// 그래플 목표 타겟은 윤곽선 아웃라인(스텐실 1번 효과)을 적용합니다.
		SetActorCustomDepth(CurrentTargetActor, true, 1);

		AShardsOfSeoulCharacter* Character = Cast<AShardsOfSeoulCharacter>(Owner);
		if (Character && Character->InteractionHUDInstance)
		{
			FText TargetDesc;
			FProperty* DescProp = CurrentTargetActor->GetClass()->FindPropertyByName(FName("DescriptionText"));
			if (DescProp)
			{
				DescProp->GetValue_InContainer(CurrentTargetActor, &TargetDesc);
			}
			else
			{
				TargetDesc = FText::FromString(TEXT("E"));
			}

			Character->CurrentInteractionHUDOwner = CurrentTargetActor;
			Character->InteractionHUDInstance->UpdateTargetUI(FVector2D::ZeroVector, TargetDesc);
		}
	}
}

void UGrappleComp::SetActorCustomDepth(AActor* Target, bool bEnable, int32 StencilValue)
{
	if (!Target) return;

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Target->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
	{
		if (PrimComp)
		{
			PrimComp->SetRenderCustomDepth(bEnable);
			if (bEnable)
			{
				PrimComp->SetCustomDepthStencilValue(StencilValue);
			}
		}
	}
}
