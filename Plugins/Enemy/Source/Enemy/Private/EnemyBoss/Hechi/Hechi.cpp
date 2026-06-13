#include "EnemyBoss/Hechi/Hechi.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnemyProjectile/BaseEnemyProjectile.h"
#include "EnemyProjectile/BlackholeProjectile.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

AHechi::AHechi()
{
	LaserSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LaserSpawnPoint"));
	LaserSpawnPoint->SetupAttachment(RootComponent);
	
	RightHandPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RightHandPoint"));
	RightHandPoint->SetupAttachment(GetMesh(), TEXT("RightHandSocket")); // 메시의 오른손 소켓에 부착
	
}

void AHechi::BeginPlay()
{
	Super::BeginPlay();
	
	CommonBossLogData.BossID = BossLogId; // 로그 데이터에 보스 ID 기록
	
}

void AHechi::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	HandleGravityAttack(DeltaTime);
}

float AHechi::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
	AActor* DamageCauser)
{
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AHechi::Die()
{
	// 액터가 사라질 때 타이머 청소
	GetWorldTimerManager().ClearTimer(LaserTimerHandle);
	
	Super::Die();
}

UAnimMontage* AHechi::StartLaserAttack()
{
	if ( LaserAttackMontage )
	{
		PlayAnimMontage(LaserAttackMontage);
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.LaserAttackDelay); // 행동 딜레이 설정
		}
		
		return LaserAttackMontage;
	}
	return nullptr;
}

void AHechi::StartLaser()
{
	UE_LOG(LogTemp, Log, TEXT("Hechi: StartLaser"));
	
	if (LaserSpawnPoint && LaserEffect)
	{
		if (CurrentLaserComp && CurrentLaserComp->IsActive())
		{
			CurrentLaserComp->Deactivate();
			CurrentLaserComp = nullptr;
		}
		CurrentLaserComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			LaserEffect, 
			LaserSpawnPoint, 
			NAME_None, 
			FVector::Zero(),        
			FRotator::ZeroRotator,   
			EAttachLocation::KeepRelativeOffset, 
			true
		);
		
		GetWorldTimerManager().SetTimer(LaserTimerHandle, this, &AHechi::StopLaser, 2.0f, false);
	}
}

void AHechi::StopLaser()
{
	// 타이머 핸들 초기화
	GetWorldTimerManager().ClearTimer(LaserTimerHandle);

	if (CurrentLaserComp)
	{
		CurrentLaserComp->Deactivate();       // 나이아가라 시스템 페이드아웃 (부드럽게 꺼짐)
		CurrentLaserComp = nullptr;
	}
}

void AHechi::StartShootLaserProjectile()
{
	UE_LOG(LogTemp, Log, TEXT("Hechi: Start Shooting Projectiles"));

	// 1. 혹시 이미 사격 중이라면 타이머를 한 번 초기화해서 중복 실행 방지
	GetWorldTimerManager().ClearTimer(ProjectileTimerHandle);

	// 2. 함수가 호출되자마자 첫 발을 즉시 발사합니다.
	FireOneProjectile();

	// 3. ★ 핵심: ProjectileFireRate(예: 0.2초) 마다 FireOneProjectile 함수를 '무한 반복(true)' 호출합니다.
	GetWorldTimerManager().SetTimer(
		ProjectileTimerHandle, 
		this, 
		&AHechi::FireOneProjectile, 
		ProjectileFireRate, 
		true // ◀ true로 설정하여 일정 텀마다 계속 발사되도록 함
	);
}

void AHechi::FireOneProjectile()
{
	// 실제로 발사체를 월드에 스폰하는 로직
	if (LaserProjectileClass && LaserSpawnPoint)
	{
		FVector SpawnLocation = LaserSpawnPoint->GetComponentLocation();
       
		// 1. 컴포넌트의 원래 회전 값을 가져옵니다.
		FRotator SpawnRotation = LaserSpawnPoint->GetComponentRotation();

		// 2. ★ Y축 회전(Pitch) 값에 90도를 더해 각도를 정면으로 들어 올립니다.
		SpawnRotation.Pitch += 90.0f; 

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		// 보정된 SpawnRotation을 넣어 발사체를 스폰합니다.
		GetWorld()->SpawnActor<ABaseEnemyProjectile>(
		   LaserProjectileClass, 
		   SpawnLocation, 
		   SpawnRotation, 
		   SpawnParams
		);
	}
}

void AHechi::StopShootLaserProjectole()
{
	UE_LOG(LogTemp, Log, TEXT("Hechi: Stop Shooting Projectiles"));

	// ★ 타이머를 클리어하면 즉시 연사가 멈춥니다.
	GetWorldTimerManager().ClearTimer(ProjectileTimerHandle);
}

UAnimMontage* AHechi::PlayGravityAttack()
{
	if ( GravityAttackMontage )
	{
		PlayAnimMontage(GravityAttackMontage);
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.GravityAttackDelay); // 행동 딜레이 설정
		}
		
		return GravityAttackMontage;
	}
	return nullptr;
}

void AHechi::StartGravityAttack()
{
	if (!TargetCharacter || !GravityGroundEffect) return;

	UCharacterMovementComponent* MoveComp = TargetCharacter->GetCharacterMovement();
	if (MoveComp)
	{
		DefaultGravityScale = MoveComp->GravityScale;
		DefaultAirControl = MoveComp->AirControl;
		DefaultMaxWalkSpeed = MoveComp->MaxWalkSpeed;
	}
	
	// 바닥 위치 계산 로직 (기존과 동일)
	const FVector CharacterLocation = TargetCharacter->GetActorLocation();
	FVector SpawnLocation = CharacterLocation;
	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	if ( TargetCharacter )TraceParams.AddIgnoredActor(TargetCharacter);
	TraceParams.AddIgnoredActor(this);

	TArray<AActor*> PetActors;
	TArray<AActor*> EnemyActors;

	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Pet"), PetActors);
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Enemy"), EnemyActors);

	// TraceParams에 각각 추가
	TraceParams.AddIgnoredActors(PetActors);
	TraceParams.AddIgnoredActors(EnemyActors);
	
	if (GetWorld()->LineTraceSingleByChannel(HitResult, CharacterLocation, CharacterLocation - FVector(0.f, 0.f, 1000.f), ECC_WorldStatic, TraceParams))
	{
		SpawnLocation = HitResult.Location;
	}

	// 상태값 설정
	GravityAttackCenter = SpawnLocation + FVector(0.f, 0.f, GravityHalfHeight - 200.f);
	GravityTimer = 0.0f; // 타이머 초기화

	// 이펙트 스폰
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), GravityGroundEffect, SpawnLocation + FVector(0.f, 0.f, 2.f), FRotator::ZeroRotator, FVector(1.f));

	// 시전 시간 뒤에 실제 캡슐 판정 활성화
	FTimerHandle GravityActivationTimer;
	GetWorldTimerManager().SetTimer(GravityActivationTimer, [this]()
	{
		bIsGravityAttackActive = true; 
	}, 1.0f, false); // 1.0초 지연 (원하는 시간으로 조절 가능)
	
	FTimerHandle ImpactEffectTimer;
	// 람다 캡처에 SpawnLocation을 추가하여 시작 시점의 바닥 위치를 기억하게 합니다.
	GetWorldTimerManager().SetTimer(ImpactEffectTimer, [this, SpawnLocation]()
	{
		if (GravityImpactEffect)
		{
			// 시작 시 장판(GravityGroundEffect) 위치와 동일한 위치에서 Z축으로 -100만큼 조정
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), GravityImpactEffect,
				SpawnLocation + FVector(0.f, 0.f, -100.f), 
				FRotator::ZeroRotator, FVector(1.f));
		}
	}, 4.7f, false);
	
	// 일정 시간 뒤 종료 예약
	FTimerHandle GravityEndTimer;
	GetWorldTimerManager().SetTimer(GravityEndTimer, this, &AHechi::EndGravityAttack, GravityDuration, false);
}

void AHechi::EndGravityAttack()
{
	bIsGravityAttackActive = false;
	
	// 1. 범위 내의 모든 캐릭터를 찾기 위한 설정
	TArray<FOverlapResult> OverlapResults;
	FCollisionShape GravityCapsule = FCollisionShape::MakeCapsule(GravityRadius, GravityHalfHeight);
	FCollisionQueryParams OverlapParams;
	OverlapParams.AddIgnoredActor(this);

	// 2. 공격 종료 시 캡슐 범위 내에 있는 캐릭터들 체크
	if (GetWorld()->OverlapMultiByChannel(OverlapResults, GravityAttackCenter, FQuat::Identity, ECC_Pawn, GravityCapsule, OverlapParams))
	{
		for (auto& Result : OverlapResults)
		{
			ACharacter* OverlappedChar = Cast<ACharacter>(Result.GetActor());
			if (OverlappedChar && OverlappedChar->ActorHasTag(TEXT("Player")))
			{
				UCharacterMovementComponent* MoveComp = OverlappedChar->GetCharacterMovement();
				if (MoveComp)
				{
					// 1. 원래 상태로 복구
					MoveComp->GravityScale = DefaultGravityScale;
					MoveComp->AirControl = DefaultAirControl;
					MoveComp->MaxWalkSpeed = DefaultMaxWalkSpeed;

					// 2. 바닥으로 빠르게 발사 (Z축 하향 속도 부여)
					// -1500.f 정도면 매우 빠르게 바닥으로 내리꽂힙니다.
					FVector SlamVelocity = FVector(0.f, 0.f, -4000.f); 
					OverlappedChar->LaunchCharacter(SlamVelocity, false, true);
                
					// 대미지 주기
					UGameplayStatics::ApplyDamage(OverlappedChar, 
						AttackStruct.GravityAttackDamage, GetController(), this, UDamageType::StaticClass());
					
					CommonBossLogData.TotalDamageDealt += AttackStruct.GravityAttackDamage;
					//BossSkeletonMageLogData.GravityAttackDamage += AttackStruct.GravityAttackDamage;
					
					UEnemyLogManager::EnemyLog(EEnemyLogType::SkeletonMage, 
						FString::Printf(TEXT("[스켈레톤 메이지] 중력 공격으로 플레이어에게 %.2f 대미지"), AttackStruct.GravityAttackDamage));
					
					//BossSkeletonMageLogData.GravityAttackDamage += AttackStruct.GravityAttackDamage;
				}
			}
		}
	}
	
	// 3. (혹시 범위 밖에 나갔더라도) 타겟팅된 메인 플레이어는 확실하게 복구
	if (TargetCharacter)
	{
		UCharacterMovementComponent* MoveComp = TargetCharacter->GetCharacterMovement();
		if (MoveComp && MoveComp->GravityScale < 1.0f)
		{
			MoveComp->GravityScale = DefaultGravityScale;
			MoveComp->AirControl = DefaultAirControl;
			MoveComp->MaxWalkSpeed = DefaultMaxWalkSpeed;
		}
	}
}

void AHechi::HandleGravityAttack(float DeltaTime)
{
	if (bIsGravityAttackActive)
	{
		// 디버그 캡슐 그리기는 매 프레임 실행 (시각화 유지용)
		//DrawDebugCapsule(GetWorld(), GravityAttackCenter, GravityHalfHeight, GravityRadius, 
		//FQuat::Identity, FColor::Purple, false, DeltaTime * 2.f, 0, 1.0f);

		// 범위 체크
		TArray<FOverlapResult> OverlapResults;
		FCollisionShape GravityCapsule = FCollisionShape::MakeCapsule(GravityRadius, GravityHalfHeight);
		FCollisionQueryParams OverlapParams;
		OverlapParams.AddIgnoredActor(this);

		bool bIsPlayerInArea = false;
		if (GetWorld()->OverlapMultiByChannel(OverlapResults, GravityAttackCenter, FQuat::Identity, ECC_Pawn, GravityCapsule, OverlapParams))
		{
			for (auto& Result : OverlapResults)
			{
				ACharacter* OverlappedChar = Cast<ACharacter>(Result.GetActor());
				if (OverlappedChar && OverlappedChar->ActorHasTag(TEXT("Player")))
				{
					bIsPlayerInArea = true;
					UCharacterMovementComponent* MoveComp = OverlappedChar->GetCharacterMovement();
					if (MoveComp)
					{
						// 영역 안에 있는 동안 지속적으로 무중력 적용
						MoveComp->GravityScale = 0.05f; 
						MoveComp->AirControl = 0.7f;  // 공중 제어력 증가
						MoveComp->MaxWalkSpeed = 200.f; // 좀 느리게
						
						// Z축으로 살짝 뜨게 하는 힘 (둥둥 뜨는 느낌)
						if (OverlappedChar->GetVelocity().Z < 100.f)
						{
							MoveComp->AddImpulse(FVector(0.f, 0.f, 20.f), true);
						}
					}
				}
			}
		}

		// 만약 플레이어가 영역 밖으로 나갔다면 중력 원복
		if (!bIsPlayerInArea && TargetCharacter)
		{
			UCharacterMovementComponent* MoveComp = TargetCharacter->GetCharacterMovement();
			if (MoveComp && MoveComp->GravityScale < 1.0f)
			{
				MoveComp->GravityScale = DefaultGravityScale;
				MoveComp->AirControl = DefaultAirControl;
				MoveComp->MaxWalkSpeed = DefaultMaxWalkSpeed;
			}
		}
	}
}

UAnimMontage* AHechi::PlayTeleportMontage(const FVector& Destination)
{	
	if ( TeleportMontage )
	{
		PlayAnimMontage(TeleportMontage);
			
		TeleportDestination = Destination;
		
		if ( BlackboardComp ) BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.TeleportDelay); // 행동 딜레이 설정
		
		return TeleportMontage;
	}
	return nullptr;
}

void AHechi::TeleportMoveToNextPoint()
{
	if (TeleportDestination != FVector::ZeroVector)
	{
		//SpawnTeleportEffectAtLocation(GetActorLocation()); // 현재 위치에 이펙트 생성
		
		// --- 수정된 부분: 캡슐 절반 높이만큼 위로 오프셋을 주어 바닥 위에 정상적으로 서도록 함 ---
		float SafeZOffset = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
		FVector AdjustedDestination = TeleportDestination + FVector(0.f, 0.f, SafeZOffset);
		
		SetActorLocation(AdjustedDestination); // 조정된 위치로 텔레포트 이동
		
		
		// TargetCharacter를 찾아서 바라보도록 회전
		if (TargetCharacter)
		{
			const FVector TargetLocation = TargetCharacter->GetActorLocation();
			// 현재 위치에서 타겟 위치를 바라보는 회전값 계산
			const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), TargetLocation);

			// Z축(Yaw) 회전만 적용하여 수평으로 바라보게 함
			SetActorRotation(FRotator(0.f, LookAtRotation.Yaw, 0.f));
		}
		
	}
}

void AHechi::StartDisappear()
{
	// 1. 플래그를 true로 설정
	HandleTeleportFlag = true;

	// 만약 기존에 돌고 있던 타이머가 있다면 초기화 (안전장치)
	GetWorldTimerManager().ClearTimer(TeleportTimerHandle);
	
	// 3. 타이머 세팅: Duration 초 후에 EndDisappear 함수를 '한 번만' 실행합니다.
	GetWorldTimerManager().SetTimer(
		TeleportTimerHandle, 
		this, 
		&AHechi::EndDisappear, 
		DisappearDuration, 
		false // 반복 여부 (false = 1회성)
	);
}

void AHechi::EndDisappear()
{
	HandleTeleportFlag = false;
}

void AHechi::SetMeshHidden()
{
	GetMesh()->SetVisibility(false);
}

void AHechi::SetMeshVissible()
{	
	GetMesh()->SetVisibility(true);
}

void AHechi::SpawnBlackhole()
{
	if ( BlackholeProjectileClass )
	{	
		FVector SpawnLocation = GetActorLocation();
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		GetWorld()->SpawnActor<ABlackholeProjectile>(
			BlackholeProjectileClass, 
			SpawnLocation, 
			FRotator::ZeroRotator, 
			SpawnParams
		);
	}
}

#if WITH_EDITOR
void AHechi::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AHechi, bDebugMode))
	{
		if ( bDebugMode == true )
		{

		}
		else
		{

		}
	}
}
#endif