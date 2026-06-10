#include "EnemyBoss/BossAIController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/BaseBossEnemy.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"

void ABossAIController::BeginPlay()
{
	Super::BeginPlay();
	GameState = UGameplayStatics::GetGameState(GetWorld());
}

void ABossAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	PollInit();
}

void ABossAIController::PollInit()
{
	if ( bFocusToPlayer == false )
	{
		if ( GameState && GameState->HasMatchStarted() ) // 매치 시작되고 생성되는 플레이어 생성 시점에 플레이어한테 포커스
		{
			TargetPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0); // 타깃 폰 설정
			
			if ( BehaviorTree )
			{
				RunBehaviorTree(BehaviorTree); // B 트리 실행 + 이걸 가장 먼저 해야 블랙보드 컴포넌트가 생성됨
				
				SetBlackboardKey(); // 자식에서 추가 구현한 블랙보드 키 설정 함수 호출
			}
			bFocusToPlayer = true;
		}
	}
}

void ABossAIController::SetBlackboardKey()
{
	BlackboardComp = GetBlackboardComponent();
	// 세부적인 키 값은 자식에서 구현
}
