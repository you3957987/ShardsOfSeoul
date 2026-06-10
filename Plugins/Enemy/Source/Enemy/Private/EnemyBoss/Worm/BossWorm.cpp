#include "EnemyBoss/Worm/BossWorm.h"

#include "Kismet/GameplayStatics.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemyBoss/Worm/BossWormProjectile.h"
#include "EnemyProjectile/BaseStreamProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsEngine/RadialForceComponent.h"

ABossWorm::ABossWorm()
{
	// 최대 걷기 속도 제로로 해서 움직이지 않도록 설정
	GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	
	UnBurrowAttackCollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("UnBurrowAttackCollisionSphere"));
	UnBurrowAttackCollisionSphere->SetupAttachment(GetRootComponent());
	UnBurrowAttackCollisionSphere->ShapeColor = FColor::Blue;
	UnBurrowAttackCollisionSphere->SetVisibility(false);
	UnBurrowAttackCollisionSphere->SetHiddenInGame(false);
	// 콜리전 오버랩만 감지하도록 설정
	UnBurrowAttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	RangedAttackPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RangedAttackPoint"));
	RangedAttackPoint->SetupAttachment(RootComponent); // 루트 컴포넌트
	
	SuctionRadialForceComp = CreateDefaultSubobject<URadialForceComponent>(TEXT("SuctionRadialForceComp"));
	SuctionRadialForceComp->SetupAttachment(RootComponent);
	
	SuctionRadialForceComp->Radius = 10000.f;        // 영향을 미칠 범위
	SuctionRadialForceComp->ForceStrength = AttackStruct.SuctionForce; // 빨아들이는 힘 (음수)
	SuctionRadialForceComp->bImpulseVelChange = false ; // 질량에 따른 힘 조정 여부 (false면 질량이 클수록 덜 빨려감)
	SuctionRadialForceComp->bAutoActivate = false;
}

void ABossWorm::BeginPlay()
{
	Super::BeginPlay();
	
	TArray<USphereComponent*> SphereComps;
	GetComponents<USphereComponent>(SphereComps);
	
	// 반복문 돌면서 태그 확인
	for (USphereComponent* Sphere : SphereComps)
	{
		if (Sphere && Sphere->ComponentHasTag(TEXT("Attack"))) // "Attack" 태그가 있는 SphereComponent를 찾습니다.
		{
			AttackCollisionSphere = Sphere;
			AttackCollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABossWorm::OnBeginOverlapAttackCollisionSphere);
			// 콜리전 끄기
			AttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break; // 찾았으니 루프 종료
		}
	}
	
	// FireBreath 태그 있는 씬 컴포넌트 찾아서 FireBreathPoint에 할당
	TArray<USceneComponent*> SceneComps;
	GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* SceneComp : SceneComps)
	{
		if (SceneComp && SceneComp->ComponentHasTag(TEXT("FireBreath")))
		{
			FireBreathPoint = SceneComp;
			break; // 찾았으니 루프 종료
		}
	}
}

void ABossWorm::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	HandleSuction(DeltaTime);
}

void ABossWorm::Die()
{
	Super::Die();
	
	// 타이머 정지 (발사 중단)
	GetWorldTimerManager().ClearTimer(FireBreathTimerHandle);
}

void ABossWorm::OnBeginOverlapAttackCollisionSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack Sphere Overlapped with Player!"));

		// 대미지 적용
		UGameplayStatics::ApplyDamage(OtherActor, AttackDamage, GetController(),
			this, UDamageType::StaticClass());

		if ( AttackDamage == AttackStruct.LungeAttackDamage || AttackDamage == AttackStruct.SuctionAttackDamage )
		{
			ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
			
			const float PushForce = 2000.0f;       // 밀어내는 힘
			const float PushUpwardForce = 400.0f;  // 띄우는 힘

			// 밀어낼 방향 계산 (보스 -> 플레이어)
			FVector PushDirection = PlayerCharacter->GetActorLocation() - GetActorLocation();
			PushDirection.Z = 0; 
			PushDirection.Normalize();

			// 밀어낼 속도 계산
			const FVector LaunchVelocity = PushDirection * PushForce + FVector(0.f, 0.f, PushUpwardForce);

			// 플레이어 캐릭터를 밀어냄
			PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);
		}
		
		if ( AttackDamage == AttackStruct.NormalAttackDamage )
		{
			UEnemyLogManager::EnemyLog(EEnemyLogType::Worm, FString::Printf(TEXT("[웜] 일반 공격 | 대미지[%.f]"), AttackDamage));
		}
		else if ( AttackDamage == AttackStruct.LungeAttackDamage )
		{
			UEnemyLogManager::EnemyLog(EEnemyLogType::Worm, FString::Printf(TEXT("[웜] 런지 공격 | 대미지[%.f]"), AttackDamage));
		}
		else if ( AttackDamage ==  AttackStruct.SuctionAttackDamage )
		{
			UEnemyLogManager::EnemyLog(EEnemyLogType::Worm, FString::Printf(TEXT("[웜] 석션 공격 | 대미지[%.f]"), AttackDamage));
		}
			
		
		// 다시 콜리전 끄기
		if ( AttackCollisionSphere ) AttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossWorm::AttackStart_AttackCollisionSphere()
{
	if ( AttackCollisionSphere ) AttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ABossWorm::AttackEnd_AttackCollisionSphere()
{
	if ( AttackCollisionSphere ) AttackCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABossWorm::FinishBurrow()
{
	// 캡슐 콜리전 비활성화
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bIsBurrowing = true; // 땅 아래로 들어가는 몽타주가 끝나는 시점에 땅파는 상태를 true로 변경
	
	// 딜레이 설정
	if ( BlackboardComp )
	{
		BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.BurrowDelay); // 행동 딜레이 설정
	}
	
	// 타겟 위치 세팅하는 함수 == BurrowDelay 에서 2초 정도 남았을 때 호출되어 타겟 위치 세팅
	float TargetSetDelay = AttackStruct.BurrowDelay - 0.1f;
	
	// 만약 BurrowDelay가 2초보다 짧다면 즉시 호출하거나 아주 짧은 딜레이로 설정
	if (TargetSetDelay < 0.0f)
	{
		TargetSetDelay = 0.1f;
	}

	GetWorld()->GetTimerManager().SetTimer(
		UnBurrowTargetTimerHandle, 
		this, 
		&ABossWorm::SetUnBurrowTargetLocation, 
		TargetSetDelay, 
		false
	);
}

void ABossWorm::FinishUnburrow()
{
	// 캡슐 콜리전 활성화
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	bIsBurrowing = false; // 땅에서 나오는 몽타주가 끝나는 시점에 땅파는 상태를 false로 변경
	
	// 공격 딜레이 설정
	if ( BlackboardComp )
	{
		BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.UnBurrowAttackDelay); // 행동 딜레이 설정
	}
}

void ABossWorm::NormalAttack()
{
	if ( NormalAttackMontage )
	{
		PlayAnimMontage(NormalAttackMontage);

		if ( bIsInSuctionAttackArea)
		{
			AttackDamage = AttackStruct.SuctionAttackDamage  ; // 석션 공격 범위 안에 있으면 석션 공격 대미지로 설정
			bIsInSuctionAttackArea = false; // 공격 후에는 범위 플래그 초기화
		}
		else AttackDamage = AttackStruct.NormalAttackDamage; // 공격 대미지 설정
		
		bFocusPlayerAfterAttack = false; // 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.NormalAttackDelay); // 행동 딜레이 설정
		}
	}
}

void ABossWorm::LungeAttack()
{
	if ( LungeAttackMontage )
	{
		PlayAnimMontage(LungeAttackMontage);

		AttackDamage = AttackStruct.LungeAttackDamage; // 공격 대미지 설정
		
		bFocusPlayerAfterAttack = false;  
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.LungeAttackDelay); // 행동 딜레이 설정
		}
	}
}

void ABossWorm::Burrow()
{
	if ( BurrowMontage )
	{
		PlayAnimMontage(BurrowMontage);
		BlackboardComp->SetValueAsBool( TEXT("bIsBurrowing"), true);
		
		ChangePlayerLockOn(false); // 플레이어 락온 해제
	}
}

void ABossWorm::SetUnBurrowTargetLocation()
{
	if (TargetCharacter)
	{
		// 1. 타겟의 현재 위치 (캡슐 중심)
		UnBurrowTargetLocation = TargetCharacter->GetActorLocation();

		// 타겟의 캡슐 컴포넌트 가져오기 (타겟 발바닥 위치 계산용)
		UCapsuleComponent* TargetCapsule = TargetCharacter->GetCapsuleComponent();
		float TargetHalfHeight = 0.f;
		if (TargetCapsule)
		{
			TargetHalfHeight = TargetCapsule->GetScaledCapsuleHalfHeight();
		}

		// 내(보스) 캡슐 컴포넌트 가져오기 (내 중심 위치 보정용)
		UCapsuleComponent* MyCapsule = GetCapsuleComponent();
		float MyHalfHeight = 0.f;
		if (MyCapsule)
		{
			MyHalfHeight = MyCapsule->GetScaledCapsuleHalfHeight();
		}

		// 공식: 타겟 중심 - 타겟 키(발바닥으로 내림) + 내 키(내 중심으로 올림)
		UnBurrowTargetLocation.Z = (UnBurrowTargetLocation.Z - TargetHalfHeight) + MyHalfHeight;

		// 2. 먼저 계산된 위치로 이동
		SetActorLocation(UnBurrowTargetLocation);
		
		// 3. 회전 설정 (보스 현재 위치에서 타겟을 바라보는 회전 계산)
		// 주의: 이미 위치를 옮겼으므로 단순히 TargetCharacter를 바라보면 됨
		const FRotator LookAtRotation =
			UKismetMathLibrary::FindLookAtRotation(UnBurrowTargetLocation, TargetCharacter->GetActorLocation());

		// 수평으로만 바라보도록 Yaw 값만 사용합니다.
		const FRotator SpawnRotation = FRotator(0.f, LookAtRotation.Yaw, 0.f);

		SetActorRotation(SpawnRotation);
	}
	else
	{
		// 타겟이 없으면 제자리
		UnBurrowTargetLocation = GetActorLocation();
	}
}

void ABossWorm::Unburrow()
{
	if ( UnburrowMontage )
	{
		PlayAnimMontage(UnburrowMontage);
		BlackboardComp->SetValueAsBool( TEXT("bIsBurrowing"), false);
	}
	
	// --- 여기서부터 즉시 오버랩 검사 및 밀치기 로직 ---
	if (UnBurrowAttackCollisionSphere)
	{
		TArray<AActor*> OverlappingActors;
		UnBurrowAttackCollisionSphere->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());

		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor != this && Actor->ActorHasTag(FName("Player")))
			{
				ACharacter* PlayerCharacter = Cast<ACharacter>(Actor);
				if (PlayerCharacter)
				{
					const float PushForce = 2000.0f;       // 밀어내는 힘
					const float PushUpwardForce = 400.0f;  // 띄우는 힘

					// 밀어낼 방향 계산 (보스 -> 플레이어)
					FVector PushDirection = PlayerCharacter->GetActorLocation() - GetActorLocation();
					PushDirection.Z = 0; 
					PushDirection.Normalize();

					// 밀어낼 속도 계산
					const FVector LaunchVelocity = PushDirection * PushForce + FVector(0.f, 0.f, PushUpwardForce);

					// 플레이어 캐릭터를 밀어냄
					PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);
					
					// 대미지 적용
					UGameplayStatics::ApplyDamage(PlayerCharacter, AttackStruct.UnBurrowAttackDamage, GetController(),
						this, UDamageType::StaticClass());
					
					UEnemyLogManager::EnemyLog(EEnemyLogType::Worm, FString::Printf(TEXT("[웜] 언버로우 공격 | 대미지[%.f]"), AttackStruct.UnBurrowAttackDamage));
					
					// 로그 출력 (필요시 주석 해제)
					UE_LOG(LogTemp, Warning, TEXT("Player Hit At Unburrow Attack!"));
				}
			}
		}
	}
}

void ABossWorm::RangedAttack()
{
	if ( RangedAttackMontage )
	{
		PlayAnimMontage( RangedAttackMontage);
		
		bFocusPlayerAfterAttack = false; // 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.RangedAttackDelay); // 행동 딜레이 설정
		}
	}
}

void ABossWorm::ShootRangedProjectile()
{
    if (RangedProjectileClass && RangedAttackPoint && TargetCharacter)
    {
        const FVector SpawnLocation = RangedAttackPoint->GetComponentLocation();

        // 타겟 위치 보정 (발 밑)
        FVector TargetLocation = TargetCharacter->GetActorLocation();
        const UCapsuleComponent* TargetCapsule = TargetCharacter->GetCapsuleComponent();
        if (TargetCapsule)
        {
            TargetLocation.Z -= TargetCapsule->GetScaledCapsuleHalfHeight();
        }

        FVector LaunchVelocity;

        // [수정] 생성자는 4개의 인자만(World, Start, End, Speed) 받습니다.
        UGameplayStatics::FSuggestProjectileVelocityParameters Params(GetWorld(), 
        	SpawnLocation, TargetLocation, 2000.0f);

        // 나머지 옵션은 멤버 변수로 직접 설정
        Params.CollisionRadius = 10.0f;
        Params.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;
        Params.bFavorHighArc = false; // 여기서 곡사/직사 여부 설정

        // 함수 실행 (Reference로 LaunchVelocity를 받아옴)
        bool bHaveSolution = UGameplayStatics::SuggestProjectileVelocity(Params, LaunchVelocity);

        // 해결책을 못 찾았을 경우
        if (!bHaveSolution)
        {
            FVector Direction = (TargetLocation - SpawnLocation);
            float Distance = Direction.Size();
            float FallbackSpeed = FMath::Clamp(Distance * 1.5f, 1000.0f, 3000.0f);

            // [수정] 동일하게 Fallback 파라미터도 생성자 인자 개수 맞춤
            UGameplayStatics::FSuggestProjectileVelocityParameters FallbackParams(GetWorld(), 
            	SpawnLocation, TargetLocation, FallbackSpeed);
            FallbackParams.CollisionRadius = 10.0f;
            FallbackParams.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;
            FallbackParams.bFavorHighArc = true; // 멀면 고각 발사 시도

            if(!UGameplayStatics::SuggestProjectileVelocity(FallbackParams, LaunchVelocity))
            {
                 // 그래도 실패하면 그냥 타겟 방향으로 쏘기
                 Direction.Normalize();
                 Direction.Z += 0.5f;
                 LaunchVelocity = Direction.GetSafeNormal() * FallbackSpeed;
            }
        }

        // 회전값은 속도 벡터의 방향으로 설정
        const FRotator SpawnRotation = LaunchVelocity.Rotation();

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this;

        UWorld* World = GetWorld();
        if (World)
        {
            ABaseEnemyProjectile* SpawnedProjectile =
                World->SpawnActor<ABaseEnemyProjectile>(RangedProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);

            if (SpawnedProjectile)
            {
                UProjectileMovementComponent* ProjMoveComp =
                    SpawnedProjectile->FindComponentByClass<UProjectileMovementComponent>();
                if (ProjMoveComp)
                {
                    ProjMoveComp->Velocity = LaunchVelocity;
                    ProjMoveComp->InitialSpeed = LaunchVelocity.Size();
                    ProjMoveComp->MaxSpeed = LaunchVelocity.Size();
                }
            }
        }
    }
}

void ABossWorm::LinearFireBreathStart()
{
	if ( LinearFireBreathMontage )
	{
		PlayAnimMontage(LinearFireBreathMontage);
		
		bFocusPlayerAfterAttack = false; // 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.LinearFireBreathDelay); // 행동 딜레이 설정
		}
	}
}

void ABossWorm::FanFireBreathStart()
{
	if ( FanFireBreathMontage )
	{
		PlayAnimMontage(FanFireBreathMontage);
		
		bFocusPlayerAfterAttack = false; // 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.FanFireBreathDelay); // 행동 딜레이 설정
		}
	}
}

void ABossWorm::StartFireBreath()
{
	// 이미 발사 중이라면 중복 실행 방지
	if (GetWorldTimerManager().IsTimerActive(FireBreathTimerHandle)) return;
	
	// 0.05초 간격으로 SpawnFireBreathProjectile 함수 반복 호출 (Loop = true)
	GetWorldTimerManager().SetTimer(FireBreathTimerHandle, this, 
		&ABossWorm::SpawnFireBreathProjectile, FireBreathInterval, true);
}

void ABossWorm::EndFireBreath()
{
	// 타이머 정지 (발사 중단)
	GetWorldTimerManager().ClearTimer(FireBreathTimerHandle);
}

void ABossWorm::SpawnFireBreathProjectile()
{
	if (FireBreathPoint && StreamProjectileClass)
	{
		// FireBreathPoint는 머리 뼈에 붙어 있으므로, 애니메이션에 따라 위치와 회전이 실시간으로 변함
		const FVector SpawnLocation = FireBreathPoint->GetComponentLocation();
		
		// 소켓의 회전을 가져오되, Pitch(위아래)와 Roll(좌우기울기)은 0으로 만들어 수평으로 발사
		FRotator SpawnRotation = FireBreathPoint->GetComponentRotation();
		SpawnRotation.Pitch = 0.0f; // 바닥으로 박히지 않게 수평 유지
		SpawnRotation.Roll = 0.0f;  // 기울어지지 않게

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		GetWorld()->SpawnActor<ABaseStreamProjectile>(StreamProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	}
}

void ABossWorm::HandleSuction(float DeltaTime)
{
	if ( bIsSuctioning == true && TargetCharacter )
	{
		
		// 만약 보스랑 타깃 캐릭터 사이 거리가가까워지면 흡입력 끄기
		float DistanceToTarget = FVector::Dist(GetActorLocation(), TargetCharacter->GetActorLocation());
		if (DistanceToTarget < 400.f)
		{
			bIsInSuctionAttackArea = true; // 흡입 공격 범위 안에 들어왔음을 표시
			
			AttackDamage = AttackStruct.SuctionAttackDamage; // 공격 대미지 설정
			
			BlackboardComp->SetValueAsBool("bIsInSuctionAttackArea", true); // 블랙보드에도 반영
			
			float ElapsedTime = GetWorld()->GetTimeSeconds() - SuctionStartTime;
			
			UEnemyLogManager::EnemyLog(EEnemyLogType::Worm, 
				FString::Printf(TEXT("[웜] 석션 성공 | 경과 시간: %.1f초"), ElapsedTime));
			
			EndSuction();
			return;
		}
		
		// 방향 벡터 계산 (플레이어 -> 보스)
		FVector ToBoss = GetActorLocation() - TargetCharacter->GetActorLocation();
		ToBoss.Normalize();

		// [수정] 실제 속도(Velocity) 대신 플레이어가 바라보는 방향(ForwardVector)을 사용
		// Velocity는 흡입력 때문에 뒤로 가려 해도 보스 쪽으로 끌려가면 양수가 나옵니다.
		// 따라서 캐릭터가 등을 돌리고 있는지(ForwardVector)로 판단하는 게 정확합니다.
		FVector PlayerForward = TargetCharacter->GetActorForwardVector();

		// 내적(Dot Product) 계산
		// 플레이어가 보스를 보고 있으면(공격 중) 양수, 등을 돌리고 있으면(도망 중) 음수
		float Resistance = FVector::DotProduct(ToBoss, PlayerForward);

		bool bIsMovingInput = false;
		if (ACharacter* Char = Cast<ACharacter>(TargetCharacter))
		{
			if (UCharacterMovementComponent* Movement = Char->GetCharacterMovement())
			{
				// 작은 오차를 고려해 1.0f 이상이면 입력이 있는 것으로 간주
				bIsMovingInput = Movement->GetCurrentAcceleration().SizeSquared() > 1.0f;
			}
		}
	 	
		float FinalForce = AttackStruct.SuctionForce;

		if (Resistance < 0.0f && bIsMovingInput) // 플레이어가 보스를 등지고 + 도망가려고 할 때
		{
			// 저항이 음수일 때 힘을 줄여서 플레이어가 도망갈 수 있도록 허용
			FinalForce *= 0.8f;
		}
		
		//UE_LOG(LogTemp, Warning, TEXT("Resistance Value: %f, FinalForce Value: %f"), Resistance, FinalForce);
	 	
		// 매 프레임 힘의 크기를 갱신하여 적용
		SuctionRadialForceComp->ForceStrength = FinalForce;
	} 
}

void ABossWorm::SuctionStartMontagePlay()
{
	if ( SuctionMontage )
	{
		PlayAnimMontage(SuctionMontage);
		
		bFocusPlayerAfterAttack = false; // 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.SuctionAttackDelay); // 행동 딜레이 설정
		}
	}
}

void ABossWorm::StartSuction()
{	
	if (SuctionRadialForceComp)
	{
		SuctionRadialForceComp->Activate(); // 빨아들이는 힘 활성화
		
		bIsSuctioning = true;
		
		SuctionStartTime = GetWorld()->GetTimeSeconds(); // 시작 시간 기록
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsBool("bIsInSuctionAttackArea", false); 
		}
		bIsSuctioning = true; // 빨아들이는 중임을 표시
	}
}

void ABossWorm::EndSuction()
{
	if (SuctionRadialForceComp)
	{
		SuctionRadialForceComp->Deactivate(); // 빨아들이는 힘 비활성화
		bIsSuctioning = false;
	}
}

#if WITH_EDITOR
void ABossWorm::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABossWorm, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( UnBurrowAttackCollisionSphere ) UnBurrowAttackCollisionSphere->SetVisibility(true);
		}
		else
		{
			if ( UnBurrowAttackCollisionSphere ) UnBurrowAttackCollisionSphere->SetVisibility(false);
		}
	}
}
#endif
