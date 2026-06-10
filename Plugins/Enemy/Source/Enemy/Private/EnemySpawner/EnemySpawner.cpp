#include "EnemySpawner/EnemySpawner.h"

#include "BaseEnemy.h"
#include "CSVLog.h"
#include "EnemyLogManager.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/Progressbar.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemyHUD/EnemyHealthBarWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true; 
	
	RootCollisionSphere = CreateDefaultSubobject<UCapsuleComponent>(TEXT("RootCollisionSphere"));
	RootComponent = RootCollisionSphere;

	SpawnerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpawnerMesh"));
	SpawnerMesh->SetupAttachment(RootComponent);
	SpawnerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpawnLocation = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnLocation"));
	SpawnLocation->SetupAttachment(RootComponent);

	PlayerDetectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PlayerDetectSphere"));
	PlayerDetectSphere->SetupAttachment(RootComponent);
	PlayerDetectSphere->SetHiddenInGame(false);
	PlayerDetectSphere->SetSphereRadius(1000.f); // 기본값
	
	// 체력 바 위젯 컴포넌트 생성 및 설정
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::World);
	
	Tags.Add(FName("Enemy")); // 적 캐릭터 태그 추가 -> 이걸 이용해서 프로젝트에서 플러그인 에너미 접근. 매우 중요!!!!
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if ( PlayerDetectSphere )
	{
		PlayerDetectSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemySpawner::OnBeginOverlapPlayerDetectSphere);
		PlayerDetectSphere->OnComponentEndOverlap.AddDynamic(this, &AEnemySpawner::OnEndOverlapPlayerDetectSphere);
	}
	
	if ( HealthBarWidget ) // 체력바 위젯에서 프로그레스바 설정
	{
		UEnemyHealthBarWidget* HealthBar = Cast<UEnemyHealthBarWidget>(HealthBarWidget->GetUserWidgetObject());
		if ( HealthBar )
		{
			HealthBar->HealthProgressBar->SetPercent(Health / MaxHealth);
		}
	}
}

void AEnemySpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PollInit();

	UpdateHealthBarWidget(DeltaTime); // 체력 바 위젯 업데이트 -> 항상 캐릭터 쪽으로 바라보도록
	
	if ( bTargetInRange == false ) return;
	
	// 1. 쿨타임 감소
	if (CurrentSpawnCooldown > 0.0f)
	{
		CurrentSpawnCooldown -= DeltaTime;
	}

	// 2. 쿨타임이 끝났고, 플레이어가 유효하다면 시야 체크 시도
	if (CurrentSpawnCooldown <= 0.0f && TargetCharacter)
	{
		// 시야 체크 (Line Trace)
		if (CanSeePlayer())
		{
			SpawnEnemy();
            
			// 스폰 후 쿨타임 재설정
			CurrentSpawnCooldown = SpawnDelay;
		}
		else
		{
			// 시야가 안 보이면?
			// 매 프레임 검사하면 너무 무거우니 0.2초 정도 있다가 다시 검사하게 설정 (최적화 팁)
			CurrentSpawnCooldown = 0.2f; 
		}
	}
}

void AEnemySpawner::PollInit()
{
	if ( bTargetInitalize == false )
	{
		TargetCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0); // 월드에서 첫 번째 플레이어 캐릭터를 가져옵니다.
		if ( TargetCharacter ) // 캐릭터가 유효한지 확인합니다.
		{
			bTargetInitalize = true; // 캐릭터가 유효하면 초기화 플래그를 true로 설정합니다.
		}
	}
}

float AEnemySpawner::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	float DamageToApply = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	UE_LOG(LogTemp, Warning, TEXT("Enemy Take Damage : %f"), DamageToApply);
	
	EnemyLogData.TotalDamageReceived += DamageToApply; // 로그 데이터에 받은 대미지 누적
	
	if ( DamageToApply > 0.f )
	{
		Health -= DamageToApply;
		if ( Health <= 0.f )
		{
			UEnemyLogManager::EnemyLog(EEnemyLogType::Spawner, 
			FString::Printf(TEXT("적 [스포너]가 [%.f] 대미지 받아 사망"), DamageToApply));
			
			Die();
			return DamageToApply;
		}
	}
	
	SetHitOverlay(); // 히트 오버레이 설정
	
	UEnemyLogManager::EnemyLog(EEnemyLogType::Spawner, 
		FString::Printf(TEXT("적 [스포너]가 [%.f] 대미지 받음 (%.f / %.f)"), DamageToApply, MaxHealth,Health));
	
	return DamageToApply;
}

// 플레이어가 감지 범위에 들어왔을 때
void AEnemySpawner::OnBeginOverlapPlayerDetectSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어 태그 확인
	if ( OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")) )
	{
		if (!TargetCharacter)
		{
			TargetCharacter = Cast<ACharacter>(OtherActor);
		}
		
		bTargetInRange = true; // 타겟이 감지 범위 안에 있다고 설정
		
		// 들어오자마자 바로 쏠 수 있게 쿨타임 초기화 (원한다면)
		if ( bActivateSpawner == false )CurrentSpawnCooldown = 0.0f; 
		
		bActivateSpawner = true;
		
	}
}

// 플레이어가 감지 범위에서 나갔을 때
void AEnemySpawner::OnEndOverlapPlayerDetectSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if ( OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")) )
	{
		bTargetInRange = false; // 타겟이 감지 범위 밖에 있다고 설정
	}
}

void AEnemySpawner::UpdateHealthBarWidget(float DeltaTime)
{
	if (HealthBarWidget && HealthBarWidget->IsVisible())
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (PC && PC->PlayerCameraManager)
		{
			const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			const FVector WidgetLocation = HealthBarWidget->GetComponentLocation();

			// 위젯에서 카메라를 바라보는 방향의 회전값을 계산합니다.
			const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(WidgetLocation, CameraLocation);

			// 위젯이 항상 수평을 유지하도록 Yaw 값만 사용하여 회전을 설정합니다.
			HealthBarWidget->SetWorldRotation(FRotator(0.f, LookAtRotation.Yaw, 0.f));
		}

		UEnemyHealthBarWidget* HealthBar = Cast<UEnemyHealthBarWidget>(HealthBarWidget->GetUserWidgetObject());
		if ( HealthBar )
		{
			HealthBar->HealthProgressBar->SetPercent(Health / MaxHealth);
		}
	}
}

bool AEnemySpawner::CanSeePlayer() const
{
	// 타겟이 없으면 보이지 않는 것으로 간주
	if (!TargetCharacter) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	// 시작점: 스포너 위치 (바닥보단 살짝 위)
	FVector Start = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	// 끝점: 타겟 플레이어 위치
	FVector End = TargetCharacter->GetActorLocation();

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);            // 스포너 자신은 무시
	CollisionParams.AddIgnoredActor(TargetCharacter); // 플레이어 자체는 무시 (플레이어 '앞'을 가리는 벽만 체크)

	// Visibility 채널(시야)로 라인 트레이스 수행
	bool bHit = World->LineTraceSingleByChannel(
	   HitResult,
	   Start,
	   End,
	   ECollisionChannel::ECC_Visibility,
	   CollisionParams
	);

	// 무언가(벽)에 맞았다면 시야가 막힌 것이므로 false, 안 맞았으면 트인 것이므로 true
	return !bHit;
}

void AEnemySpawner::SpawnEnemy()
{
	// 0. 시야 체크: 플레이어가 보이지 않으면 스폰하지 않음
	if (CanSeePlayer() == false) return;
	
	if (SpawningEnemyClasses.Num() > 0 && SpawnLocation)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			// 배열에서 랜덤하게 하나 선택
			int32 RandomIndex = FMath::RandRange(0, SpawningEnemyClasses.Num() - 1);
			TSubclassOf<ABaseEnemy> SelectedEnemyClass = SpawningEnemyClasses[RandomIndex];

			// 선택된 클래스가 유효한지 확인
			if (!SelectedEnemyClass) return;

			// 기본 스폰 위치
			FVector OriginLocation = SpawnLocation->GetComponentLocation();
			FVector FinalSpawnLocation = OriginLocation;
			FRotator SpawnRotation = SpawnLocation->GetComponentRotation();

			// 1. 내비게이션 시스템을 통해 랜덤 위치 찾기
			UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			float RandomRadius = 100.0f; 

			if (NavSystem)
			{
				FNavLocation RandomPoint;
				// OriginLocation 기준 RandomRadius 반경 내의 랜덤한 이동 가능 지점 찾기
				if (NavSystem->GetRandomReachablePointInRadius(OriginLocation, RandomRadius, RandomPoint))
				{
					FinalSpawnLocation = RandomPoint.Location;
				}
				
				// 2. 캡슐 컴포넌트의 높이만큼 Z축 보정 (땅에 파묻히지 않게)
				float SafeZOffset = 0.f;
				ACharacter* EnemyCDO = Cast<ACharacter>(SelectedEnemyClass->GetDefaultObject());
				if (EnemyCDO && EnemyCDO->GetCapsuleComponent())
				{
					SafeZOffset = EnemyCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				}

				// 위치 보정 적용
				FinalSpawnLocation.Z += SafeZOffset;

				// 3. 스폰 파라미터 설정 및 스폰
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				ABaseEnemy* SpawnedEnemy = World->SpawnActor<ABaseEnemy>(
					SelectedEnemyClass,
					FinalSpawnLocation,
					SpawnRotation,
					SpawnParams
				);
				
				if (SpawnedEnemy)
				{
					SpawnedEnemy->EnemyLogID = FString::Printf(TEXT("%s_SpawnedEnemy_%d"), *EnemyLogID,SpawnEnemyId);
				}
				
				// 스켈레톤 메시 이름 가져오기
				FString MeshName = TEXT("Unknown");
				if (SpawnedEnemy && SpawnedEnemy->GetMesh() && SpawnedEnemy->GetMesh()->GetSkeletalMeshAsset())
				{
					MeshName = SpawnedEnemy->GetMesh()->GetSkeletalMeshAsset()->GetName();
				}
				
				if ( SpawnSound )
				{
					//2d 사운드로 재생
					UGameplayStatics::PlaySound2D(this, SpawnSound);
				}

				EnemyLogData.SpawnCount += 1; // 로그 데이터에 스폰 횟수 누적
				SpawnEnemyId++;
				
				// 로그에 메시 이름 추가 출력
				UEnemyLogManager::EnemyLog(EEnemyLogType::Spawner, FString::Printf(TEXT("적 [스포너]가 [%s] 스폰"), *MeshName));
			}
		}
	}
}

void AEnemySpawner::Die()
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(false);
	}
	
	if (DeadSound)
	{
		UGameplayStatics::PlaySound2D(this, DeadSound);
	}
	
	if ( DeathEffectCascade )
	{
		// 현재 위치 + (앞방향 * 앞뒤 오프셋) + (윗방향 * 위아래 오프셋)
		const FVector EffectSpawnLocation = GetActorLocation();
		
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DeathEffectCascade,
			EffectSpawnLocation, GetActorRotation(), FVector(1.f));
	}

	// 액터 삭제 전에 아이템 드롭 함수 호출
	DropItemsAfterDead();
	
	// 최종적으로 CSV 로그 파일에 추가
	UCSVLog::AddEnemyLog(TEXT("Test"), 
		EnemyLogID, TEXT("Spanwer"), EnemyLogData);
	
	// EnemyLogData 초기화 (다음 전투를 위해)
	EnemyLogData = FEnemyLogData();
	
	Destroy(); 
}

void AEnemySpawner::DropItemsAfterDead()
{
	for (const TSubclassOf<AActor>& ItemClassToSpawn : DropItems)
	{
		if ( !ItemClassToSpawn ) continue;

		// 적의 현재 위치 (발 밑)
		FVector ItemSpawnLocation = GetActorLocation();

		// 캡슐의 절반 높이만큼 올려서 아이템이 땅에 닿도록 조정
		AActor* ItemCDO = ItemClassToSpawn->GetDefaultObject<AActor>();
		if ( ItemCDO )
		{
			UCapsuleComponent* ItemCapsule = ItemCDO->FindComponentByClass<UCapsuleComponent>();
			if ( ItemCapsule )
			{
				ItemSpawnLocation.Z += ItemCapsule->GetScaledCapsuleHalfHeight();
			}
		}

		// 여러 아이템이 완전히 겹치지 않도록 X, Y 주변에 약간의 랜덤 오프셋 주기
		const float RandomXY = 40.f;
		ItemSpawnLocation.X += FMath::RandRange(-RandomXY, RandomXY);
		ItemSpawnLocation.Y += FMath::RandRange(-RandomXY, RandomXY);

		// 회전도 랜덤하게 설정
		FRotator SpawnRotation = FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f);

		// 월드에 아이템 액터 스폰
		GetWorld()->SpawnActor<AActor>(ItemClassToSpawn, ItemSpawnLocation, SpawnRotation);
	}
}

void AEnemySpawner::SetHitOverlay()
{
	if (SpawnerMesh && HitOverlayMaterial)
	{
		SpawnerMesh->SetOverlayMaterial(HitOverlayMaterial);
		
		// 0.2초 후에 ClearHitOverlay 함수를 호출하여 오버레이 제거
		GetWorld()->GetTimerManager().SetTimer(HitFlashTimerHandle, this,
			&AEnemySpawner::ClearHitOverlay, 0.2f, false);
	}
}

void AEnemySpawner::ClearHitOverlay()
{
	if (SpawnerMesh)
	{
		SpawnerMesh->SetOverlayMaterial(nullptr);
	}
}


#if WITH_EDITOR
void AEnemySpawner::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AEnemySpawner, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( PlayerDetectSphere ) PlayerDetectSphere->SetVisibility(true);
		}
		else
		{
			if ( PlayerDetectSphere ) PlayerDetectSphere->SetVisibility(false);
		}
	}
	
}
#endif
