#include "BaseEnemy.h"

#include "AIController.h"
#include "EnemyLogManager.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/Progressbar.h"
#include "Kismet/GameplayStatics.h" // UGameplayStatics 사용을 위한 헤더 파일
#include "Components/SphereComponent.h" // USphereComponent 사용을 위한 헤더 파일
#include "Components/WidgetComponent.h"
#include "EnemyHUD/EnemyHealthBarWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interface/PlayerDeadInterface.h"
#include "Particles/ParticleSystemComponent.h"
#include "CSVLog.h"  

ABaseEnemy::ABaseEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	AttackRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangeSphere"));
	AttackRangeSphere->SetupAttachment(RootComponent); // 루트 컴포넌트
	AttackRangeSphere->ShapeColor = FColor::Red;
	AttackRangeSphere->SetSphereRadius(AttackRange); // 초기 공격 범위 설정
	AttackRangeSphere->SetVisibility(false); // 디버그 모드 기본은 비활성화
	AttackRangeSphere->SetHiddenInGame(false);
	AttackRangeSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	DetectRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AcceptRangeSphere"));
	DetectRangeSphere->SetupAttachment(RootComponent); // 루트 컴포넌트
	DetectRangeSphere->ShapeColor = FColor::Green;
	DetectRangeSphere->SetSphereRadius(DetectRange);
	DetectRangeSphere->SetVisibility(false);
	DetectRangeSphere->SetHiddenInGame(false);
	DetectRangeSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ChaseRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ChaseRangeSphere"));
	ChaseRangeSphere->SetupAttachment(RootComponent); // 루트 컴포넌트
	ChaseRangeSphere->ShapeColor = FColor::Blue;
	ChaseRangeSphere->SetSphereRadius(ChaseRange);
	ChaseRangeSphere->SetVisibility(false);
	ChaseRangeSphere->SetHiddenInGame(false);
	ChaseRangeSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// 체력 바 위젯 컴포넌트 생성 및 설정
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::World); // 월드 공간으로 변경

	// 락온용 위젯 컴포넌트 생성 및 설정
	LockOnWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("LockOnWidget"));
	LockOnWidget->SetupAttachment(GetCapsuleComponent());
	LockOnWidget->SetWidgetSpace(EWidgetSpace::Screen); // 스크린 스페이스
	LockOnWidget->SetVisibility(false); // 기본은 숨김
	LockOnWidget->ComponentTags.Add(FName("LockOnMarker"));

	// 캐릭터 메시의 콜리전을 비활성화합니다.
	GetMesh()->SetCollisionProfileName(TEXT("NoCollision"));

	// 캡슐 컴포넌트가 카메라에 반응하지 않도록 설정합니다.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECollisionResponse::ECR_Block);
	
	// 적 캐릭터 태그 추가 -> 이걸 이용해서 프로젝트에서 플러그인 에너미 접근. 매우 중요!!!!
	Tags.Add(FName("Enemy")); 
	// AI 컨트롤러가 자동 빙의 하는거 제한. BeginPlay에서 스폰 몽타주 사용 여부에 따라 설정
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ABaseEnemy::BeginPlay()
{
	Super::BeginPlay();

	//Health = MaxHealth; // 시작할 때 체력을 최대 체력으로 설정

	if ( HealthBarWidget ) // 체력바 위젯에서 프로그레스바 설정
	{
		UEnemyHealthBarWidget* HealthBar = Cast<UEnemyHealthBarWidget>(HealthBarWidget->GetUserWidgetObject());
		if ( HealthBar )
		{
			HealthBar->HealthProgressBar->SetPercent(Health / MaxHealth);
		}
	}
	
	if ( bUseSpawnMontage == true && SpawnMontage ) // 스폰 몽타주 사용 시
	{
		UE_LOG(LogTemp, Warning, TEXT("Spawn Montage Play"));
		
		bIsSpawning = true; // 스폰 중 상태로 설정
		
		PlayAnimMontage(SpawnMontage);
		// 체력바 안보이게
		if ( HealthBarWidget )
		{
			HealthBarWidget->SetVisibility(false);
		}
	}
	else
	{
		if ( EnemyType != EEnemyType::EET_Mimic ) // 미믹 타입이 아니면
		{
			SpawnDefaultController();// 스폰 몽타주 사용 안하면 자동 빙의 설정
		}
	}
	TestDeadLogic(); // 죽음 로직 테스트 함수
}

void ABaseEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PollInit();

	UpdateHealthBarWidget(DeltaTime); // 체력 바 위젯 업데이트 -> 항상 캐릭터 쪽으로 바라보도록
	
}

void ABaseEnemy::PollInit()
{
	if ( bTargetInitalize == false )
	{
		TargetCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0); // 월드에서 첫 번째 플레이어 캐릭터를 가져옵니다.
		if ( TargetCharacter ) // 캐릭터가 유효한지 확인합니다.
		{
			IPlayerDeadInterface* DeadInterface = Cast<IPlayerDeadInterface>(TargetCharacter);
			IItemDropInterface* ItemDropInterface = Cast<IItemDropInterface>(TargetCharacter);
			
			if (DeadInterface && ItemDropInterface)
			{
				// 이전에 바인딩된 게 있다면 제거하고 등록 (중복 방지 안전장치)
				DeadInterface->ReturnOnPlayerDeadDelegate().RemoveDynamic(this, &ABaseEnemy::PlayerDeadLog);
				DeadInterface->ReturnOnPlayerDeadDelegate().AddDynamic(this, &ABaseEnemy::PlayerDeadLog);
				
				// 에너미가 죽은거 플레이어 알려주는거는 SpawnDeadEffectAndDestroy에서 직접
				// 알려줌
				
				bTargetInitalize = true; 
			}
			else 
			{
				UE_LOG(LogTemp, Error, TEXT("Player does not implement IPlayerDeadInterface"));
			}
		}
	}
	// 블랙보드 컴포넌트	
	if (bBlackboardInitialized == false)
	{
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			BlackboardComp = AIController->GetBlackboardComponent();
		}
		if (BlackboardComp)
		{
			// 패시브 몹은 일단 패시브한 상태(비선공)로 시작. 나머지는 아님
			if ( EnemyType == EEnemyType::EET_Passive ) BlackboardComp->SetValueAsBool(TEXT("Passive"), true); 
			else BlackboardComp->SetValueAsBool(TEXT("Passive"), false); 
			bBlackboardInitialized = true;
		}
	}
}

// 포커스, 사운드, 애니메이션, 이펙트 등
UAnimMontage* ABaseEnemy::Attack()
{
	// 여기서 SetFoucs 하면 나중에 ClearFocus 도 해줘야함. 일단 커찮아서 안함.

	if ( Health <= 0.f ) return nullptr;
	
	// 어택 몽타주 배열중 랜덤하게 하나 실행
	if ( AttackMontages.Num() > 0 )
	{
		if ( Health <= 0.f ) return nullptr;
		
		int32 RandomIndex = FMath::RandRange(0, AttackMontages.Num() - 1);
		
		if ( Health <= 0.f ) return nullptr;
		
		UAnimMontage* RandomAttackMontage = AttackMontages[RandomIndex];
		if ( RandomAttackMontage )
		{
			if ( Health <= 0.f ) return nullptr;
			
			PlayAnimMontage(RandomAttackMontage);
			bFocusPlayerAfterAttack = false; // 공격 후 포커스 시작 플래그를 false로 설정
			
			return RandomAttackMontage;
		}
	}

	return nullptr;
}

void ABaseEnemy::StartFocusPlayerAfterAttack()
{
	bFocusPlayerAfterAttack = true; // 공격 후 포커스 시작 플래그를 true로 설정
}

float ABaseEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, 
	class AController* EventInstigator, AActor* DamageCauser)
{
	if ( bIsSpawning == true ) return 0.0f; // 스폰 몽타주 재생 중이면 대미지 안받도록 함
	
	float DamageToApply = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	UE_LOG(LogTemp, Warning, TEXT("Enemy Take Damage : %f"), DamageToApply);

	FString MeshName = GetMesh() && GetMesh()->GetSkeletalMeshAsset() ? 
		GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
	
	EnemyLogData.TotalDamageReceived += DamageToApply; // 로그 데이터에 받은 대미지 누적
	
	if ( DamageToApply > 0.f )
	{
		Health -= DamageToApply;
		if ( Health <= 0.f )
		{
			UEnemyLogManager::EnemyLog(GetLogTypeFromEnemyType(), 
			FString::Printf(TEXT("적 [%s]가 [%.f] 대미지 받아 사망"), 
				*MeshName, DamageToApply));
			
			Die();
			return DamageToApply;
		}
	}
	
	SetHitOverlay(); // 히트 오버레이 설정
	
	UEnemyLogManager::EnemyLog(GetLogTypeFromEnemyType(), 
		FString::Printf(TEXT("적 [%s]가 [%.f] 대미지 받음 (%.f / %.f)"), 
			*MeshName, DamageToApply, MaxHealth,Health));
	
	return DamageToApply;
}

void ABaseEnemy::Die()
{
	if ( DeathMontage ) PlayAnimMontage(DeathMontage);
	
	// 충돌 비활성화
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	
	// AI 로직 중지
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsBool(TEXT("IsDead"), true);
	}
	
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(false);
	}
	
	EnemyLogData.Result = TEXT("EnemyDead"); // 로그 데이터에 결과 기록
	
	EndBattleLog(); // 전투 로그 종료
}

void ABaseEnemy::AfterDieMontageEnd()
{
	if ( GetMesh() )
	{
		GetMesh()->bPauseAnims = true;
	}
	
	// 0.3초 후에 SpawnEffectAndDestroy 함수를 호출합니다.
	GetWorld()->GetTimerManager().SetTimer(DeathTimerHandle, this,
		&ABaseEnemy::SpawnDeadEffectAndDestroy, 1.0f, false);
}

void ABaseEnemy::SpawnDeadEffectAndDestroy()
{
	if ( DeadSound )
	{
		UGameplayStatics::PlaySound2D(this, DeadSound);
	}
	if ( DeadEffectCascade )
	{
		// 현재 위치 + (앞방향 * 앞뒤 오프셋) + (윗방향 * 위아래 오프셋)
		const FVector SpawnLocation = GetMesh()->GetComponentLocation() 
			+ (GetActorForwardVector() * DeadEffectForwardOffset)
			+ (GetActorUpVector() * DeadEffectUpOffset);

		const FRotator SpawnRotation = GetActorRotation();
		const FVector SpawnScale = FVector(DeadEffectScale);

		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DeadEffectCascade,
			SpawnLocation, SpawnRotation, SpawnScale);
	}

	if (TargetCharacter && TargetCharacter->Implements<UItemDropInterface>())
	{
		IItemDropInterface::Execute_HandleEnemyDeadAndDropItem(TargetCharacter, this);
	}
	
	Destroy(); // 이펙트가 없으면 바로 액터 삭제
}

void ABaseEnemy::SpawnAndPossessAIController()
{
	// 애니메이셔 모드를 블루 프린트 모드로 설정
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	//UE_LOG(LogTemp, Warning, TEXT("SpawnAndPossessAIController Call") );
	SpawnDefaultController(); // 기본 AI 컨트롤러 스폰 및 빙의
	
	// 체력바 보이게
	if ( HealthBarWidget )
	{
		HealthBarWidget->SetVisibility(true);
	}
	
	bIsSpawning = false; // 스폰 상태 해제
}

void ABaseEnemy::ShowCharacterMesh()
{
	bUseSpawnMontage = false; // 스폰 몽타주 끝났음을 표시
}

void ABaseEnemy::UpdateHealthBarWidget(float DeltaTime)
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

void ABaseEnemy::SetAttackDelayToBehaviorTree(float Delay)
{
	if (BlackboardComp)	
	{
		BlackboardComp->SetValueAsFloat(TEXT("AttackDelay"), Delay);
	}
}

void ABaseEnemy::SetHitOverlay()
{
	if (GetMesh() && HitOverlayMaterial)
	{
		GetMesh()->SetOverlayMaterial(HitOverlayMaterial);
		
		// 0.2초 후에 ClearHitOverlay 함수를 호출하여 오버레이 제거
		GetWorld()->GetTimerManager().SetTimer(HitFlashTimerHandle, this,
			&ABaseEnemy::ClearHitOverlay, 0.2f, false);
	}
}

void ABaseEnemy::ClearHitOverlay()
{
	if (GetMesh())
	{
		GetMesh()->SetOverlayMaterial(nullptr);
	}
}

void ABaseEnemy::TestDeadLogic()
{
	// 죽음 로직 체크
	if (bCheckDeadLogic)
	{
		FTimerHandle DeadTestTimerHandle;
		// 5초 후에 체력을 0으로 만들고 Die() 함수를 호출합니다.
		GetWorld()->GetTimerManager().SetTimer(DeadTestTimerHandle, [this]()
		{
		 Health = 0.f;
		 Die();
		}, 5.0f, false);
	}
}

void ABaseEnemy::StartBattleLog()
{
	// 이미 전투 중이면 중복 기록 안 함
	if (bIsInBattle) return;

	bIsInBattle = true;
	BattleStartTime = GetWorld()->GetTimeSeconds(); // 현재 월드 시간 초단위 저장

	// 메쉬 이름 가져오기 헬퍼
	FString MeshName = GetMesh() && GetMesh()->GetSkeletalMeshAsset() ? 
		GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");

	UEnemyLogManager::EnemyLog(GetLogTypeFromEnemyType(), // 적절한 타입으로 변경 가능
		FString::Printf(TEXT("적 [%s] 전투 진입 (시작 시간: %.2f)"), *MeshName, BattleStartTime));
	
	EnemyLogData.StartWorldTime = BattleStartTime; // 로그 데이터에 시작 시간 기록
	
}

void ABaseEnemy::PlayerDeadLog()
{
	// 메쉬 이름 가져오기 헬퍼
	FString MeshName = GetMesh() && GetMesh()->GetSkeletalMeshAsset() ? 
		GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
	
	UEnemyLogManager::EnemyLog(GetLogTypeFromEnemyType(), 
		FString::Printf(TEXT("적 [%s] 의 공격으로 플레이어 사망"), *MeshName));
	
	EnemyLogData.Result = TEXT("PlayerDead"); // 로그 데이터에 결과 기록
	
	if ( BlackboardComp )
	{
		BlackboardComp->SetValueAsBool(TEXT("IsDead"), true);
	} 
	
	EndBattleLog();
}

void ABaseEnemy::EndBattleLog()
{
	// 전투 중이 아니면 종료 로그 안 함
	if (!bIsInBattle) return;

	float EndTime = GetWorld()->GetTimeSeconds();
	float BattleDuration = EndTime - BattleStartTime; // 지속 시간 계산

	FString MeshName = GetMesh() && GetMesh()->GetSkeletalMeshAsset() ? 
		GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");

	UEnemyLogManager::EnemyLog(GetLogTypeFromEnemyType(), 
		FString::Printf(TEXT("적 [%s] 전투 종료 | 소요 시간: %.2f초 (시작: %.2f / 종료: %.2f)"), 
			*MeshName, BattleDuration, BattleStartTime, EndTime));

	EnemyLogData.EndWorldTime = EndTime; // 로그 데이터에 종료 시간 기록
	EnemyLogData.ElapsedTime = BattleDuration; // 로그 데이터에 소요 시간 기록
	
	// 최종적으로 CSV 로그 파일에 추가
	UCSVLog::AddEnemyLog(TEXT("Test"), 
		EnemyLogID, GetEnemyTypeAsString(), EnemyLogData);
	
	// EnemyLogData 초기화 (다음 전투를 위해)
	EnemyLogData = FEnemyLogData();
	
	// 상태 초기화
	bIsInBattle = false;
	BattleStartTime = 0.f;
}

FString ABaseEnemy::GetEnemyTypeAsString() const
{
	// 이름에 Rebirth가 포함된 특수 케이스 처리
	if (GetName().Contains(TEXT("Rebirth")))
	{
		return TEXT("Revive");
	}

	// 각 타입에 맞는 명확한 문자열 반환
	switch (EnemyType)
	{
		case EEnemyType::EET_Melee:     return TEXT("Melee");
		case EEnemyType::EET_Ranged:    return TEXT("Ranged");
		case EEnemyType::EET_Exploder:  return TEXT("Exploder");
		case EEnemyType::EET_Transpar:  return TEXT("Transpar");
		case EEnemyType::EET_Mimic:     return TEXT("Mimic");
		case EEnemyType::EET_Slime:     return TEXT("Slime");
		case EEnemyType::EET_Mage:      return TEXT("Mage");
		case EEnemyType::EET_Guard:     return TEXT("Guard");
		case EEnemyType::EET_Passive:   return TEXT("Passive");
		case EEnemyType::EET_Burrow:    return TEXT("Burrow");
		case EEnemyType::EET_Revive:    return TEXT("Revive");
	default:                        return TEXT("Melee");
	}
}

EEnemyLogType ABaseEnemy::GetLogTypeFromEnemyType() const
{
	if (GetName().Contains(TEXT("Rebirth")))
	{
		return EEnemyLogType::Revive;
	}

	switch (EnemyType)
	{
		case EEnemyType::EET_Melee:     return EEnemyLogType::Melee;
		case EEnemyType::EET_Ranged:    return EEnemyLogType::Ranged;
		case EEnemyType::EET_Exploder:  return EEnemyLogType::Exploder;
		case EEnemyType::EET_Transpar:  return EEnemyLogType::Transpar;
		case EEnemyType::EET_Mimic:     return EEnemyLogType::Mimic;
		case EEnemyType::EET_Slime:     return EEnemyLogType::Slime;
		case EEnemyType::EET_Mage:      return EEnemyLogType::Mage;
		case EEnemyType::EET_Guard:     return EEnemyLogType::Guard;
		case EEnemyType::EET_Passive:   return EEnemyLogType::Passive;
		case EEnemyType::EET_Burrow:    return EEnemyLogType::Burrow;
		case EEnemyType::EET_Revive: return EEnemyLogType::Revive;
	default:                        return EEnemyLogType::Melee;
	}
}

#if WITH_EDITOR
void ABaseEnemy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseEnemy, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( AttackRangeSphere ) AttackRangeSphere->SetVisibility(true);
			if ( DetectRangeSphere ) DetectRangeSphere->SetVisibility(true);
			if ( ChaseRangeSphere ) ChaseRangeSphere->SetVisibility(true);
		}
		else
		{
			if ( AttackRangeSphere ) AttackRangeSphere->SetVisibility(false);
			if ( DetectRangeSphere ) DetectRangeSphere->SetVisibility(false);
			if ( ChaseRangeSphere ) ChaseRangeSphere->SetVisibility(false);
		}
	}
	// AttackRange 프로퍼티가 변경되었는지 확인합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseEnemy, AttackRange))
	{
		if (AttackRangeSphere)
		{
			// AttackRangeSphere의 반지름을 AttackRange 값으로 설정합니다.
			AttackRangeSphere->SetSphereRadius(AttackRange);
		}
	}
	if ( PropertyName == GET_MEMBER_NAME_CHECKED(ABaseEnemy, DetectRange) )
	{
		if ( DetectRangeSphere )
		{
			DetectRangeSphere->SetSphereRadius(DetectRange);
		}
	}
	if ( PropertyName == GET_MEMBER_NAME_CHECKED(ABaseEnemy, ChaseRange) )
	{
		if ( ChaseRangeSphere )
		{
			ChaseRangeSphere->SetSphereRadius(ChaseRange);
		}
	}
	if ( PropertyName == GET_MEMBER_NAME_CHECKED(ABaseEnemy, MoveSpeed) )
	{
		if ( GetCharacterMovement() )
		{
			GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
		}
	}
}
#endif

