#include "EnemyBoss/BlackKnight/BossBlackKnight.h"

#include "NiagaraFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

// 깃 허브 업데이트 확인
ABossBlackKnight::ABossBlackKnight()
{
	RushAttackSphere = CreateDefaultSubobject<USphereComponent>(TEXT("RushAttackSphere"));
	RushAttackSphere ->SetupAttachment(GetRootComponent());
	RushAttackSphere->ShapeColor = FColor::Blue;
	RushAttackSphere->SetVisibility(false);
	RushAttackSphere->SetHiddenInGame(false);
	// 콜리전 끄기
	RushAttackSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// 최대 걷기 속도 100
	GetCharacterMovement()->MaxWalkSpeed = 100.0f;
}

void ABossBlackKnight::BeginPlay()
{
	Super::BeginPlay();

	if ( RushAttackSphere )
	{
		RushAttackSphere->OnComponentBeginOverlap.AddDynamic(this, &ABossBlackKnight::OnBeginOverlapRushAttackSphere);
	}

	TArray<USphereComponent*> SphereComps;
	GetComponents<USphereComponent>(SphereComps);
	
	// 반복문 돌면서 태그 확인
	for (USphereComponent* Sphere : SphereComps)
	{
		if (Sphere && Sphere->ComponentHasTag(TEXT("Axe")))
		{
			AxeCollisionSphere = Sphere;
			AxeCollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABossBlackKnight::OnBeginOverlapAxeCollisionSphere);
			// 콜리전 끄기
			AxeCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break; // 찾았으니 루프 종료
		}
	}
}

void ABossBlackKnight::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

float ABossBlackKnight::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	if ( bIsGuarding )
	{
		DamageWhileGuarding += DamageAmount;

		// 아직 리액션 대미지에 도달하지 않았으면 가드 몽타주 재생
		if ( DamageWhileGuarding < AttackStruct.MaxDamageToReaction && GuardMontage )
		{
			PlayAnimMontage(GuardMontage);
		}
		// 가드 했다고 로그매니저 출력 
		UEnemyLogManager::EnemyLog(EEnemyLogType::BlackKnight, 
			FString::Printf(TEXT("[블랙나이트] 가드 성공 | 가드로 막은 대미지: [%.f] | 가드중 받은 대미지 / 반격 임계치[%.f / %.f]"), 
				DamageAmount, DamageWhileGuarding, AttackStruct.MaxDamageToReaction));
		
		return 0.f; // 대미지 무효화
	}
	
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ABossBlackKnight::OnBeginOverlapAxeCollisionSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack Sphere Overlapped with Player!"));

		// 대미지 적용 ( 어택 대미지는 공격 전 가드 공격인지 아님 일반 공격인지에 따라 각각 함수에서 설정 )
		UGameplayStatics::ApplyDamage(OtherActor, AttackDamage, GetController(),
			this, UDamageType::StaticClass());

		if ( AttackDamage == AttackStruct.GuardAttackDamage )
		{
			UEnemyLogManager::EnemyLog(EEnemyLogType::BlackKnight, FString::Printf(TEXT("[블랙나이트] 가드 반격 적중 | 대미지: [%.f]"), AttackDamage));
		}
		else if ( AttackDamage == AttackStruct.NormalAttackDamage )
		{
			UEnemyLogManager::EnemyLog(EEnemyLogType::BlackKnight, FString::Printf(TEXT("[블랙나이트] 일반 공격 적중 | 대미지: [%.f]"), AttackDamage));
		}
		else if ( AttackDamage == AttackStruct.ChargeAttackDamage )
		{
			UEnemyLogManager::EnemyLog(EEnemyLogType::BlackKnight, FString::Printf(TEXT("[블랙나이트] 차지 공격 적중 | 대미지: [%.f]"), AttackDamage));
		}
		
		// 다시 콜리전 끄기
		if ( AxeCollisionSphere ) AxeCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossBlackKnight::AttackStart_AxeCollisionSphere()
{
	if ( AxeCollisionSphere ) AxeCollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ABossBlackKnight::AttackEnd_AxeCollisionSphere()
{
	if ( AxeCollisionSphere ) AxeCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABossBlackKnight::OnBeginOverlapRushAttackSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 오버랩된 액터가 플레이어인지 확인합니다.
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rush Attack Sphere Overlapped with Player!"));

		// 대미지 적용
		UGameplayStatics::ApplyDamage(OtherActor, AttackStruct.RushAttackDamage, GetController(),
			this, UDamageType::StaticClass());

		// --- 플레이어 밀치기 효과 시작 ---
		ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
		if (PlayerCharacter)
		{
			// 1. 밀어낼 방향 계산 (보스 -> 플레이어)
			FVector PushDirection = PlayerCharacter->GetActorLocation() - GetActorLocation();
			PushDirection.Z = 0; // 수평 방향으로만 밀도록 Z값을 0으로 설정
			PushDirection.Normalize();

			// 2. 밀어낼 속도 계산 (방향 * 힘 + 위로 띄우는 힘)
			const FVector LaunchVelocity = PushDirection * PushForce + FVector(0.f, 0.f, PushUpwardForce);

			// 3. 플레이어 캐릭터를 밀어냄
			// bXYOverride와 bZOverride를 true로 설정하여 현재 속도를 무시하고 새로운 속도를 적용합니다.
			PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);
		}
		
		// 돌진 공격 적중 여부 로그매니저 로그 출력
		UEnemyLogManager::EnemyLog(EEnemyLogType::BlackKnight, FString::Printf(TEXT("[블랙나이트] 돌진 공격 적중 | 대미지: [%.f]"), AttackStruct.RushAttackDamage));
		
		// 다시 콜리전 끄기
		RushAttackSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossBlackKnight::RushAttack()
{
	if ( RushAttackMontage )
	{
		PlayAnimMontage(RushAttackMontage);
		
		bFocusPlayerAfterAttack = false; // 돌진 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp == nullptr ) return;
	
		BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.RushAttackDelay); // 행동 딜레이 설정
	}
}

void ABossBlackKnight::StartRush()
{
	// 콜리전 켜기: 돌진 중 플레이어와 충돌 감지
	RushAttackSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ABossBlackKnight::EndRush()
{
	RushAttackSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABossBlackKnight::GuardAttack()
{
	if ( GuardAttackMontage )
	{
		PlayAnimMontage(GuardAttackMontage);
		// 공격 데미지를 가드 공격 데미지로 설정
		AttackDamage = AttackStruct.GuardAttackDamage;
		
		bFocusPlayerAfterAttack = false; // 가드 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.GuardAttackDelay); // 행동 딜레이 설정
		}
	}
}

void ABossBlackKnight::StartWaveAttack()
{
	bIsHitZap = false; // 번개 공격 초기화
	
    const int32 TotalWaves = 4;
    const float TimeBetweenWaves = 0.4f;
    const float BaseDistance = 150.0f; 
    const float DistanceStep = 200.0f;
	
    UWorld* World = GetWorld();
    if (!World) return;

    FVector StartLocation = (AxeCollisionSphere) ? AxeCollisionSphere->GetComponentLocation() : GetActorLocation();
    FVector FixedForward = GetActorForwardVector();

    TWeakObjectPtr<ABossBlackKnight> WeakThis(this);

    for (int32 i = 0; i < TotalWaves; i++)
    {
        // 웨이브 로직을 별도의 람다로 정의
        auto ExecuteWave = [WeakThis, i, BaseDistance, DistanceStep, World, StartLocation, FixedForward]()
        {
            if (!WeakThis.IsValid()) return;

            float CurrentDistance = BaseDistance + (i * DistanceStep);
            const TArray<float> Angles = { -30.0f, 0.0f, 30.0f };

            for (float Angle : Angles)
            {
                FVector Direction = FixedForward.RotateAngleAxis(Angle, FVector::UpVector);
                FVector TargetLocation = StartLocation + (Direction * CurrentDistance);

                FHitResult HitResult;
                FVector TraceStart = TargetLocation + FVector(0.f, 0.f, 1000.f);
                FVector TraceEnd = TargetLocation - FVector(0.f, 0.f, 1000.f);

            	FCollisionQueryParams TraceParams;
            	TraceParams.AddIgnoredActor(WeakThis.Get());
            	TraceParams.AddIgnoredActor(WeakThis->TargetCharacter);
            	
                FVector FinalLocation = TargetLocation;
                if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
                {
                    FinalLocation = HitResult.Location;
                }

            	if ( WeakThis->bDebugMode == true ) DrawDebugSphere(World, FinalLocation, 80.0f, 16, FColor::Purple, false, 2.0f);
            	
            	// --- 플레이어 감지 로직 추가 ---
            	TArray<FOverlapResult> OverlapResults;
            	FCollisionShape SphereShape = FCollisionShape::MakeSphere(80.0f); // 디버그 스피어와 동일한 크기
            	FCollisionQueryParams QueryParams;
            	QueryParams.AddIgnoredActor(WeakThis.Get()); // 보스 자신은 제외

            	// ECC_Pawn 채널을 사용하여 주변 액터 검출
            	if (World->OverlapMultiByChannel(OverlapResults, FinalLocation, FQuat::Identity, ECC_Pawn, SphereShape, QueryParams))
            	{
            		for (const FOverlapResult& Result : OverlapResults)
            		{
            			AActor* OverlappedActor = Result.GetActor();
            			if (OverlappedActor && OverlappedActor->ActorHasTag(TEXT("Player")))
            			{
            				if ( WeakThis->bIsHitZap == false )
            				{
            					// 여기서 플레이어에게 데미지를 주거나 로직 수행!
            					UE_LOG(LogTemp, Warning, TEXT("Player Hit At Zap"));
            					WeakThis->bIsHitZap = true;
            					
            					UGameplayStatics::ApplyDamage(
									OverlappedActor,            // 데미지를 받을 대상
									WeakThis->AttackStruct.ZapDamage,    // 데미지 수치
									WeakThis->GetController(),  // 가해자 컨트롤러
									WeakThis.Get(),             // 가해자 액터 (보스)
									UDamageType::StaticClass()  // 데미지 타입 클래스
								);
            					
            					UEnemyLogManager::EnemyLog(EEnemyLogType::BlackKnight, 
            						FString::Printf(TEXT("[블랙나이트] 차지 어택 번개 적중 | 대미지: [%.f]"), WeakThis->AttackStruct.ZapDamage));
            				}
            			}
            		}
            	}
            	// 가드 어택 이펙트 있으면 재생
            	if (WeakThis->ZapEffect)
            	{
            		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
						World, 
						WeakThis->ZapEffect, 
						FinalLocation, 
						FixedForward.Rotation(), // 보스가 바라보는 정면 방향으로 이펙트 회전 설정
						FVector(5.f)
					);
            	}
            }
        };
        // 첫 번째 웨이브(i=0)는 즉시 실행, 나머지는 타이머 예약
        if (i == 0)
        {
            ExecuteWave();
        }
        else
        {
            FTimerHandle WaveTimerHandle;
            FTimerDelegate WaveDelegate;
            WaveDelegate.BindLambda(ExecuteWave);
            World->GetTimerManager().SetTimer(WaveTimerHandle, WaveDelegate, i * TimeBetweenWaves, false);
        }
    }
}

void ABossBlackKnight::NormalAttack()
{
	if ( NormalAttackMontage )
	{
		PlayAnimMontage(NormalAttackMontage);

		AttackDamage = AttackStruct.NormalAttackDamage;
		
		bFocusPlayerAfterAttack = false; // 차지 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.NormalAttackDelay); // 행동 딜레이 설정
		}
	}
}

void ABossBlackKnight::ChargeAttack()
{
	if ( ChargeAttackMontage )
	{
		PlayAnimMontage(ChargeAttackMontage);

		AttackDamage = AttackStruct.ChargeAttackDamage;
		
		bFocusPlayerAfterAttack = false; // 차지 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.ChargeAttackDelay); // 행동 딜레이 설정
		}
	}
}

void ABossBlackKnight::SpawnRandomZapEffect()
{
	// [설정] 헤더 변수 대신 함수 내부에서 값 정의
    const int32  SpawnCount      = 7;      // 떨어질 번개 횟수
    const float  SpawnRadius     = 800.0f;  // 보스 기준 생성 반경
    const float  SpawnInterval   = 0.3f;   // 번개 생성 간격 (초)
    const float  DamageRadius    = 50.0f;  // 대미지 판정 범위
    const float  EffectScale     = 3.0f;    // 이펙트 크기 배율

    UWorld* World = GetWorld();
    if (!World) return;

    // 타이머 람다에서 안전하게 접근하기 위해 Weak Pointer 사용
    TWeakObjectPtr<ABossBlackKnight> WeakThis(this);

    for (int32 i = 0; i < SpawnCount; i++)
    {
        // 순차적으로 터지도록 딜레이 설정
        float Delay = i * SpawnInterval;

        FTimerHandle ZapTimerHandle;
        
        // 람다 함수 내부에 로직 구현
        World->GetTimerManager().SetTimer(ZapTimerHandle, [WeakThis, SpawnRadius, DamageRadius, EffectScale]()
        {
            if (!WeakThis.IsValid()) return;
            UWorld* WorldContext = WeakThis->GetWorld();
            if (!WorldContext) return;

            // 1. 랜덤 위치 계산 (보스 주변 원형 범위)
            FVector BossLocation = WeakThis->GetActorLocation();
            FVector RandomDir = FMath::VRand();
            RandomDir.Z = 0.0f; // 수평 방향만 고려
            RandomDir.Normalize();

            // 최소 150cm ~ 최대 SpawnRadius 사이 랜덤 거리
            float RandomDist = FMath::FRandRange(150.0f, SpawnRadius); 
            FVector TargetLocation = BossLocation + (RandomDir * RandomDist);

            // 2. 바닥 위치 보정 (공중에 뜨지 않게 LineTrace)
            FHitResult HitResult;
            FVector TraceStart = TargetLocation + FVector(0.f, 0.f, 500.f);
            FVector TraceEnd = TargetLocation - FVector(0.f, 0.f, 500.f);

        	FCollisionQueryParams TraceParams;
        	TraceParams.AddIgnoredActor(WeakThis.Get());
			TraceParams.AddIgnoredActor(WeakThis->TargetCharacter);
        	
            if (WorldContext->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
            {
                TargetLocation = HitResult.Location;
            }

            // 3. 이펙트 재생 
            if (WeakThis->ZapEffect)
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                    WorldContext,
                    WeakThis->ZapEffect,
                    TargetLocation,
                    FRotator::ZeroRotator,
                    FVector(EffectScale)
                );
            }

            // (디버그용) 위치 표시
            if ( WeakThis->bDebugMode == true )  DrawDebugSphere(WorldContext, TargetLocation, DamageRadius, 12, FColor::Yellow, false, 1.0f);

        	/*
            // 4. 범위 대미지 판정
            TArray<FOverlapResult> OverlapResults;
            FCollisionShape SphereShape = FCollisionShape::MakeSphere(DamageRadius);
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(WeakThis.Get()); // 보스 자신 제외

            if (WorldContext->OverlapMultiByChannel(OverlapResults, TargetLocation, FQuat::Identity, ECC_Pawn, SphereShape, QueryParams))
            {
                for (const FOverlapResult& Result : OverlapResults)
                {
                    AActor* OverlappedActor = Result.GetActor();
                    // 플레이어 태그 확인
                    if (OverlappedActor && OverlappedActor->ActorHasTag(TEXT("Player")))
                    {
                        // ZapDamage 변수 재사용 (StartWaveAttack에 있는 변수)
                        UGameplayStatics::ApplyDamage(
                            OverlappedActor,
                            WeakThis->ZapDamage, 
                            WeakThis->GetController(),
                            WeakThis.Get(),
                            UDamageType::StaticClass()
                        );
                        
                        // 로그 출력 (필요시 주석 해제)
                        // UE_LOG(LogTemp, Warning, TEXT("Random Zap Hit Player!"));
                    }
                }
            }
            */

        }, Delay, false);
    }
}

void ABossBlackKnight::ZapAttack()
{
	if ( ZapAttackMontage )
	{
		PlayAnimMontage(ZapAttackMontage);

		AttackDamage = AttackStruct.ZapDamage;
		
		bFocusPlayerAfterAttack = false; // 잽 공격 중에는 포커스 비활성화
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.ZapAttackDelay); // 행동 딜레이 설정 
		}
	}
}

void ABossBlackKnight::SetZapTargetLocation()
{
	if (TargetCharacter == nullptr) return;

	// 1. 타겟의 현재 위치를 가져옵니다.
	const FVector CharacterLocation = TargetCharacter->GetActorLocation();
	FVector GoalLocation = CharacterLocation; // 기본 위치

	// 2. 바닥을 찾기 위해 라인 트레이스를 수행합니다. (참고 코드 로직 적용)
	FHitResult HitResult;
	const FVector StartTrace = CharacterLocation;
	const FVector EndTrace = CharacterLocation - FVector(0.f, 0.f, 1000.f); // 아래로 1000 유닛
	
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(TargetCharacter);
	TraceParams.AddIgnoredActor(this);

	// 라인 트레이스로 바닥 위치를 찾습니다.
	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartTrace, EndTrace, ECC_WorldStatic, TraceParams))
	{
		// 충돌 지점을 목표 위치로 설정합니다.
		GoalLocation = HitResult.Location;
	}

	// 3. 최종 계산된 바닥 위치 저장
	ZapTargetLocation = GoalLocation;
}

void ABossBlackKnight::SpawnZapAttackEffect()
{
	// 저장된 위치 사용
	FVector SpawnLocation = ZapTargetLocation;

	if (ZapEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ZapEffect, SpawnLocation, FRotator::ZeroRotator, FVector(5.f));
	}

	// 디버그 스피어 그리기
	if ( bDebugMode == true ) DrawDebugSphere(GetWorld(), SpawnLocation, 150.0f, 12, FColor::Blue, false, 2.0f);

	// 대미지 판정 범위 설정
	float DamageRadius = 150.0f; 

	// 플레이어 감지 및 대미지 적용
	TArray<FOverlapResult> OverlapResults;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(DamageRadius);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); // 보스 자신 제외

	if (GetWorld()->OverlapMultiByChannel(OverlapResults, SpawnLocation, FQuat::Identity, ECC_Pawn, SphereShape, QueryParams))
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* OverlappedActor = Result.GetActor();
			if (OverlappedActor && OverlappedActor->ActorHasTag(TEXT("Player")))
			{
				UGameplayStatics::ApplyDamage(
					OverlappedActor,
					AttackStruct.ZapAttackDamage,         // 잽 공격 대미지
					GetController(),
					this,
					UDamageType::StaticClass()
				);
				UE_LOG(LogTemp, Warning, TEXT("Zap Attack Hit Player!"));
				
				UEnemyLogManager::EnemyLog(EEnemyLogType::BlackKnight, FString::Printf(TEXT("[블랙나이트] 번개 소환 적중 | 대미지: [%.f]"), AttackStruct.ZapAttackDamage));
			}
		}
	}
}

#if WITH_EDITOR
void ABossBlackKnight::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABossBlackKnight, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( RushAttackSphere ) RushAttackSphere->SetVisibility(true);
		}
		else
		{
			if ( RushAttackSphere ) RushAttackSphere->SetVisibility(false);
		}
	}
}
#endif
