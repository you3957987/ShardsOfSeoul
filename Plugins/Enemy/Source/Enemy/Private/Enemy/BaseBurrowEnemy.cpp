#include "Enemy/BaseBurrowEnemy.h"

#include "EnemyLogManager.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemyProjectile/BaseStreamProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseBurrowEnemy::ABaseBurrowEnemy()
{
	// 최대 걷기 속도 제로로 해서 움직이지 않도록 설정
	GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	
	// 이동 불가 
	GetCharacterMovement()->MaxWalkSpeed = 0.0f; 
	
	HealthBarWidget->SetVisibility(false);
	
	DetectRangeSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ChaseRangeSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	MeleeAttackPoint = CreateDefaultSubobject<USceneComponent>(TEXT("AttackPoint"));
	MeleeAttackPoint->SetupAttachment(RootComponent); // 루트 컴포넌트
	
	AttackRangePointSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangePointSphere"));
	AttackRangePointSphere->SetupAttachment(MeleeAttackPoint); // AttackPoint에 부착
	AttackRangePointSphere->ShapeColor = FColor::Purple;
	AttackRangePointSphere->SetVisibility(false);
	AttackRangePointSphere->SetHiddenInGame(false);
	
	BurrowAttackCollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("BurrowAttackCollisionSphere"));
	BurrowAttackCollisionSphere->SetupAttachment(RootComponent);
	BurrowAttackCollisionSphere->ShapeColor = FColor::Black;
	BurrowAttackCollisionSphere->SetVisibility(false);
	BurrowAttackCollisionSphere->SetHiddenInGame(false);
	
	EnemyType = EEnemyType::EET_Burrow;
}

void ABaseBurrowEnemy::BeginPlay()
{
	Super::BeginPlay();
	
	TArray<USphereComponent*> SphereComps;
	GetComponents<USphereComponent>(SphereComps);
	
	if ( AttackRangePointSphere )
	{
		AttackRangePointSphere->OnComponentBeginOverlap.AddDynamic(this, &ABaseBurrowEnemy::OnBeginOverlapAttackCollisionSphere);
		AttackRangePointSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
		
	if ( DetectRangeSphere ) DetectRangeSphere->OnComponentBeginOverlap.AddDynamic(this, &ABaseBurrowEnemy::OnBeginOverlapDetectRangeSphere);
	
	if ( ChaseRangeSphere ) ChaseRangeSphere->OnComponentEndOverlap.AddDynamic(this, &ABaseBurrowEnemy::OnEndOverlapChaseRangeSphere);
	
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
	
	Tags.Add(FName("Burrow")); // 버로우 태그 추가
}

void ABaseBurrowEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	//bIsRecentlyUnburrowed 계속 로그 찍기 
	UE_LOG( LogTemp, Warning, TEXT("bIsRecentlyUnburrowed : %s"), bIsRecentlyUnburrowed ? TEXT("true") : TEXT("false") );
}

float ABaseBurrowEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ABaseBurrowEnemy::OnBeginOverlapAttackCollisionSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if ( bIsRecentlyUnburrowed == true ) return;
	
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack Sphere Overlapped with Player!"));

		// 로그 기록 로직
		if (GetMesh()) 
		{
			// 스켈레탈 메쉬 에셋 이름 가져오기
			FString MeshName = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
					
			UEnemyLogManager::EnemyLog(EEnemyLogType::Burrow, 
				FString::Printf(TEXT("적 [%s]가 [%.f] 대미지 - 기본 공격"), 
					*MeshName, 
					AttackDamage));
		}
		
		EnemyLogData.TotalDamageDealt += AttackDamage; // 로그 데이터에 입힌 대미지 누적
		
		// 대미지 적용 ( 어택 대미지는 공격 전 가드 공격인지 아님 일반 공격인지에 따라 각각 함수에서 설정 )
		UGameplayStatics::ApplyDamage(OtherActor, AttackDamage, GetController(),
			this, UDamageType::StaticClass());
		
		// 다시 콜리전 끄기
		if ( AttackRangePointSphere ) AttackRangePointSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABaseBurrowEnemy::OnBeginOverlapDetectRangeSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		if ( bIsBurrowing == true )
		{
			PlayUnburrowMontage();
		}
	}
}

void ABaseBurrowEnemy::OnEndOverlapChaseRangeSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		if ( bIsBurrowing == false )
		{
			PlayBurrowMontage();
			
			// 타이머 정지 (발사 중단) 
			GetWorldTimerManager().ClearTimer(FireBreathTimerHandle);
		}
	}
}

void ABaseBurrowEnemy::AttackStart_AttackCollisionSphere()
{
	if ( AttackRangePointSphere ) AttackRangePointSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ABaseBurrowEnemy::AttackEnd_AttackCollisionSphere()
{
	if ( AttackRangePointSphere ) AttackRangePointSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABaseBurrowEnemy::PlayBurrowMontage()
{
	if ( BurrowMontage )
	{
		PlayAnimMontage(BurrowMontage);
	}
}

void ABaseBurrowEnemy::PlayUnburrowMontage()
{
	if ( UnburrowMontage )
	{
		PlayAnimMontage(UnburrowMontage);
	}
}

void ABaseBurrowEnemy::FinishUnburrow()
{
	bIsBurrowing = false;
	
	if ( HealthBarWidget ) HealthBarWidget->SetVisibility(true);
	
	// --- 1초 공격 제한 로직 추가 ---
	bIsRecentlyUnburrowed = true; // 현재 "공격 제한" 상태임
	GetWorldTimerManager().SetTimer(UnburrowDelayTimerHandle, this, &ABaseBurrowEnemy::ResetUnburrowDelay, 3.0f, false);
	
	if ( BurrowAttackCollisionSphere )
	{
		// 플레이어 액터 감지 
		TArray<AActor*> OverlappingActors;
		BurrowAttackCollisionSphere->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass()); // 캐릭터 클래스만 필터링

		for (AActor* Actor : OverlappingActors)
		{
			// 나 자신이 아니고, Player 태그가 있는지 확인
			if (Actor && Actor != this && Actor->ActorHasTag(FName("Player")))
			{
				ACharacter* PlayerCharacter = Cast<ACharacter>(Actor);
				if (PlayerCharacter)
				{
					const float PushForce = 700.0f;       // 밀어내는 힘 
					const float PushUpwardForce = 200.0f;  // 띄우는 힘 

					// 밀어낼 방향 계산 (적 -> 플레이어)
					FVector PushDirection = PlayerCharacter->GetActorLocation() - GetActorLocation();
					PushDirection.Z = 0; // 수직 방향 영향 제거
					PushDirection.Normalize();

					// 밀어낼 속도 계산
					const FVector LaunchVelocity = PushDirection * PushForce + FVector(0.f, 0.f, PushUpwardForce);

					// 플레이어 캐릭터를 밀쳐냄 (XY, Z 모두 강제 적용 override)
					PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);
					
					// 로그 기록 로직
					if (GetMesh()) 
					{
						// 스켈레탈 메쉬 에셋 이름 가져오기
						FString MeshName = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
					
						UEnemyLogManager::EnemyLog(EEnemyLogType::Burrow, 
							FString::Printf(TEXT("적 [%s]가 [%.f] 대미지 - 언버로우"), 
								*MeshName, 
								UnburrowAttackDamage));
					}
					
					EnemyLogData.TotalDamageDealt += UnburrowAttackDamage; // 로그 데이터에 입힌 대미지 누적
					
					// 대미지 적용 
					UGameplayStatics::ApplyDamage(PlayerCharacter, UnburrowAttackDamage, GetController(),
						this, UDamageType::StaticClass());
					
					// 로그 출력
					UE_LOG(LogTemp, Warning, TEXT("[BaseBurrowEnemy] Unburrow Attack Hit Player!"));
				}
			}
		}
	}
}

void ABaseBurrowEnemy::FinishBurrow()
{
	bIsBurrowing = true;
	
	if ( HealthBarWidget ) HealthBarWidget->SetVisibility(false);
}

void ABaseBurrowEnemy::StartFireBreath()
{
	// 이미 발사 중이라면 중복 실행 방지
	if (GetWorldTimerManager().IsTimerActive(FireBreathTimerHandle)) return;
	
	// 0.05초 간격으로 SpawnFireBreathProjectile 함수 반복 호출 (Loop = true)
	GetWorldTimerManager().SetTimer(FireBreathTimerHandle, this, 
		&ABaseBurrowEnemy::SpawnFireBreathProjectile, FireBreathInterval, true);
}

void ABaseBurrowEnemy::EndFireBreath()
{
	// 타이머 정지 (발사 중단)
	GetWorldTimerManager().ClearTimer(FireBreathTimerHandle);
}

void ABaseBurrowEnemy::SpawnFireBreathProjectile()
{
	if (FireBreathPoint && StreamProjectileClass)
	{
		// FireBreathPoint는 머리 뼈에 붙어 있으므로, 애니메이션에 따라 위치와 회전이 실시간으로 변함
		const FVector SpawnLocation = FireBreathPoint->GetComponentLocation();
		
		// 소켓의 회전을 가져오되, Pitch(위아래)와 Roll(좌우기울기)은 0으로 만들어 수평으로 발사
		FRotator SpawnRotation = FireBreathPoint->GetComponentRotation();
		SpawnRotation.Pitch = -30.0f; // 바닥으로 박히지 않게 수평 유지
		SpawnRotation.Roll = 0.0f;  // 기울어지지 않게

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		GetWorld()->SpawnActor<ABaseStreamProjectile>(StreamProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	}
}

#if WITH_EDITOR
void ABaseBurrowEnemy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseBurrowEnemy, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( AttackRangePointSphere ) AttackRangePointSphere->SetVisibility(true);
			if (BurrowAttackCollisionSphere) BurrowAttackCollisionSphere->SetVisibility(true);
		}
		else
		{
			if ( AttackRangePointSphere ) AttackRangePointSphere->SetVisibility(false);
			if ( BurrowAttackCollisionSphere ) BurrowAttackCollisionSphere->SetVisibility(false);
		}
	}
}
#endif