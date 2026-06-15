#include "Enemy/Melee_RebirthEnemy.h"

#include "AIController.h"
#include "NiagaraComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AMelee_RebirthEnemy::AMelee_RebirthEnemy()
{
	SoulPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SoulPoint"));
	SoulPoint->SetupAttachment(RootComponent); 
	
	SoulCollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SoulCollisionSphere"));
	SoulCollisionSphere->SetupAttachment(SoulPoint); 
	SoulCollisionSphere->ShapeColor = FColor::Cyan;
	SoulCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
	SoulCollisionSphere->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
	
	SoulEffectNiagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SoulEffectNiagara"));
	SoulEffectNiagara->SetupAttachment(SoulCollisionSphere);

	SoulEffectNiagara->bAutoActivate = false;
	
	EnemyType = EEnemyType::EET_Revive; // 근접 공격 적으로 설정
}

float AMelee_RebirthEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	// 소울 상태인지 먼저 확인하
	if (bIsActiveSoul)
	{
		SoulHpCount--;

		if ( SoulHpCount <= 0 )
		{
			// 완전히 죽었으므로 예약된 부활 타이머 취소 (중요)
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(ReviveTimerHandle);
			}
   
			bReviveFlag = false;
			EndBattleLog(); // 전투 로그 종료
			SpawnDeadEffectAndDestroy();
		}

		// 소울 상태에서는 물리 데미지 수치 반환 or 0 반환
		return DamageAmount;
	}

	// 2. 소울이 아닐 때만 일반 데미지 처리 수행
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AMelee_RebirthEnemy::AfterDieMontageEnd()
{
	// 부모 클래스의 AfterDieMontageEnd 호출 대신 커스텀 로직 사용
	//Super::AfterDieMontageEnd();
	if ( GetMesh() )
	{
		GetMesh()->bPauseAnims = true;
	}

	if ( ReviveCount <= 0 ) // 부활 횟수 다 쓴 경우
	{
		// 0.3초 후에 SpawnEffectAndDestroy 함수를 호출합니다.
		GetWorld()->GetTimerManager().SetTimer(DeathTimerHandle, this,
			&ABaseEnemy::SpawnDeadEffectAndDestroy, 1.0f, false);

		EnemyLogData.Result = TEXT("EnemyDead"); // 로그 데이터에 결과 기록
		
		EndBattleLog();
		return;
	}
	else
	{
		SpawnSoul(); // 소울 생성 함수 호출
	}
}

void AMelee_RebirthEnemy::Die()
{
	//Super::Die(); // 부모꺼 안씀
	
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
}

void AMelee_RebirthEnemy::SpawnSoul()
{
	bIsActiveSoul = true;
	
	// 1. 소울 이펙트 켜기
	if (SoulEffectNiagara)
	{
		SoulEffectNiagara->Activate(true); // 파티클 재생 시작

		SoulEffectNiagara->SetVisibility(true);
	}

	// 2. 소울 충돌 감지 켜기 (오버랩)
	if (SoulCollisionSphere)
	{
		SoulCollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	
	// [추가] 지정된 시간이(ReviveDelayTime) 지나면 Revive 함수 호출
	GetWorld()->GetTimerManager().SetTimer(
	  ReviveTimerHandle,      // 타이머 핸들
	  this,                   // 호출할 객체
	  &AMelee_RebirthEnemy::Revive, // 호출할 함수 주소
	  ReviveDelayTime,        // 지연 시간
	  false                   // 반복 여부 (false = 1회만 실행)
	 );
}

void AMelee_RebirthEnemy::Revive()
{
	//  혹시 모르니 실행 시 타이머 핸들 초기화
 	GetWorld()->GetTimerManager().ClearTimer(ReviveTimerHandle);

	if( bReviveFlag == false ) return; // 부활 플래그가 false면 부활 안함
	
	// 혹시 모르니 체력 완전 회복
	Health = MaxHealth; 

	if ( GetMesh() )
	{
		GetMesh()->bPauseAnims = false;
	}
	
	if ( ReviveMontage )
	{
		PlayAnimMontage(ReviveMontage);
	}
	
	// 부활 시 소울(구슬) 다시 숨기기
	if (SoulCollisionSphere)
	{
		SoulCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	if (SoulEffectNiagara)
	{
		SoulEffectNiagara->Deactivate(); // 파티클 정지
	}

	bIsActiveSoul = false;
	
	// 로그 기록 로직
	if (GetMesh()) 
	{
		// 스켈레탈 메쉬 에셋 이름 가져오기
		FString MeshName = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
					
		UEnemyLogManager::EnemyLog( EEnemyLogType::Revive,
			FString::Printf(TEXT("적 [%s]가 부활"), 
				*MeshName));
	}
	
	EnemyLogData.ReviveCount++; // 로그 데이터에 부활 횟수 누적
}

void AMelee_RebirthEnemy::AfterReviveMontageEnd()
{

	UE_LOG(LogTemp, Warning, TEXT("Enemy Revived!"));
	
	// 1. 체력 및 상태 초기화
	Health = MaxHealth;

	ReviveCount--;
    
	// 2. 충돌 다시 켜기 (Die에서 껐던 것 복구)
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// 3. 이동 다시 켜기 (DisableMovement의 반대)
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// 4. AI 로직 재개 (빙의는 유지한 채 신호만 줌)
	if (BlackboardComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy Revived!"));
		// 비헤이비어 트리가 다시 작동하도록 신호 변경
		BlackboardComp->SetValueAsBool(TEXT("IsDead"), false);
	}

	// 5. 체력바 다시 보이기
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(true);
	}
}

#if WITH_EDITOR
void AMelee_RebirthEnemy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseEnemy, bDebugMode))
	{
		if (bDebugMode == true)
		{
			if (SoulCollisionSphere) SoulCollisionSphere->SetVisibility(true);
		}
		else
		{
			if (SoulCollisionSphere) SoulCollisionSphere->SetVisibility(false);
		}
	}
}
#endif




