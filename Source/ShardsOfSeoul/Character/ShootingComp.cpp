#include "ShootingComp.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "ShardsOfSeoul.h"
#include "ShardsOfSeoulCharacter.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

UShootingComp::UShootingComp()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 보간 동작 시 엇박자 흔들림 방지를 위해 물리 처리 후로 틱 배치
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UShootingComp::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		SpringArmComp = Cast<USpringArmComponent>(OwnerCharacter->GetComponentByClass(USpringArmComponent::StaticClass()));
		CameraComp = Cast<UCameraComponent>(OwnerCharacter->GetComponentByClass(UCameraComponent::StaticClass()));
		

		
		if (SpringArmComp)
		{
			DefaultArmLength = SpringArmComp->TargetArmLength;
			DefaultSocketOffset = SpringArmComp->SocketOffset;
			// 캐릭터 캡슐 간섭 지터링 방지용 충돌구 크기 축소 (지형/벽 충돌 테스트 상시 유지 활성화)
			SpringArmComp->ProbeSize = 3.f;
		}

		// Pistol Static Mesh 동적 생성 및 소켓 부착
		if (PistolStaticMesh)
		{
			PistolMeshComponent = NewObject<UStaticMeshComponent>(OwnerCharacter, TEXT("PistolMeshComponent"));
			if (PistolMeshComponent)
			{
				PistolMeshComponent->RegisterComponent();
				PistolMeshComponent->SetStaticMesh(PistolStaticMesh);
				PistolMeshComponent->AttachToComponent(
					OwnerCharacter->GetMesh(),
					FAttachmentTransformRules::SnapToTargetIncludingScale,
					PistolSocketName
				);
				
				// 부착 후 엔진의 상대 스케일 연산 꼬임 방지를 위해 상대 스케일을 명시적으로 1.0으로 초기화
				PistolMeshComponent->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));
				
				// 총기에도 상시 커스텀 뎁스(Custom Depth) 및 아웃라인용 스텐실 1번 켜두기 설정
				PistolMeshComponent->SetRenderCustomDepth(true);
				PistolMeshComponent->SetCustomDepthStencilValue(2);
				
				PistolMeshComponent->SetVisibility(false);
			}
		}
		
		// 게임 시작 시에는 줌 변경 중이 아니므로 틱을 강제 잠재워 둡니다 (최적화)
		SetComponentTickEnabled(false);
	}
}

void UShootingComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (SpringArmComp)
	{
		float TargetArmLength = bIsAiming ? AimArmLength : DefaultArmLength;
		FVector TargetSocketOffset = bIsAiming ? AimSocketOffset : DefaultSocketOffset;
		
		float LengthDiff = FMath::Abs(SpringArmComp->TargetArmLength - TargetArmLength);
		float OffsetDiff = FVector::Dist(SpringArmComp->SocketOffset, TargetSocketOffset);
		
		// 1. 카메라가 목표 위치에 완벽히 수렴했을 때
		if (LengthDiff < 0.1f && OffsetDiff < 0.1f)
		{
			SpringArmComp->TargetArmLength = TargetArmLength;
			SpringArmComp->SocketOffset = TargetSocketOffset;
			
			// 조준이 풀린 완료 상황에서만 틱을 완전히 잠재웁니다 (조준 중에는 실시간 사거리 조준 피드백을 위해 틱을 유지합니다)
			if (!bIsAiming)
			{
				SetComponentTickEnabled(false);
				UE_LOG(LogShardsOfSeoul, Warning, TEXT("[Shooting] Camera returned to default view. Disabling Tick."));
			}
		}
		else
		{
			// 2. 아직 목적지에 도달하지 않았다면 부드럽게 보간 진행
			SpringArmComp->TargetArmLength = FMath::FInterpTo(SpringArmComp->TargetArmLength, TargetArmLength, DeltaTime, InterpSpeed);
			SpringArmComp->SocketOffset = FMath::VInterpTo(SpringArmComp->SocketOffset, TargetSocketOffset, DeltaTime, InterpSpeed);
		}
	}

	// 2. 조준 중일 때 매 틱마다 사정거리 내 타격 유효 검출 및 크로스헤어 UI 투명도 업데이트
	if (bIsAiming && AimWidgetInstance && CameraComp)
	{
		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Add(OwnerCharacter);

		// 2-1. 카메라 조준점(영점 일치점) 1차 검출
		FVector CamStartLoc = CameraComp->GetComponentLocation();
		FVector CamForwardDir = CameraComp->GetForwardVector();
		FVector CamEndLoc = CamStartLoc + (CamForwardDir * MaxFireDistance);
		
		FHitResult CamHitResult;
		bool bCamHit = UKismetSystemLibrary::LineTraceSingle(
			GetWorld(),
			CamStartLoc,
			CamEndLoc,
			UEngineTypes::ConvertToTraceType(ECC_Visibility),
			false,
			ActorsToIgnore,
			EDrawDebugTrace::None,
			CamHitResult,
			true
		);
		
		FVector TargetWorldLoc = bCamHit ? CamHitResult.ImpactPoint : CamEndLoc;

		// 2-2. 총구 위치 획득
		FVector MuzzleLoc = FVector::ZeroVector;
		FRotator MuzzleRot = FRotator::ZeroRotator;
		GetMuzzleLocationAndRotation(MuzzleLoc, MuzzleRot);

		// 2-3. 실제 총구에서 조준 방향으로의 사정거리 유효 트레이스 수행
		FVector FireDir = (TargetWorldLoc - MuzzleLoc).GetSafeNormal();
		FVector MuzzleTraceEnd = MuzzleLoc + (FireDir * MaxFireDistance);

		FHitResult RangeCheckHit;
		bool bValidTargetInRange = UKismetSystemLibrary::LineTraceSingle(
			GetWorld(),
			MuzzleLoc,
			MuzzleTraceEnd,
			UEngineTypes::ConvertToTraceType(ECC_Visibility),
			false,
			ActorsToIgnore,
			EDrawDebugTrace::None, // 디버그 드로우 안함
			RangeCheckHit,
			true
		);
		
		// 무언가 사격 거리에 닿으면 진하게(1.0), 닿지 않는 허공이면 흐리게(DefaultWidgetOpacity) 실시간 보간 처리
		float CurrentOpacity = AimWidgetInstance->GetRenderOpacity();
		float TargetOpacity = bValidTargetInRange ? 1.f : DefaultWidgetOpacity;
		
		// 투명도 변경이 너무 급격히 튄다면 FInterpTo로 약간의 스무딩을 가해 부드럽게 깜빡이도록 연출
		float NewOpacity = FMath::FInterpTo(CurrentOpacity, TargetOpacity, DeltaTime, 15.f);
		AimWidgetInstance->SetRenderOpacity(NewOpacity);

		// coloring 태그를 가진 액터 타겟팅 및 스텐실 실시간 아웃라인 토글 연동
		AActor* HitActor = bValidTargetInRange ? RangeCheckHit.GetActor() : nullptr;
		if (HitActor && HitActor->ActorHasTag(FName("coloring")))
		{
			if (LastColoringTargetActor.Get() != HitActor)
			{
				// 1. 이전 타겟 아웃라인(스텐실 1) 끄기 (스텐실 값이 2가 아니라면 안전하게 차단)
				if (LastColoringTargetActor.IsValid())
				{
					TArray<UPrimitiveComponent*> PrimitiveComps;
					LastColoringTargetActor->GetComponents<UPrimitiveComponent>(PrimitiveComps);
					for (UPrimitiveComponent* PrimComp : PrimitiveComps)
					{
						if (PrimComp && PrimComp->CustomDepthStencilValue == 1)
						{
							PrimComp->SetRenderCustomDepth(false);
						}
					}
				}

				// 2. 새로운 타겟 아웃라인 켜기 (이미 스텐실 2번(채도 복원 완료) 상태가 아니라면 스텐실 1 할당)
				TArray<UPrimitiveComponent*> PrimitiveComps;
				HitActor->GetComponents<UPrimitiveComponent>(PrimitiveComps);
				for (UPrimitiveComponent* PrimComp : PrimitiveComps)
				{
					if (PrimComp && PrimComp->CustomDepthStencilValue != 2)
					{
						PrimComp->SetRenderCustomDepth(true);
						PrimComp->SetCustomDepthStencilValue(1);
					}
				}

				LastColoringTargetActor = HitActor;
			}
		}
		else
		{
			// 타겟을 아예 잃었거나 coloring 태그가 없는 경우 이전 타겟 아웃라인 해제
			if (LastColoringTargetActor.IsValid())
			{
				TArray<UPrimitiveComponent*> PrimitiveComps;
				LastColoringTargetActor->GetComponents<UPrimitiveComponent>(PrimitiveComps);
				for (UPrimitiveComponent* PrimComp : PrimitiveComps)
				{
					if (PrimComp && PrimComp->CustomDepthStencilValue == 1)
					{
						PrimComp->SetRenderCustomDepth(false);
					}
				}
				LastColoringTargetActor = nullptr;
			}
		}
	}
}

void UShootingComp::StartAiming()
{
	if (bIsAiming) return;
	
	bIsAiming = true;
	
	if (OwnerCharacter)
	{
		// 1. 캐릭터가 조준 시 카메라 방향을 상시 동기화하도록 Yaw 회전 락 활성화
		OwnerCharacter->bUseControllerRotationYaw = true;

		// 2. 조준 시 캐릭터 무브먼트의 이동 방향 정렬 비활성화 (시선 회전과의 힘겨루기 지터 방지)
		if (OwnerCharacter->GetCharacterMovement())
		{
			OwnerCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
		}

		// 3. 조준 중 카메라 충돌 테스트 해제 및 틱 활성화 (보간 재생 시작)
		SetComponentTickEnabled(true);

		// 4. 조준 위젯 생성 및 출력
		if (AimWidgetClass)
		{
			APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
			if (PC)
			{
				if (!AimWidgetInstance)
				{
					AimWidgetInstance = CreateWidget<UUserWidget>(PC, AimWidgetClass);
				}
				
				if (AimWidgetInstance && !AimWidgetInstance->IsInViewport())
				{
					AimWidgetInstance->AddToViewport();
					// 위젯 생성 시 최초 틱 대입 전까지 기본 반투명 opacity 적용
					AimWidgetInstance->SetRenderOpacity(DefaultWidgetOpacity);
				}
			}
		}

		// 5. 권총 스태틱 메시 활성화
		if (PistolMeshComponent)
		{
			PistolMeshComponent->SetVisibility(true);
		}
	}
}

void UShootingComp::StopAiming()
{
	if (!bIsAiming) return;
	
	bIsAiming = false;
	
	if (OwnerCharacter)
	{
		// 1. 캐릭터 Yaw 회전 락 비활성화 (원래 캐릭터 회전 세팅 복원)
		OwnerCharacter->bUseControllerRotationYaw = false;

		// 2. 캐릭터 무브먼트의 이동 방향 정렬 원래대로 활성화
		if (OwnerCharacter->GetCharacterMovement())
		{
			OwnerCharacter->GetCharacterMovement()->bOrientRotationToMovement = true;
		}

		// 3. 카메라 충돌 테스트 복구 및 틱 활성화 (원래 뷰 복구 보간 재생 시작)
		SetComponentTickEnabled(true);

		// 4. 조준 위젯 화면에서 제거
		if (AimWidgetInstance)
		{
			AimWidgetInstance->RemoveFromParent();
		}

		// 5. 권총 스태틱 메시 숨기기
		if (PistolMeshComponent)
		{
			PistolMeshComponent->SetVisibility(false);
		}

		// 6. 조준이 풀릴 때 바라보고 있던 coloring 타겟의 아웃라인을 안전하게 제거
		if (LastColoringTargetActor.IsValid())
		{
			TArray<UPrimitiveComponent*> PrimitiveComps;
			LastColoringTargetActor->GetComponents<UPrimitiveComponent>(PrimitiveComps);
			for (UPrimitiveComponent* PrimComp : PrimitiveComps)
			{
				if (PrimComp && PrimComp->CustomDepthStencilValue == 1)
				{
					PrimComp->SetRenderCustomDepth(false);
				}
			}
			LastColoringTargetActor = nullptr;
		}
	}
}

void UShootingComp::Fire()
{
	// 조준(줌) 상태가 아닐 때는 사격 제한
	if (!bIsAiming) return;

	if (!OwnerCharacter || !CameraComp) return;

	// 움직이고 있는 상태에서는 사격을 아예 제한 (제자리에 멈춰 서서만 사격 가능)
	if (OwnerCharacter->GetVelocity().Size2D() > 10.f)
	{
		UE_LOG(LogShardsOfSeoul, Warning, TEXT("[Shooting] Cannot fire while moving. Must stand still."));
		return;
	}

	// 사격 애니메이션 몽타주 재생
	if (FireMontage)
	{
		OwnerCharacter->PlayAnimMontage(FireMontage);
	}
	
	// 1. 총구 소켓(PistolMuzzle)의 월드 위치 및 회전값 획득
	FVector MuzzleLoc = FVector::ZeroVector;
	FRotator MuzzleRot = FRotator::ZeroRotator;
	GetMuzzleLocationAndRotation(MuzzleLoc, MuzzleRot);
	
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerCharacter);

	// 2. 카메라 조준선 기준 기본 목적지(영점 정렬 지점) 검출
	FVector CamStartLoc = CameraComp->GetComponentLocation();
	FVector CamForwardDir = CameraComp->GetForwardVector();
	FVector CamEndLoc = CamStartLoc + (CamForwardDir * MaxFireDistance);
	
	FHitResult CamHitResult;
	bool bCamHit = UKismetSystemLibrary::LineTraceSingle(
		GetWorld(),
		CamStartLoc,
		CamEndLoc,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		CamHitResult,
		true
	);
	
	FVector TargetWorldLoc = bCamHit ? CamHitResult.ImpactPoint : CamEndLoc;

	// 3. 움직일 때 집탄률 감소(탄퍼짐 - Spread) 수식 적용
	float CurrentSpeed = OwnerCharacter->GetVelocity().Size2D();
	if (CurrentSpeed >= 50.f)
	{
		// 속도에 비례하여 최대 분산 반경을 무작위 구형 오프셋으로 설정해 목표점에 가산
		float MaxSpread = (CurrentSpeed / 300.f) * MovementSpreadMultiplier;
		FVector RandomSpread = FMath::VRand() * FMath::FRandRange(0.f, MaxSpread);
		TargetWorldLoc += RandomSpread;
	}

	// 4. 총구에서 대상을 바라보는 정방향 사격 벡터를 구하고, 이를 총구 기준으로 정직하게 MaxFireDistance만큼 연장시킵니다.
	// 이 보정식 덕분에 카메라의 후방 위치로 인해 실제 사격 거리가 뚝 끊겨 씹히는 기하학적 버그가 완벽히 소멸합니다.
	FVector FireDir = (TargetWorldLoc - MuzzleLoc).GetSafeNormal();
	FVector EndLoc = MuzzleLoc + (FireDir * MaxFireDistance);
	
	FHitResult MuzzleHitResult;
	
	// 최종 격발 라인 트레이스 수행
	bool bMuzzleHit = UKismetSystemLibrary::LineTraceSingle(
		GetWorld(),
		MuzzleLoc,
		EndLoc,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,
		ActorsToIgnore,
		bDrawDebugLine ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, // 디버그 선 온오프 토글 적용
		MuzzleHitResult,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		1.f // 1초 동안 궤적 표시
	);

	// 4. 리본 나이아가라 총알 궤적(Tracer) VFX 스폰
	// 아무것도 맞지 않았을 때(허공 사격)는 총구 발사 방향으로 최대 사거리(MaxFireDistance)까지 빔을 연장하여 리본이 도중에 끊기지 않게 방지합니다.
	FVector HitPoint = bMuzzleHit ? MuzzleHitResult.ImpactPoint : (MuzzleLoc + FireDir * MaxFireDistance);
	
	if (TracerEffect)
	{
		UNiagaraComponent* TracerComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			TracerEffect,
			MuzzleLoc
		);
		if (TracerComp)
		{
			// 나이아가라 리본 빔의 시작점 및 끝점 월드 좌표 바인딩 (월드 스페이스 빔 연동)
			TracerComp->SetVariableVec3(FName("User.BeamStart"), MuzzleLoc);
			TracerComp->SetVariableVec3(FName("User.BeamEnd"), HitPoint);
		}
	}

	// 5. 피격점 위치에 피격(스파크/먼지) 나이아가라 VFX 스폰
	// 대부분의 먼지/스파크 VFX 이미터는 로컬 Z축(0,0,1) 방향으로 분사되도록 설계되어 있습니다.
	// 따라서 단순 Rotation() 대신 MakeFromZ를 이용해 법면 방향이 이펙트의 로컬 Z축이 되도록 정렬하여 스폰합니다.
	// 랜드스케이프(지형)나 평지 피격 시 지면 아래로 파편이 파묻혀 가려지는 클리핑 현상을 막기 위해, 법면 방향으로 미세하게(5.f 유닛) 오프셋을 띄워 스폰합니다.
	if (bMuzzleHit && ImpactEffect)
	{
		FVector SprungHitPoint = HitPoint + (MuzzleHitResult.ImpactNormal * 5.f);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ImpactEffect,
			SprungHitPoint,
			FRotationMatrix::MakeFromZ(MuzzleHitResult.ImpactNormal).Rotator()
		);
	}
	
	// 실제 타격 판정 및 데미지 처리
	if (bMuzzleHit && MuzzleHitResult.GetActor())
	{
		AActor* HitActor = MuzzleHitResult.GetActor();
		UE_LOG(LogShardsOfSeoul, Warning, TEXT("[Shooting] Weapon Hit Actor: %s, Component: %s"), *HitActor->GetName(), *MuzzleHitResult.GetComponent()->GetName());
		
		// 피격한 적이 Enemy 태그를 가지고 있으면 데미지 전달
		if (HitActor->ActorHasTag(FName("Enemy")))
		{
			AController* InstigatorController = OwnerCharacter->GetController();
			
			UGameplayStatics::ApplyDamage(
				HitActor,
				BaseDamage,
				InstigatorController,
				OwnerCharacter,
				UDamageType::StaticClass()
			);
			
			UE_LOG(LogShardsOfSeoul, Warning, TEXT("[Shooting] Damage Applied: %f to Enemy %s"), BaseDamage, *HitActor->GetName());
		}
	}
}

bool UShootingComp::GetMuzzleLocationAndRotation(FVector& OutLoc, FRotator& OutRot) const
{
	OutLoc = FVector::ZeroVector;
	OutRot = FRotator::ZeroRotator;
	bool bFound = false;

	// 1순위: 무기 스태틱 메시 컴포넌트의 PistolMuzzle 소켓 조회
	if (PistolMeshComponent && PistolMeshComponent->DoesSocketExist(FName("PistolMuzzle")))
	{
		OutLoc = PistolMeshComponent->GetSocketLocation(FName("PistolMuzzle"));
		OutRot = PistolMeshComponent->GetSocketRotation(FName("PistolMuzzle"));
		bFound = true;
	}
	// 2순위: 캐릭터 스켈레탈 메시의 PistolMuzzle 소켓 조회 (소켓이 캐릭터에 박혀 있는 경우 대비)
	else if (OwnerCharacter && OwnerCharacter->GetMesh() && OwnerCharacter->GetMesh()->DoesSocketExist(FName("PistolMuzzle")))
	{
		OutLoc = OwnerCharacter->GetMesh()->GetSocketLocation(FName("PistolMuzzle"));
		OutRot = OwnerCharacter->GetMesh()->GetSocketRotation(FName("PistolMuzzle"));
		bFound = true;
	}

	// 3순위 (폴백): 어디에도 소켓이 없는 경우 오른손 장착 소켓(PistolSocket) 사용
	if (!bFound)
	{
		if (OwnerCharacter && OwnerCharacter->GetMesh())
		{
			OutLoc = OwnerCharacter->GetMesh()->GetSocketLocation(PistolSocketName);
			OutRot = OwnerCharacter->GetMesh()->GetSocketRotation(PistolSocketName);
			bFound = true;
		}
	}

	return bFound;
}
