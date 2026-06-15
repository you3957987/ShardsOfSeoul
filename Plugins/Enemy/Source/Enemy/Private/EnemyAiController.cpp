#include "EnemyAiController.h"

#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "BaseEnemy.h"
#include "BehaviorTree/BlackboardComponent.h"


void AEnemyAiController::BeginPlay()
{
	Super::BeginPlay();

	GameState = UGameplayStatics::GetGameState(GetWorld());
}

void AEnemyAiController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PollInit();
}

// 사실상 필요한 초기화는 여기서 다 끝남 + B 트리 실행
void AEnemyAiController::PollInit()
{
	if ( bFocusToPlayer == false ) // 초기화는 한번만
	{
		if ( GameState && GameState->HasMatchStarted() ) // 매치 시작되고 생성되는 플레이어 생성 시점에 플레이어한테 포커스
		{
			Targetpawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
			
			ControlledEnemy = Cast<ABaseEnemy>(GetPawn());

			if ( BehaviorTree )
			{
				RunBehaviorTree(BehaviorTree); // B 트리 실행 + 이걸 가장 먼저 해야 블랙보드 컴포넌트가 생성됨
				
				BlackboardComp = GetBlackboardComponent();
				if ( BlackboardComp && Targetpawn )
				{
					BlackboardComp->SetValueAsObject(TEXT("TargetCharacter"), Targetpawn);
					BlackboardComp->SetValueAsVector("StartLocation", ControlledEnemy->GetActorLocation());
					BlackboardComp->SetValueAsFloat("AttackRange", ControlledEnemy->AttackRange);
					BlackboardComp->SetValueAsFloat("DetectRange", ControlledEnemy->DetectRange);
					BlackboardComp->SetValueAsFloat("AttackDelay", ControlledEnemy->AttackDelay);
					BlackboardComp->SetValueAsFloat("PatrolDelay", ControlledEnemy->PatrolDelay);
					BlackboardComp->SetValueAsEnum(TEXT("EnemyType"), static_cast<uint8>(ControlledEnemy->EnemyType));
					bAlwaysChase = ControlledEnemy->bAlwaysChase; // 무조건 추적 모드 여부
				}
			}
			bFocusToPlayer = true;
		}
	}
}

// 감지 범위 안에 있는지 여부
bool AEnemyAiController::IsInArrangeOfDetectRange()
{
	if (ControlledEnemy == nullptr || Targetpawn == nullptr) return false;
	const float DistanceToTarget = FVector::Dist(ControlledEnemy->GetActorLocation(), Targetpawn->GetActorLocation());

	if ( DistanceToTarget <= ControlledEnemy->DetectRange )
	{
		bIsChasing = true;
		return true;
	}
	else
	{
		return false;
	}
}

bool AEnemyAiController::IsInArrangeOfChaseRange()
{
	if (ControlledEnemy == nullptr || Targetpawn == nullptr) return false;
	const float DistanceToTarget = FVector::Dist(ControlledEnemy->GetActorLocation(), Targetpawn->GetActorLocation());

	if ( DistanceToTarget <= ControlledEnemy->ChaseRange )
	{
		return true;
	}
	else 
	{
		bIsChasing = false;
		return false;
	}
}

