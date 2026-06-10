#include "EnemyBoss/MagicSwordMan/BossMagicSwordMan.h"

#include "CSVLog.h"
#include "MotionWarpingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SphereComponent.h"
#include "EnemyProjectile/BaseEnemyProjectile.h"
#include "Engine/OverlapResult.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"

ABossMagicSwordMan::ABossMagicSwordMan()
{
	PowerAttackCollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PowerAttackCollisionSphere"));
	PowerAttackCollisionSphere->SetupAttachment(GetRootComponent());
	PowerAttackCollisionSphere->SetSphereRadius( 540.f ); // 궁극기 공격 범위
	PowerAttackCollisionSphere->ShapeColor = FColor::Red;
	PowerAttackCollisionSphere->SetVisibility(false);
	PowerAttackCollisionSphere->SetHiddenInGame(false); 
}

void ABossMagicSwordMan::BeginPlay()
{
	Super::BeginPlay();
	
	CommonBossLogData.BossID = BossLogId; // 로그 데이터에 보스 ID 기록
	
	TArray<UCapsuleComponent*> CapsuleCollisionComps;
	GetComponents<UCapsuleComponent>(CapsuleCollisionComps);
	
	// 반복문 돌면서 태그 확인
	for (UCapsuleComponent* Capsule : CapsuleCollisionComps)
	{
		if (Capsule && Capsule->ComponentHasTag(TEXT("Weapon"))) // "Weapon" 태그를 가진 콜리전 스피어를 찾습니다.)))
		{
			WeaponCollision = Capsule;
			WeaponCollision->OnComponentBeginOverlap.AddDynamic(this, &ABossMagicSwordMan::OnBeginOverlapWeaponCollisionSphere);
			// 콜리전 끄기
			WeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break; // 찾았으니 루프 종료
		}
	}
	
	if ( PowerAttackCollisionSphere )
	{
		PowerAttackCollisionSphere->OnComponentBeginOverlap.AddDynamic(this, 
			&ABossMagicSwordMan::OnBeginOverlapPowerAttackCollisionSphere);
		PowerAttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossMagicSwordMan::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

float ABossMagicSwordMan::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	if ( bIsGuarding )
	{
		DamageWhileGuarding += DamageAmount;

		// 아직 리액션 대미지에 도달하지 않았으면 가드 몽타주 재생
		if ( DamageWhileGuarding < AttackStruct.MaxDamageToReaction && GuardHitMontage )
		{
			PlayAnimMontage(GuardHitMontage);
		}
		
		UEnemyLogManager::EnemyLog(EEnemyLogType::BlackKnight, 
			FString::Printf(TEXT("[소드맨] 가드 성공 | 가드로 막은 대미지: [%.f] | 가드중 받은 대미지 / 반격 임계치[%.f / %.f]"), 
				DamageAmount, DamageWhileGuarding, AttackStruct.MaxDamageToReaction));
		
		BossMagicSwordManLogData.TotalDamageGuarded += DamageAmount; // 로그 데이터에 가드로 막은 대미지 누적
		
		return 0.f; // 대미지 무효화
	}
	
	// 2. 가드 중이 아닐 때 실제 대미지 적용
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 3. 체력 비율 체크를 통한 2페이즈(특수 패턴) 진입 로직
	if (Health > 0.f && bIsSecondPhaseStarted == false)
	{
		// 현재 체력 비율 계산 (MaxHealth가 0이 아님을 전제)
		float CurrentHealthRatio = Health / MaxHealth;

		if (CurrentHealthRatio <= SecondPhaseHealthThreshold)
		{
			bIsSecondPhaseStarted = true; // 중복 실행 방지
			
			// 블랙보드 값 업데이트 (StartSecondPhase 함수 호출)
			StartSecondPhase(); 
		}
	}
	
	return ActualDamage;
}

void ABossMagicSwordMan::OnBeginOverlapWeaponCollisionSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                                             UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if ( AttackDamage == 0.f ) return; // 대미지가 0이면 피격 처리 안 함
	
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		// 어택 대미지 로그 
		UE_LOG(LogTemp, Warning, TEXT("Boss Attack Damage : %f"), AttackDamage);

		// 대미지 적용 ( 어택 대미지는 공격 전 가드 공격인지 아님 일반 공격인지에 따라 각각 함수에서 설정 )
		UGameplayStatics::ApplyDamage(OtherActor, AttackDamage, GetController(),
			this, UDamageType::StaticClass());

		if ( AttackType == EMagicSwordManAttackType::CloseJumpUpAttack || 
			AttackType == EMagicSwordManAttackType::DashJumpUpAttack )
		{
			// 플레이어 캐릭터 캐스팅
			ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
			if (HitCharacter)
			{
				HitCharacter->LaunchCharacter(FVector(0.f, 0.f, 900.f), false, true);
				bSuccessJumpUpAttack = true; // 띄우기 성공 여부를 true로 설정
			}
		}
		else if ( AttackType == EMagicSwordManAttackType::AirAttack )
		{
			// 공중 공격 피격 시 로직 추가
			ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
			if (HitCharacter)
			{
				//HitCharacter->LaunchCharacter(FVector(0.f, 0.f, 10.f), false, true);
			}
		}
		
		if ( AttackType == EMagicSwordManAttackType::CloseAttack )
		{
			BossMagicSwordManLogData.CloseAttackDamage += AttackDamage;
			CommonBossLogData.TotalDamageDealt += AttackDamage;
		}
		else if ( AttackType == EMagicSwordManAttackType::DashAttack )
		{
			BossMagicSwordManLogData.DashAttackDamage += AttackDamage;
			CommonBossLogData.TotalDamageDealt += AttackDamage;
		}
		else if ( AttackType == EMagicSwordManAttackType::CloseJumpUpAttack )
		{
			BossMagicSwordManLogData.CloseJumpUpAttackDamage += AttackDamage;
			CommonBossLogData.TotalDamageDealt += AttackDamage;
		}
		else if ( AttackType == EMagicSwordManAttackType::DashJumpUpAttack )
		{
			BossMagicSwordManLogData.DashJumpUpAttackDamage += AttackDamage;
			CommonBossLogData.TotalDamageDealt += AttackDamage;
		}
		else if ( AttackType == EMagicSwordManAttackType::GuardCounterAttack )
		{
			BossMagicSwordManLogData.GuardCounterAttackDamage += AttackDamage;
			CommonBossLogData.TotalDamageDealt += AttackDamage;
		} 
		else if ( AttackType == EMagicSwordManAttackType::AirAttack )
		{
			BossMagicSwordManLogData.AirAttackDamage += AttackDamage;
			CommonBossLogData.TotalDamageDealt += AttackDamage;
		}
		else if ( AttackType == EMagicSwordManAttackType::JumpAttack )
		{
			BossMagicSwordManLogData.JumpAttackDamage += AttackDamage;
			CommonBossLogData.TotalDamageDealt += AttackDamage;
		}
		
		// 다시 콜리전 끄기
		if ( WeaponCollision ) WeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AttackDamage = 0.f; // 대미지 초기화
	}
}

void ABossMagicSwordMan::AttackStart_WeaponCollision()
{
	if ( WeaponCollision ) WeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ABossMagicSwordMan::AttackEnd_WeaponCollision()
{
	if ( WeaponCollision ) WeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABossMagicSwordMan::SetAttackDamage(float DamageToApply)
{
	AttackDamage = DamageToApply;
}

UAnimMontage* ABossMagicSwordMan::StartCloseAttack()
{
	if (CloseAttackMontages.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, CloseAttackMontages.Num() - 1);
		
		if (CloseAttackMontages[RandomIndex])
		{
			PlayAnimMontage(CloseAttackMontages[RandomIndex]);
			AttackType = EMagicSwordManAttackType::CloseAttack;
			
			BossMagicSwordManLogData.CloseAttackCount++;
			
			if (BlackboardComp) BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.CloseAttackDelay);
			return CloseAttackMontages[RandomIndex];
		}
	}
	
	UE_LOG(LogTemp, Error, TEXT("array is EMPTY"));
	return nullptr;
}

UAnimMontage* ABossMagicSwordMan::StartDashAttack()
{	
	if (DashAttackMontages.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, DashAttackMontages.Num() - 1);
		
		if (DashAttackMontages[RandomIndex])
		{
			PlayAnimMontage(DashAttackMontages[RandomIndex]);
			AttackType = EMagicSwordManAttackType::DashAttack;
			
			BossMagicSwordManLogData.DashAttackCount++;
			
			if (BlackboardComp) BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.DashAttackDelay);
			return DashAttackMontages[RandomIndex];
		}
	}
	
	UE_LOG(LogTemp, Error, TEXT("array is EMPTY"));
	return nullptr;
}

UAnimMontage* ABossMagicSwordMan::StartCloseJumpUpAttack()
{
	if (CloseJumpUpAttackMontage)
	{
		PlayAnimMontage(CloseJumpUpAttackMontage);
		AttackType = EMagicSwordManAttackType::CloseJumpUpAttack;
		
		BossMagicSwordManLogData.CloseJumpUpAttackCount++;
		
		// 플라잉 모드로 전환
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
		bSuccessJumpUpAttack = false; // 공격 시작 시점에는 띄우기 성공 여부를 false로 초기화
		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsBool("AirAttack", false);
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.CloseJumpUpAttackDelay);
		}
		return CloseJumpUpAttackMontage;
	}
	return nullptr;
}

UAnimMontage* ABossMagicSwordMan::StartDashJumpUpAttack()
{
	if (DashJumpUpAttackMontage)
	{
		PlayAnimMontage(DashJumpUpAttackMontage);
		AttackType = EMagicSwordManAttackType::DashJumpUpAttack;
		
		BossMagicSwordManLogData.DashJumpUpAttackCount++;
		
		// 플라잉 모드로 전환
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
		bSuccessJumpUpAttack = false; // 공격 시작 시점에는 띄우기 성공 여부를 false로 초기화
		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsBool("AirAttack", false);
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.DashJumpUpAttackDelay);
		}
		return DashJumpUpAttackMontage;
	}
	return nullptr;
}

void ABossMagicSwordMan::JumpUpAttackCheck()
{
	UE_LOG( LogTemp, Warning, TEXT("JumpUpAttackCheck called. bSuccessJumpUpAttack: %s"), bSuccessJumpUpAttack ? TEXT("true") : TEXT("false") );
	
	UEnemyLogManager::EnemyLog(EEnemyLogType::MagicSwordMan, 
		FString::Printf(TEXT("[소드맨] 띄우기 성공 여부: [%s]"), bSuccessJumpUpAttack ? TEXT("성공") : TEXT("실패")));
	
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	if ( bSuccessJumpUpAttack == true )
	{
		BossMagicSwordManLogData.JumpUpAttackSuccessCount++;
		
		// 띄우기 성공 시 공중 공격 패턴으로 전환
		BlackboardComp->SetValueAsBool("AirAttack", true);
		
		BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.AirAttackDelay);
	}
}

UAnimMontage* ABossMagicSwordMan::StartAirAttack()
{
	if (AirAttackMontage)
	{
		PlayAnimMontage(AirAttackMontage);
		AttackType = EMagicSwordManAttackType::AirAttack;
		
		BossGravityScaleBeforeAirAttack = GetCharacterMovement()->GravityScale;
		// 보스랑 타깃 캐릭터의 중력 설정
		GetCharacterMovement()->GravityScale = 0.78f;

		// 타깃 캐릭터의 중력 스케일 조절
		if (TargetCharacter)
		{
			// TargetCharacter가 ACharacter 타입인지 확인 후 캐스팅
			if (ACharacter* CastedTarget = Cast<ACharacter>(TargetCharacter))
			{
				TargetCharacterGravityScaleBeforeAirAttack = CastedTarget->GetCharacterMovement()->GravityScale;
				
				CastedTarget->GetCharacterMovement()->GravityScale = 0.75f;
				
			}
		}
		return AirAttackMontage;
	}
	return nullptr;
}

void ABossMagicSwordMan::AirAttackEnd()
{
	// 공중 공격이 끝났을 때 중력 스케일을 원래대로 복구
	GetCharacterMovement()->GravityScale = BossGravityScaleBeforeAirAttack;	
	if (TargetCharacter)
	{
		if (ACharacter* CastedTarget = Cast<ACharacter>(TargetCharacter))
		{
			CastedTarget->GetCharacterMovement()->GravityScale = TargetCharacterGravityScaleBeforeAirAttack;
		}
	}
}

UAnimMontage* ABossMagicSwordMan::StartJumpAttack()
{
	if (JumpAttackMontages.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, JumpAttackMontages.Num() - 1);
		
		if (JumpAttackMontages[RandomIndex])
		{
			PlayAnimMontage(JumpAttackMontages[RandomIndex]);
			AttackType = EMagicSwordManAttackType::JumpAttack;
			
			BossMagicSwordManLogData.JumpAttackCount++;
			
			if (BlackboardComp) BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.JumpAttackDelay);
			return JumpAttackMontages[RandomIndex];
		}
	}
	
	UE_LOG(LogTemp, Error, TEXT("array is EMPTY"));
	return nullptr;
}

void ABossMagicSwordMan::JumpStart()
{
	LaunchCharacter( FVector(0.f, 0.f, 800.f), false, true);
}

UAnimMontage* ABossMagicSwordMan::StartGuardReactionAttack()
{
	if (GuardReactionMontages.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, GuardReactionMontages.Num() - 1);
		
		if (GuardReactionMontages[RandomIndex])
		{
			BossMagicSwordManLogData.GuardCounterAttackCount++;
			
			PlayAnimMontage(GuardReactionMontages[RandomIndex]);
			AttackType = EMagicSwordManAttackType::GuardCounterAttack;
			return GuardReactionMontages[RandomIndex];
		}
	}
	return nullptr;
}

UAnimMontage* ABossMagicSwordMan::StartPowerAttack()
{
	if (PowerAttackMontage)
	{
		PlayAnimMontage(PowerAttackMontage);
		bIsPowerAttackHit = false; // 공격 시작 시점에는 궁극기 공격이 적중했는지 여부를 false로 초기화
		if (BlackboardComp) BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.PowerAttackDelay);
		return PowerAttackMontage;
	}
	return nullptr;
}

void ABossMagicSwordMan::OnBeginOverlapPowerAttackCollisionSphere(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		HandlePowerAttackDamage(OtherActor);
		
		bIsPowerAttackHit = true; // 궁극기 공격이 적중했음을 표시하는 플래그를 true로 설정
		
		UEnemyLogManager::EnemyLog(EEnemyLogType::MagicSwordMan, FString::Printf(TEXT("[소드맨] 궁극기 공격 적중 ")));
		
		// 타겟 정지 및 입력 차단 로직 시작 
		if (ACharacter* HitCharacter = Cast<ACharacter>(OtherActor))
		{
			//  플레이어 컨트롤러를 가져와서 입력을 비활성화
			if (APlayerController* PlayerController = Cast<APlayerController>(HitCharacter->GetController()))
			{
				HitCharacter->DisableInput(PlayerController);
			}

			//무브먼트 컴포넌트를 가져와서 움직임을 정지
			if (UCharacterMovementComponent* MovementComp = HitCharacter->GetCharacterMovement())
			{
				MovementComp->StopMovementImmediately(); // 현재 속도와 가속도를 0으로 만듦
				MovementComp->SetMovementMode(EMovementMode::MOVE_None); // 이동 모드를 끄면 중력 영향을 받지 않고 그 자리에 멈춤
			}
			// 애니메이션도 멈추기 
			if (USkeletalMeshComponent* StopMesh = HitCharacter->GetMesh())
			{
				StopMesh->bPauseAnims = true;
			}
		}
		
		// 궁극기 공격이 한 번 적중하면 콜리전 비활성화 
		PowerAttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossMagicSwordMan::HandlePowerAttackDamage(AActor* OtherActor)
{
	if (!OtherActor) return;

	// 1. 반응성을 위해 즉시 1회 대미지 적용 (선택 사항)
	UGameplayStatics::ApplyDamage(OtherActor, AttackDamage, GetController(), this, UDamageType::StaticClass());

	UEnemyLogManager::EnemyLog(EEnemyLogType::MagicSwordMan, FString::Printf(TEXT("[소드맨] 궁극기 대미지 적용 | 대미지: [%.f]"), AttackDamage));
	
	BossMagicSwordManLogData.PowerAttackDamage += AttackDamage;
	CommonBossLogData.TotalDamageDealt += AttackDamage;
	
	// 2. 파라미터(OtherActor)를 전달하기 위해 델리게이트 생성
	FTimerDelegate TimerCallback;
	TimerCallback.BindUObject(this, &ABossMagicSwordMan::OnPowerAttackTimerTick, OtherActor);

	// 3. 0.3초 간격으로 반복(true) 실행되는 타이머 설정
	GetWorld()->GetTimerManager().SetTimer(PowerAttackTimerHandle, TimerCallback, 0.05f, true);
}

void ABossMagicSwordMan::StartPowerAttackCollision()
{
	if ( PowerAttackCollisionSphere )
	{
		PowerAttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void ABossMagicSwordMan::EndPowerAttackCollision()
{
	if ( PowerAttackCollisionSphere )
	{
		PowerAttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	// 공격이 끝나면 타이머를 해제하여 대미지를 멈춤
	GetWorld()->GetTimerManager().ClearTimer(PowerAttackTimerHandle);
}

void ABossMagicSwordMan::OnPowerAttackTimerTick(AActor* TargetActor)
{
	// 타겟이 유효한지(파괴되지 않았는지) 확인
	if (IsValid(TargetActor))
	{
		// 주기적 대미지 적용
		UGameplayStatics::ApplyDamage(TargetActor, AttackStruct.PowerAttackTickDamage, 
			GetController(), this, UDamageType::StaticClass());
		// 대미지 로그
		UE_LOG(LogTemp, Warning, TEXT("Boss Power Attack Tick Damage : %f"), AttackStruct.PowerAttackTickDamage);
	}
	else
	{
		// 타겟이 유효하지 않으면 타이머 정지
		GetWorld()->GetTimerManager().ClearTimer(PowerAttackTimerHandle);
	}
}

void ABossMagicSwordMan::FinishPowerAttack()
{
	if ( bIsPowerAttackHit )
	{
		UGameplayStatics::ApplyDamage(TargetCharacter, AttackStruct.PowerAttackFinishDamage, 
			GetController(), this, UDamageType::StaticClass());
		
		UEnemyLogManager::EnemyLog(EEnemyLogType::MagicSwordMan, FString::Printf(TEXT("[소드맨] 궁극기 마무리 대미지 적용 | 대미지: [%.f]"), AttackStruct.PowerAttackFinishDamage));
		
		// 피니쉬 대미지 로그
		UE_LOG(LogTemp, Warning, TEXT("Boss Power Attack Finish Damage : %f"), AttackStruct.PowerAttackFinishDamage);
		
		BossMagicSwordManLogData.PowerAttackDamage += AttackStruct.PowerAttackFinishDamage;
		CommonBossLogData.TotalDamageDealt += AttackStruct.PowerAttackFinishDamage;
		
		// 타겟 움직임 및 입력 복구 
		if (ACharacter* HitCharacter = Cast<ACharacter>(TargetCharacter))
		{
			// 이동 모드 복구 
			if (UCharacterMovementComponent* MovementComp = HitCharacter->GetCharacterMovement())
			{
				MovementComp->SetMovementMode(EMovementMode::MOVE_Walking);
			}

			// 플레이어 입력 다시 활성화
			if (APlayerController* PlayerController = Cast<APlayerController>(HitCharacter->GetController()))
			{
				HitCharacter->EnableInput(PlayerController);
			}
			
			// 애니메이션 재생 다시 활성화
			if (USkeletalMeshComponent* StopMesh = HitCharacter->GetMesh())
			{
				StopMesh->bPauseAnims = false;
			}
		}
	}
}

UAnimMontage* ABossMagicSwordMan::StartBladeWaveAttack()
{
	if (BladeWaveAttackMontage)
	{
		BossMagicSwordManLogData.BladeWaveAttackCount++;
		
		PlayAnimMontage(BladeWaveAttackMontage);
		if (BlackboardComp) BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.BladeWaveAttackDelay);
		return BladeWaveAttackMontage;
	}
	return nullptr;
}

void ABossMagicSwordMan::StartBladeWave()
{
	if (BladeWaveProjectileClass)
	{
		// 정면으로 발사체 스폰 . 약간 앞에서만
		FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 150.f;
		FRotator SpawnRotation = GetActorRotation();
		GetWorld()->SpawnActor<ABaseEnemyProjectile>(BladeWaveProjectileClass, SpawnLocation, SpawnRotation);
	}
}

UAnimMontage* ABossMagicSwordMan::StartDashBack()
{
	if ( DashBackMontage )
	{
		BossMagicSwordManLogData.BackDashCount++;
		
		PlayAnimMontage(DashBackMontage);
		
		if (BlackboardComp) BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.BackDashDelay);
		
		return DashBackMontage;
	}
	return nullptr;
}

void ABossMagicSwordMan::StartSecondPhase()
{
	if ( BlackboardComp == nullptr ) return;
	
	UEnemyLogManager::EnemyLog(EEnemyLogType::SkeletonMage, TEXT("[매직소드맨] 2페이즈 패턴"));
	
	BlackboardComp->SetValueAsBool("SecondPhase", true);
}

UAnimMontage* ABossMagicSwordMan::PlaySecondPhaseMontage()
{
	if ( SecondPhaseMontage )
	{
		PlayAnimMontage(SecondPhaseMontage);
		return SecondPhaseMontage;
	}
	return nullptr;
}

void ABossMagicSwordMan::StartHover()
{
	if (!HoverLocationPoint)
	{
		UE_LOG(LogTemp, Warning, TEXT("[소드맨] HoverLocationPoint가 설정되지 않았습니다!"));
		return;
	}
	
	// 1. 목표 위치 계산 (기준점 + 높이 700)
	FVector BaseLocation = HoverLocationPoint->GetActorLocation();
	FVector GoalLocation = BaseLocation + FVector(0.f, 0.f, 700.f);

	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
	
	// 2. 모션 워핑 타겟 업데이트 (회전은 현재 보스 회전 유지 또는 타겟 방향)
	MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
		FName("WarpTarget"), // 몽타주 노티파이에서 사용할 이름
		GoalLocation,
		GetActorRotation()
	);
}

void ABossMagicSwordMan::SetGravityCharacter()
{
	if (!GetCharacterMovement()) return;
	
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	
	// 캡슐 콜리전 다시 켜기
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// 중력값 저장
	DefaultGravityScaleBeforeHover = GetCharacterMovement()->GravityScale;

	GetCharacterMovement()->GravityScale = 0.05f;
}

void ABossMagicSwordMan::RushToPlayer()
{
	if (!GetCharacterMovement()) return;
	// 중력 스케일 원상 복구
	GetCharacterMovement()->GravityScale = DefaultGravityScaleBeforeHover;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
}

void ABossMagicSwordMan::RushStrike()
{
	float DamageRadius = 400.f;     // 공격 범위 반지름
	// 참고하신 로직의 힘 설정 (필요 시 헤더파일의 AttackStruct 등에 정의하여 사용하세요)
	float PushForce = 600.f;       // 수평으로 밀어내는 힘
	float PushUpwardForce = 200.f;  // 위로 띄우는 힘 (참고 코드보다 조금 높게 설정하여 타격감 강화)

	// 1. 발바닥 위치 계산
	float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector FootLocation = GetActorLocation() - FVector(0.f, 0.f, CapsuleHalfHeight);

	// 2. 범위 내 모든 액터 감지 (OverlapMulti)
	TArray<FOverlapResult> OverlapResults;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(DamageRadius);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); 

	bool bHit = GetWorld()->OverlapMultiByChannel(OverlapResults, FootLocation, FQuat::Identity, ECC_Pawn, SphereShape, QueryParams);

	if (bHit)
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* HitActor = Result.GetActor();
			if (HitActor && HitActor->ActorHasTag(FName("Player")))
			{
				// 3. 대미지 적용
				UGameplayStatics::ApplyDamage(HitActor, AttackStruct.RushStrikeAttackDamage, 
					GetController(), this, UDamageType::StaticClass());

				// 4. 플레이어 밀치기 (Radial Launch)
				ACharacter* PlayerChar = Cast<ACharacter>(HitActor);
				if (PlayerChar)
				{
					// 폭발 중심(FootLocation)에서 플레이어로 향하는 수평 방향 계산
					FVector PushDirection = PlayerChar->GetActorLocation() - FootLocation;
					PushDirection.Z = 0.f; // 수평 방향 추출
					PushDirection.Normalize();

					// 수평 힘 + 수직 힘 합산
					const FVector LaunchVelocity = (PushDirection * PushForce) + FVector(0.f, 0.f, PushUpwardForce);

					// 플레이어 캐릭터 밀어냄 (기존 속도 무시)
					PlayerChar->LaunchCharacter(LaunchVelocity, true, true);
				}

				BossMagicSwordManLogData.RushStrikeAttackDamage += AttackStruct.RushStrikeAttackDamage;
				CommonBossLogData.TotalDamageDealt += AttackStruct.RushStrikeAttackDamage;
				
				UEnemyLogManager::EnemyLog(EEnemyLogType::MagicSwordMan, 
					FString::Printf(TEXT("[소드맨] RushStrike 적중 | 대미지: [%.f] | 밀치기 적용"),  AttackStruct.RushStrikeAttackDamage));
			}
		}
	}
	if ( bDebugMode == true )DrawDebugSphere(GetWorld(), FootLocation, DamageRadius, 12, FColor::Red, false, 2.0f);

	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
}

void ABossMagicSwordMan::EndBattleLog()
{
	Super::EndBattleLog();
	
	BossMagicSwordManLogData.Base = CommonBossLogData;
	
	UCSVLog::AddMagicSwordManLog( TEXT("Test"), BossMagicSwordManLogData);
	
	CommonBossLogData = FCommonBossLogData(); // 공통 로그 데이터 초기화
	BossMagicSwordManLogData = FBossMagicSwordManLogData(); // 보스별 로그 데이터 초기화
}

#if WITH_EDITOR
void ABossMagicSwordMan::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABossMagicSwordMan, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( PowerAttackCollisionSphere ) PowerAttackCollisionSphere->SetVisibility(true);
		}
		else
		{
			if ( PowerAttackCollisionSphere ) PowerAttackCollisionSphere->SetVisibility(false);
		}
	}
}
#endif