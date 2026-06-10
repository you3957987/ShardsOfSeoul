#include "Enemy/BaseSlimeEnemy.h"

#include "EnemyLogManager.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseSlimeEnemy::ABaseSlimeEnemy()
{
	MeleeAttackPoint = CreateDefaultSubobject<USceneComponent>(TEXT("AttackPoint"));
	MeleeAttackPoint->SetupAttachment(RootComponent); // 루트 컴포넌트
	
	AttackRangePointSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangePointSphere"));
	AttackRangePointSphere->SetupAttachment(MeleeAttackPoint); // AttackPoint에 부착
	AttackRangePointSphere->ShapeColor = FColor::Purple;
	AttackRangePointSphere->SetVisibility(false);
	AttackRangePointSphere->SetHiddenInGame(false); 

	AutoPossessAI = EAutoPossessAI::Disabled;
	EnemyType = EEnemyType::EET_Slime;

	// 미믹 태그 추가
	Tags.Add("Slime");
}

void ABaseSlimeEnemy::BeginPlay()
{
	Super::BeginPlay();
}

void ABaseSlimeEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckMeleeAttackHit(DeltaTime);
}

void ABaseSlimeEnemy::CheckMeleeAttackHit(float DeltaTime)
{
	if (bIsMeleeAttacking == true && EnemyType == EEnemyType::EET_Slime) 
	{
		TArray<AActor*> OverlappingActors;
		// AttackRangePointSphere와 겹치는 모든 액터를 가져옵니다.
		AttackRangePointSphere->GetOverlappingActors(OverlappingActors);
		
		for (AActor* OverlappingActor : OverlappingActors)
		{
			// 액터가 유효하고 "Player" 태그를 가지고 있으며, 아직 공격한 목록에 없는지 확인합니다.
			if (OverlappingActor && OverlappingActor->ActorHasTag(FName("Player")) && !HittedActors.Contains(OverlappingActor))
			{
				// 공격 로그를 출력합니다.
				UE_LOG(LogTemp, Warning, TEXT("Attack Hit Detected on: %s"), *OverlappingActor->GetName());

				// 로그 기록 로직
				if (GetMesh()) 
				{
					// 스켈레탈 메쉬 에셋 이름 가져오기
					FString MeshName = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
					
					UEnemyLogManager::EnemyLog(EEnemyLogType::Slime, 
						FString::Printf(TEXT("적 [%s]가 [%.f] 대미지"), 
							*MeshName, 
							MeleeAttackDamage));
				}
				
				EnemyLogData.TotalDamageDealt += MeleeAttackDamage; // 로그 데이터에 입힌 대미지 누적
				
				// 플레이어에게 대미지를 적용합니다.
				UGameplayStatics::ApplyDamage(
					OverlappingActor,
					MeleeAttackDamage, // 헤더 파일에 선언된 대미지 변수
					GetController(),
					this,
					UDamageType::StaticClass()
				);
				
				// 공격한 목록에 추가하여 중복 피해를 방지합니다.
				HittedActors.Add(OverlappingActor);

				bIsMeleeAttacking = false; // 공격 상태를 종료합니다.
			}
		}
	}
}

void ABaseSlimeEnemy::SpawnDeadEffectAndDestroy()
{
	if ( SplitCount == 0 )
	{
		Super::SpawnDeadEffectAndDestroy(); // 부모 클래스의 기능 == 원래 죽음 이펙트 및 제거 기능 호출
		return;
	}
	
	// 분열 클래스가 유효한 경우 분열 처리
	if (SplitSlimeClass)
	{
		FVector CurrentLocation = GetActorLocation();
		FRotator CurrentRotation = GetActorRotation();

		for (int32 i = 0; i < SplitCount; i++)
		{
			// 지정된 반경 내 랜덤 위치 계산
			FVector RandomOffset = FMath::VRand();
			RandomOffset.Z = 0.0f; // 높이는 현재 높이 유지 (지상 유닛 가정)
			RandomOffset.Normalize();
			RandomOffset *= FMath::RandRange(10.0f, SplitSpawnRadius); // 10.0f는 겹침 방지용 최소 거리

			FVector SpawnLocation = CurrentLocation + RandomOffset;

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.Owner = this;

			// 슬라임 스폰
			ABaseSlimeEnemy* NewSlime = GetWorld()->SpawnActor<ABaseSlimeEnemy>(
				SplitSlimeClass, 
				SpawnLocation, 
				CurrentRotation, 
				SpawnParams
			);

			if (NewSlime)
			{
				NewSlime->EnemyLogID = FString::Printf(TEXT("%s_Splint_%d"), *EnemyLogID, i);
				
				// 1. 크기 배율 적용
				NewSlime->SetActorScale3D(GetActorScale3D() * SplitSlimeScalePercent);

				// 2. 공격력 배율 적용
				NewSlime->MeleeAttackDamage = MeleeAttackDamage * SplitSlimeMeleeDamage;

				// 3. 무한 분열 방지: 자식 슬라임은 더 이상 분열하지 않도록 0으로 설정
				NewSlime->SplitCount = 0;
				
				// 4. 체력 배율 적용
				// 부모(BaseEnemy)의 MaxHealth에 배율을 곱해 새로운 최대 체력 계산
				float NewMaxHealth = MaxHealth * SplitSlimeHealthPercent;
    
				// 새 슬라임에 적용
				NewSlime->MaxHealth = NewMaxHealth;
				
				NewSlime->Health = NewMaxHealth; // 현재 체력도 최대 체력으로 초기화
				
				// 5. AI 컨트롤러 활성화 (바로 움직이게 하기 위함)
				NewSlime->SpawnDefaultController();
			}
		}
	}

	// 분열 처리가 끝났으므로 본체는 사망 이펙트 재생 및 제거 (부모 로직 호출)
	Super::SpawnDeadEffectAndDestroy();
}

#if WITH_EDITOR
void ABaseSlimeEnemy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseSlimeEnemy, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( AttackRangePointSphere ) 
			{
				AttackRangePointSphere->SetVisibility(true);
				AttackRangePointSphere->SetHiddenInGame(false);
			}
		}
		else
		{
			if ( AttackRangePointSphere ) 
			{
				AttackRangePointSphere->SetVisibility(false);
				AttackRangePointSphere->SetHiddenInGame(true);
			}
		}
	}
}
#endif