#include "EnemyBoss/Hechi/Hechi.h"

#include "CSVLog.h"
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
#include "EngineUtils.h"
#include "Components/ProgressBar.h"
#include "EnemyHUD/BossHealthBarWidget.h"

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
	
	InitializePostProcessVolume(); // 포스트 프로세스 볼륨 초기화
	
	CommonBossLogData.BossID = BossLogId; // 로그 데이터에 보스 ID 기록
	
	
	if ( bDebugMode == true )
	{
		GetWorldTimerManager().SetTimer(
			RepeatingTimerHandle,                      // 타이머 핸들
			this,                                      // 대상 클래스 (자기 자신)
			&AHechi::MyThreeSecondRepeatingFunction,   // 반복 실행할 함수의 주소
			3.0f,                                      // 실행 간격 (초 단위)
			true                                       // true = 무한 반복 실행 (false면 1번만 실행됨)
		);
	}
	
}

void AHechi::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	HandleGravityAttack(DeltaTime);
}

float AHechi::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
	AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 3. 체력 비율 체크를 통한 2페이즈(특수 패턴) 진입 로직
	if (Health > 0.f && bIsChangeMap == false)
	{
		// 현재 체력 비율 계산 (MaxHealth가 0이 아님을 전제)
		float CurrentHealthRatio = Health / MaxHealth;

		if (CurrentHealthRatio <= ChangeMapHealthThreshold)
		{
			bIsChangeMap = true; // 중복 실행 방지
			
			// 블랙보드 값 업데이트 (StartSecondPhase 함수 호출)
			StartChangeMapPattern(); 
		}
	}
	
	return ActualDamage;
}

void AHechi::Die()
{
	ShakeCamera();
	
	FTimerManager& TimerManager = GetWorldTimerManager();
	
	TimerManager.ClearTimer(RepeatingTimerHandle);
	
	//  레이저 패턴 타이머 청소
	TimerManager.ClearTimer(LaserTimerHandle);
	//  레이저 발사체(연사) 패턴 타이머 청소
	TimerManager.ClearTimer(ProjectileTimerHandle);
	//  보스 자체 텔레포트 타이머 청소
	TimerManager.ClearTimer(TeleportTimerHandle);
	//  2페이즈 맵 변경 시 캐릭터 강제 텔레포트 타이머 청소
	TimerManager.ClearTimer(CharacterTeleportTimerHandle);
	//  5초마다 무한 반복되던 포스트 프로세스 변경 루프 타이머 청소
	TimerManager.ClearTimer(PostProcessLoopHandle);
	
	RandomChangeLevelPostProcessVolume->Settings.WeightedBlendables.Array.Empty();
	
	FirstChangeLevelPostProcessVolume->Settings.WeightedBlendables.Array.Empty();
	
	Super::Die();
}

void AHechi::Destroy()
{
	if (BossHealthBar)
	{
		BossHealthBar->RemoveFromParent();
		BossHealthBar = nullptr;
	}

	Super::Destroy();
}

void AHechi::AfterDieMontageEnd()
{
	if ( GetMesh() )
	{
		GetMesh()->bPauseAnims = true;
	}
	
	if (TargetCharacter && TargetCharacter->Implements<UItemDropInterface>())
	{
		IItemDropInterface::Execute_HandleEnemyDeadAndDropItem(TargetCharacter, this);
	}
	
	FTimerHandle DestroyTimerHandle;
	GetWorldTimerManager().SetTimer(DestroyTimerHandle, [this]()
	{
	   Destroy(); // 그 후 안전하게 액터 삭제
	}, 7.0f, false);
}

void AHechi::MyThreeSecondRepeatingFunction()
{	
	if (Health <= 0.f) return;

	// 체력을 깎은 후, 0.f와 비교하여 더 큰 값을 대입 (0 미만으로 내려가지 않음)
	Health = FMath::Max(Health - 10.f, 0.f);
    
	if (Health <= 0.f)
	{
		//  디버그 및 기본 반복 타이머 청소
		GetWorldTimerManager().ClearTimer(RepeatingTimerHandle);
		
		// 체력 바를 0%로 확실하게 세팅하고 사망 처리
		if (BossHealthBar && BossHealthBar->HealthProgressBar)
		{
			BossHealthBar->HealthProgressBar->SetPercent(0.f);
		}
		Health = 0.f;
		Die();
		return;
	}
    
	if (BossHealthBar && BossHealthBar->HealthProgressBar)
	{
		BossHealthBar->HealthProgressBar->SetPercent(Health / MaxHealth);
	}
	SetHitOverlay(); // 히트 오버레이 설정
	
	if (Health > 0.f && bIsChangeMap == false)
	{
		// 현재 체력 비율 계산 (MaxHealth가 0이 아님을 전제)
		float CurrentHealthRatio = Health / MaxHealth;

		if (CurrentHealthRatio <= ChangeMapHealthThreshold)
		{
			bIsChangeMap = true; // 중복 실행 방지
			
			// 블랙보드 값 업데이트 (StartSecondPhase 함수 호출)
			StartChangeMapPattern(); 
		}
	}
}

void AHechi::InitializePostProcessVolume()
{
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		APostProcessVolume* Volume = *It;
		if (!Volume) continue;

		if (Volume->ActorHasTag(TEXT("Hechi")))
		{
			RandomChangeLevelPostProcessVolume = Volume;
		}
		else if (Volume->ActorHasTag(TEXT("Hechi_First")))
		{
			FirstChangeLevelPostProcessVolume = Volume;
		}

		// 최적화: 만약 두 개를 모두 찾았다면 더 이상 돌 필요가 없으므로 루프 탈출
		if (RandomChangeLevelPostProcessVolume && FirstChangeLevelPostProcessVolume)
		{
			break; 
		}
	}
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

UAnimMontage* AHechi::PlayThrowMagicBallMontage()
{
	if ( ThrowMagicBallMontage )
	{
		PlayAnimMontage(ThrowMagicBallMontage);
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.ThrowMagicBallDelay); // 행동 딜레이 설정
		}
		
		return ThrowMagicBallMontage;
	}
	return nullptr;
}

void AHechi::ShootMagickBall()
{
	// 필요한 모든 컴포넌트와 변수가 유효한지 확인합니다.
	if (!IsValid(TargetCharacter) || !IsValid(RightHandPoint) || MagicBallProjectileClass == nullptr)
	{
		return;
	}

	// RightHandPoint의 월드 위치와 회전값을 가져옵니다.
	const FVector SpawnLocation = RightHandPoint->GetComponentLocation();

	// 발사 위치에서 타겟을 향하는 방향을 계산합니다.
	const FVector TargetLocation = TargetCharacter->GetActorLocation();
	const FRotator SpawnRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation, TargetLocation);

	// 스폰 파라미터를 설정합니다.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 월드에 발사체를 스폰합니다.
	GetWorld()->SpawnActor<ABaseEnemyProjectile>(MagicBallProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
}

void AHechi::StartChangeMapPattern()
{
	if ( BlackboardComp == nullptr ) return;
	
	BlackboardComp->SetValueAsBool("ChangeMap", true);
}

UAnimMontage* AHechi::PlayChangeMapMontage()
{	
	if ( ChangeMapMontage )
	{
		PlayAnimMontage(ChangeMapMontage);
		return ChangeMapMontage;
	}
	return nullptr;
}

void AHechi::StartDisappearCharacter()
{
	HandleCharacterTeleportFlag = true;
	
	// 만약 기존에 돌고 있던 타이머가 있다면 초기화 (안전장치)
	GetWorldTimerManager().ClearTimer(CharacterTeleportTimerHandle);
	
	// 3. 타이머 세팅: Duration 초 후에 EndDisappear 함수를 '한 번만' 실행합니다.
	GetWorldTimerManager().SetTimer(
		CharacterTeleportTimerHandle, 
		this, 
		&AHechi::EndDisappearCharacter, 
		5.5f, 
		false // 반복 여부 (false = 1회성)
	);
}

void AHechi::EndDisappearCharacter()
{
	HandleCharacterTeleportFlag = false;
}

void AHechi::PlayCharacterTeleportInEffect()
{
	if ( TargetCharacter && CharacterTeleportInEffect )
	{
		// 타깃 캐릭터의 현재 월드 위치 가져오기
		FVector SpawnLocation = TargetCharacter->GetActorLocation();
		FRotator SpawnRotation = TargetCharacter->GetActorRotation();
		
		// 캡슐 컴포넌트의 Half Height(절반 높이)만큼 Z축 아래로 내리기 (발바닥 위치)
		float HalfHeight = TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		SpawnLocation.Z -= HalfHeight;

		// 만약 발바닥보다 조금 더 아래로 내리거나 미세 조정하고 싶다면 오프셋 추가 가능
		// SpawnLocation.Z -= 20.0f; 
		
		// 월드에 파티클 스폰
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), 
			CharacterTeleportInEffect, 
			SpawnLocation, 
			SpawnRotation, 
			FVector(0.5f), 
			true
		);
	}
}

void AHechi::DisablePlayerInput()
{
	if (TargetCharacter)
	{
		// 1. 캐릭터의 이동 컴포넌트를 가져옵니다.
		UCharacterMovementComponent* MoveComp = TargetCharacter->GetCharacterMovement();
		if (MoveComp)
		{
			// 이동(WASD) 비활성화
			MoveComp->DisableMovement(); 
			
			TargetCharacter->JumpMaxCount = 0;   // 점프 가능 횟수를 0으로 만들어 점프 차단
		}
		
		// 2. 애니메이션 그 자리에서 멈추기 (고정)
		USkeletalMeshComponent* MeshComp = TargetCharacter->GetMesh();
		if (MeshComp)
		{
			MeshComp->bPauseAnims = true; // 애니메이션 일시정지
		}
	}
}

void AHechi::EnablePlayerInput()
{
	if (TargetCharacter)
	{
		UCharacterMovementComponent* MoveComp = TargetCharacter->GetCharacterMovement();
		if (MoveComp)
		{
			// 이동 모드를 다시 기본 걷기(Walking)로 복구
			MoveComp->SetMovementMode(MOVE_Walking);
            
			// 점프 가능 횟수 원래대로 복구 (기본값 1)
			TargetCharacter->JumpMaxCount = 1; 
		}
		// 2. 애니메이션 그 자리에서 멈추기 (고정)
		USkeletalMeshComponent* MeshComp = TargetCharacter->GetMesh();
		if (MeshComp)
		{
			MeshComp->bPauseAnims = false; // 애니메이션 재생 재개
		}
	}
}

void AHechi::PlayCharacterTeleportReadyEffect()
{
	if (TargetCharacter && CharacterTeleportReadyEffect)
	{
		// 1. 트레이스의 시작점(Start)과 끝점(End) 설정
		// 캐릭터의 현재 위치에서 시작해서, 아래쪽(Z축 마이너스)으로 1000유닛만큼 레이저를 쏩니다.
		FVector StartLocation = TargetCharacter->GetActorLocation();
		FVector EndLocation = StartLocation - FVector(0.0f, 0.0f, 1000.0f); 

		// 2. 라인 트레이스 충돌 매개변수 설정 (자기 자신이나 보스는 무시하도록 설정)
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);            // 보스 자신 무시
		QueryParams.AddIgnoredActor(TargetCharacter); // 타깃 캐릭터 무시 (자기 몸통에 부딪히는 것 방지)

		// 기본 이펙트 스폰 위치용 변수 (트레이스가 실패할 경우를 대비한 백업용)
		FVector SpawnLocation = StartLocation;
		float HalfHeight = TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		SpawnLocation.Z -= HalfHeight; 

		FRotator SpawnRotation = TargetCharacter->GetActorRotation();

		// 3. 아래 방향으로 라인 트레이스 시도 (정석적인 ECC_Visibility 채널 사용)
		bool bHit = GetWorld()->LineTraceSingleByChannel(
			HitResult, 
			StartLocation, 
			EndLocation, 
			ECC_Visibility, 
			QueryParams
		);

		if (bHit)
		{
			// 바닥에 충돌했다면, 실제 충돌한 지점(ImpactPoint)을 스폰 위치로 설정합니다.
			SpawnLocation = HitResult.ImpactPoint;

			// 디버그용: 필요하다면 살짝 바닥 틈새 파묻힘을 방지하기 위해 1~2유닛 정도 올릴 수 있습니다.
			SpawnLocation.Z += 2.0f;
		}

		// 4. 최종 결정된 바닥 위치에 나이아 가라 한번만 스폰
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			CharacterTeleportReadyEffect,
			SpawnLocation,
			SpawnRotation,
			FVector(1.0f), // 스케일
			true           // AutoDestroy
		);
	}
}

void AHechi::ChangePostProcessMaterialByIndex(int32 Index)
{
	// 1. 포스트 프로세스 볼륨이 정상적으로 캐싱되어 있는지 확인
	if (RandomChangeLevelPostProcessVolume == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Hechi: CachedPPVolume is null. Cannot change material."));
		return;
	}

	// 2. 입력된 인덱스가 배열 범위 내에 있는지 안전성 검사 (Nptr/Crash 방지)
	if (!RamdomPostProcessMaterialArray.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("Hechi: Invalid material index or null material at index: %d"), Index);
		return;
	}
	
	if ( RamdomPostProcessMaterialArray[Index] == nullptr )
	{
		RandomChangeLevelPostProcessVolume->Settings.WeightedBlendables.Array.Empty();
		return;
	}

	// 3. 가중치(Weight)를 가진 가중 블렌더블 구조체 생성
	FWeightedBlendable NewBlendable;
	NewBlendable.Weight = 1.0f;              // 효과 적용 강도 (0.0 ~ 1.0)
	NewBlendable.Object = RamdomPostProcessMaterialArray[Index]; // 선택한 머터리얼 인터페이스 설정

	// 4. 기존 볼륨에 걸려있던 포스트 프로세스 머터리얼 배열을 완전히 비우기
	RandomChangeLevelPostProcessVolume->Settings.WeightedBlendables.Array.Empty();

	// 5. 새로운 머터리얼 추가 적용
	RandomChangeLevelPostProcessVolume->Settings.WeightedBlendables.Array.Add(NewBlendable);

	// 6. (옵션) 혹시 볼륨이나 가중치가 꺼져있다면 확실하게 켜주기
	RandomChangeLevelPostProcessVolume->bEnabled = true;
	RandomChangeLevelPostProcessVolume->BlendWeight = 1.0f;

	UE_LOG(LogTemp, Log, TEXT("Hechi: Successfully changed PostProcess Material to Index [%d]: %s"), 
		Index, *RamdomPostProcessMaterialArray[Index]->GetName());
}

UAnimMontage* AHechi::PlayChangePostProcessMontage()
{
	if ( ChangePostProcessMontage )
	{
		PlayAnimMontage(ChangePostProcessMontage);
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.ChangePostProcessDelay); // 행동 딜레이 설정
		}
		
		return ChangePostProcessMontage;
	}
	return nullptr;
}

void AHechi::RandomChangePostProcess()
{
	// 머터리얼 배열이 비어있는지 확인
	if (RamdomPostProcessMaterialArray.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Hechi: PostProcessMaterialArray is empty!"));
		return;
	}

	// 머터리얼이 1개만 있는 경우 -> 중복 체크 없이 바로 적용
	if (RamdomPostProcessMaterialArray.Num() == 1)
	{
		CurrentPostProcessIndex = 0;
	}
	else
	{
		// 3. 이전 인덱스와 겹치지 않는 새로운 랜덤 인덱스 추출
		int32 NewIndex = CurrentPostProcessIndex;
		while (NewIndex == CurrentPostProcessIndex)
		{
			NewIndex = FMath::RandRange(0, RamdomPostProcessMaterialArray.Num() - 1);
		}
		CurrentPostProcessIndex = NewIndex;
	}
	
	ShakeCamera();// 카메라 흔들기 효과 호출
	
	FTimerHandle PostProcessDelayHandle;
    
	// [this, NextIndex = CurrentPostProcessIndex] 형태로 인덱스 값을 확실히 캡처해 둡니다.
	GetWorldTimerManager().SetTimer(
		PostProcessDelayHandle, 
		[this, NextIndex = CurrentPostProcessIndex]()
		{
			// 2초 뒤 이 중괄호 내부 코드가 실행됩니다.
			ChangePostProcessMaterialByIndex(NextIndex);
		}, 
		0.1f,  //지연 (하드코딩)
		false  // 반복 없이 단 한 번만 실행
	);
	
}

void AHechi::FirstChangePostProcess()
{
	UE_LOG(LogTemp, Error, TEXT("Hechi: First ChangePostProcess"));
	// 1. 포스트 프로세스 볼륨이 정상적으로 캐싱되어 있는지 확인
	if (FirstChangeLevelPostProcessVolume == nullptr)
	{
		return;
	}

	// 2. 머터리얼 배열이 비어있으면 볼륨을 초기화하고 종료
	if (FirstPostProcessMaterialArray.Num() == 0)
	{
		FirstChangeLevelPostProcessVolume->Settings.WeightedBlendables.Array.Empty();
		return;
	}

	// 시각적 연출을 위해 타이머 시작 전 카메라를 먼저 흔들어줍니다.
	ShakeCamera();

	// 0.1초 지연 처리를 위한 일회성 타이머 설정
	FTimerHandle FirstPPDelayHandle;
	GetWorldTimerManager().SetTimer(
		FirstPPDelayHandle,
		[this]()
		{
			// 3. 기존 볼륨에 걸려있던 블렌더블 배열을 완전히 비우기
			FirstChangeLevelPostProcessVolume->Settings.WeightedBlendables.Array.Empty();

			// 4. 배열에 있는 모든 유효한 머터리얼을 순회하며 가중치 1.0f로 추가
			for (UMaterialInterface* Material : FirstPostProcessMaterialArray)
			{
				if (Material != nullptr)
				{
					FWeightedBlendable NewBlendable;
					NewBlendable.Weight = 1.0f; // 효과 강도 최대
					NewBlendable.Object = Material;

					FirstChangeLevelPostProcessVolume->Settings.WeightedBlendables.Array.Add(NewBlendable);
				}
			}

			// 5. 볼륨 활성화 및 가중치 확인
			FirstChangeLevelPostProcessVolume->bEnabled = true;
			FirstChangeLevelPostProcessVolume->BlendWeight = 1.0f;
			
		},
		0.1f,  // 0.1초 지연
		false  // 반복 없음
	);
}

void AHechi::ShakeCamera()
{
	// 에셋이 블루프린트에서 정상적으로 등록되었는지 확인
	if (CameraShakeClass)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			// 지정한 에셋의 세팅(0.5초)대로 카메라를 흔듭니다.
			PC->ClientStartCameraShake(CameraShakeClass, 1.0f);
		}
	}
}

void AHechi::StartLoopPostProcessChange()
{
	UE_LOG(LogTemp, Log, TEXT("Hechi: Starting PostProcess Loop Change"));
	
	GetWorldTimerManager().SetTimer(
		PostProcessLoopHandle,
		this,
		&AHechi::RandomChangePostProcess,
		LoopChangePostProcess,
		true  // true = 무한 반복 실행
	);
}

void AHechi::EndBattleLog()
{
	Super::EndBattleLog();
	
	HechiLogData.Base = CommonBossLogData;
	
	UCSVLog::AddHechiLog( TEXT("Test"), HechiLogData);
	
	CommonBossLogData = FCommonBossLogData(); // 공통 로그 데이터 초기화
	HechiLogData = FHechiLogData(); // 보스별 로그 데이터 초기화
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