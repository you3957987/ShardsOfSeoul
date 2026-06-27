#include "GrappleComp.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"

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

	// 1. 비행 중이 아닐 때는 실시간으로 최적의 자동 조준 대상을 찾아 캐싱
	if (!bIsGrappling)
	{
		CurrentTargetActor = FindBestGrappleTarget();

		// 지상에서 시전하여 몽타주 재생 대기 중(준비 상태)일 때만 캐릭터가 아래로 떨어지거나 움직이지 않도록 속도를 Zero로 고정
		if (bShouldHoldInAir && GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(GrappleDelayTimerHandle) && Owner && Owner->GetCharacterMovement())
		{
			Owner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}
	// 2. 비행 중일 때는 타겟 방향으로 부드럽게 감속 비행
	else if (bIsGrappling && Owner)
	{
		FVector CurrentLocation = Owner->GetActorLocation();
		
		// 타겟 몬스터가 움직이더라도 실시간 위치로 타겟팅 갱신
		FVector TargetLocation = GrappleTargetLocation;
		if (CurrentTargetActor)
		{
			TargetLocation = CurrentTargetActor->GetActorLocation();
		}

		float Distance = FVector::Dist(TargetLocation, CurrentLocation);
		FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();

		// 목표 지점 근처(80cm)에 도달했는지 확인
		if (Distance <= 80.f)
		{
			bIsGrappling = false;
			bShouldHoldInAir = false;
			CurrentTargetActor = nullptr;
			if (Owner->GetCharacterMovement())
			{
				// 날아가던 방향의 기존 속도를 일부 보존하고, 위로 살짝 뜨는 포물선 궤적을 위해 상승력(Z축 +400.f)을 더해 줍니다.
				FVector ParabolicVelocity = Direction * LaunchSpeed * 0.8f;
				ParabolicVelocity.Z = 400.f;

				Owner->GetCharacterMovement()->Velocity = ParabolicVelocity;
				
				// 걷기 대신 공중 낙하(Falling) 상태로 전환하여 자연스러운 포물선 비행 유도
				Owner->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
		else
		{
			if (Owner->GetCharacterMovement())
			{
				// 목표 지점과 가까워질수록 속도가 자연스럽게 감소하는 감속 보정 공식
				// 500cm(5m) 이내로 들어오면 거리에 비례하여 속도를 최대 20%까지 줄여 스르륵 착지합니다.
				float SpeedScale = FMath::Clamp(Distance / 500.f, 0.2f, 1.0f);
				float InterpolatedSpeed = LaunchSpeed * SpeedScale;

				Owner->GetCharacterMovement()->Velocity = Direction * InterpolatedSpeed;
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
		CurrentTargetActor = FindBestGrappleTarget();
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

		// 캐릭터가 공중(낙하) 상태인지 확인
		bool bIsFalling = false;
		if (Owner->GetCharacterMovement())
		{
			bIsFalling = Owner->GetCharacterMovement()->IsFalling();
		}

		bShouldHoldInAir = !bIsFalling;

		// 지상에서 시전한 경우에만 즉시 이전 이동 관성을 멈추고 공중에 고정시킴
		if (Owner->GetCharacterMovement() && bShouldHoldInAir)
		{
			Owner->GetCharacterMovement()->StopMovementImmediately();
			// 준비 기간 동안 공중에 멈춰 있도록 우선 Flying 상태로 만들고 속도를 0으로 유지
			Owner->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			Owner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}

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
	bShouldHoldInAir = false;
	if (Owner->GetCharacterMovement())
	{
		// 중력과 물리 충돌 방해를 막기 위해 Flying으로 전환
		Owner->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
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
			CurrentTargetActor = nullptr;
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
