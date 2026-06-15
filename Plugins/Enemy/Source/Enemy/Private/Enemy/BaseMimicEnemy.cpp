#include "Enemy/BaseMimicEnemy.h"

#include "EnemyLogManager.h"
#include "Enemy/BaseMeleeEnemy.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"


ABaseMimicEnemy::ABaseMimicEnemy()
{
	// 1. 위장용 메시 생성
	DisguiseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisguiseMesh"));
	DisguiseMesh->SetupAttachment(RootComponent);
	DisguiseMesh->SetCollisionProfileName(TEXT("NoCollision")); // 콜리전 비활성화
	
	// 2. 감지용 트리거 생성 (부모의 DetectRangeSphere는 NoCollision이므로 새로 만듬)
	MimicTriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MimicTriggerSphere"));
	MimicTriggerSphere->SetupAttachment(RootComponent);
	MimicTriggerSphere->ShapeColor = FColor::Magenta;
	
	MeleeAttackPoint = CreateDefaultSubobject<USceneComponent>(TEXT("AttackPoint"));
	MeleeAttackPoint->SetupAttachment(RootComponent); // 루트 컴포넌트
	
	AttackRangePointSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangePointSphere"));
	AttackRangePointSphere->SetupAttachment(MeleeAttackPoint); // AttackPoint에 부착
	AttackRangePointSphere->ShapeColor = FColor::Purple;
	AttackRangePointSphere->SetVisibility(false);
	AttackRangePointSphere->SetHiddenInGame(false); 

	AutoPossessAI = EAutoPossessAI::Disabled;
	EnemyType = EEnemyType::EET_Mimic;

	// 미믹 태그 추가
	Tags.Add("Mimic");
}

void ABaseMimicEnemy::BeginPlay()
{
	//본체(스켈레탈 메시) 숨기기
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if(GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.f; // 둥둥 뜨거나 이상현상 방지
	}

	// 2. 위장 메시 보이기
	DisguiseMesh->SetVisibility(true);
	
	// 3. 감지 이벤트 바인딩
	if(MimicTriggerSphere)
	{
		MimicTriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ABaseMimicEnemy::OnDetectOverlap);
	}

	if ( HealthBarWidget )
	{
		HealthBarWidget->SetVisibility(false);
	}
	
	// 혹시 모르니 한번 더 태그 제거 
	Tags.Remove("Enemy");
	
	Super::BeginPlay();
}

void ABaseMimicEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckMeleeAttackHit(DeltaTime);
}

void ABaseMimicEnemy::CheckMeleeAttackHit(float DeltaTime)
{
	if (bIsMeleeAttacking == true && EnemyType == EEnemyType::EET_Mimic) 
	{
		UE_LOG(LogTemp, Warning, TEXT("aaaa"));
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
					
					UEnemyLogManager::EnemyLog(EEnemyLogType::Mimic, 
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
				
				// --- 플레이어 밀치기 효과 시작 ---
				ACharacter* PlayerCharacter = Cast<ACharacter>(OverlappingActor);
				if (PlayerCharacter)
				{
					// 1. 밀어낼 방향 계산 (보스 -> 플레이어)
					FVector PushDirection = PlayerCharacter->GetActorLocation() - GetActorLocation();
					PushDirection.Z = 0; // 수평 방향으로만 밀도록 Z값을 0으로 설정
					PushDirection.Normalize();

					// 2. 밀어낼 속도 계산 (방향 * 힘 + 위로 띄우는 힘)
					const FVector LaunchVelocity = PushDirection * 500 + FVector(0.f, 0.f, 200);

					// 3. 플레이어 캐릭터를 밀어냄
					// bXYOverride와 bZOverride를 true로 설정하여 현재 속도를 무시하고 새로운 속도를 적용합니다.
					PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);
				}
				// --- 플레이어 밀치기 효과 끝 ---
				
				// 공격한 목록에 추가하여 중복 피해를 방지합니다.
				HittedActors.Add(OverlappingActor);

				bIsMeleeAttacking = false; // 공격 상태를 종료합니다.
			}
		}
	}
}

void ABaseMimicEnemy::OnDetectOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if ( bIsHiding == false ) return;
	
	// 플레이어인지 확인 (Tag나 Class로 확인)
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(TEXT("Player"))) 
	{
		WakeUp();
	}
}

void ABaseMimicEnemy::WakeUp()
{
	if ( bIsHiding == false ) return;

	// 0. 이펙트 생성 (변신 시작 시점)
	if (WakeUpEffect)
	{
		// 현재 위치와 회전값에 이펙트 스폰
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), WakeUpEffect, GetActorLocation(), GetActorRotation());
	}
	
	// 1. 위장 해제
	if(DisguiseMesh) DisguiseMesh->SetVisibility(false);
	DisguiseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 2. 본 모습 드러내기
	GetMesh()->SetVisibility(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	if(GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 1.f;
	}

	// 3. 트리거 비활성화 (더 이상 감지 필요 없음)
	if(MimicTriggerSphere)
	{
		MimicTriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	// 4. AI 활성화 (부모의 함수 사용)
	SpawnAndPossessAIController();
	
	bIsHiding = false;

	if ( HealthBarWidget )
	{
		HealthBarWidget->SetVisibility(true);
	}
	
	// 태그 제거 및 추가
	Tags.Remove("Mimic");
	Tags.Add("Enemy");
}
#if WITH_EDITOR
void ABaseMimicEnemy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseMimicEnemy, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( AttackRangePointSphere ) 
			{
				AttackRangePointSphere->SetVisibility(true);
				AttackRangePointSphere->SetHiddenInGame(false);
			}
			if ( MimicTriggerSphere ) 
			{
				MimicTriggerSphere->SetVisibility(true);
				MimicTriggerSphere->SetHiddenInGame(false);
			}
		}
		else
		{
			if ( AttackRangePointSphere ) 
			{
				AttackRangePointSphere->SetVisibility(false);
				AttackRangePointSphere->SetHiddenInGame(true);
			}
			if ( MimicTriggerSphere ) 
			{
				MimicTriggerSphere->SetVisibility(false);
				MimicTriggerSphere->SetHiddenInGame(true);
			}
		}
	}
}
#endif