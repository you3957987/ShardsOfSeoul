// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyBoss/Hechi/BTTask_HechiTeleport.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "EnemyBoss/Hechi/Hechi.h"

class UNavigationSystemV1;

UBTTask_HechiTeleport::UBTTask_HechiTeleport()
{
	NodeName = "Hechi Teleport";
}

EBTNodeResult::Type UBTTask_HechiTeleport::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// AI 컨트롤러 가져오기
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }

    // 컨트롤러가 제어하는 폰을 ABossSkeletonMage로 캐스팅
    AHechi* BossPawn = Cast<AHechi>(AIController->GetPawn());
    if (!BossPawn)
    {
        return EBTNodeResult::Failed;
    }

    // 보스 자신의 현재 위치 가져오기
    const FVector BossLocation = BossPawn->GetActorLocation();

    // 내비게이션 시스템 가져오기
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSystem)
    {
        return EBTNodeResult::Failed;
    }

    FNavLocation IdealLocation;
    FNavLocation FallbackLocation; // 그나마 멀리 있는 위치 저장용
    bool bFoundIdealLocation = false;
    bool bFoundAnyLocation = false;
    float MaxDistanceFound = 0.f;
    const int32 MaxAttempts = 50; // 최대 시도 횟수

    for (int32 i = 0; i < MaxAttempts; ++i)
    {
        FNavLocation TempLocation;
        // 보스 주변 최대 반경 내에서 이동 가능한 랜덤 위치 찾기
        if (NavSystem->GetRandomReachablePointInRadius(BossLocation, BossPawn->AttackStruct.MaxTeleportDist, TempLocation))
        {
            bFoundAnyLocation = true;
            const float CurrentDistance = FVector::Dist(BossLocation, TempLocation.Location);
            
            // 현재 위치와 찾은 위치 사이의 거리가 최소 반경보다 큰지 확인 (이상적인 경우)
            if (CurrentDistance > BossPawn->AttackStruct.MinTeleportDist)
            {
                IdealLocation = TempLocation;
                bFoundIdealLocation = true;
                UE_LOG(LogTemp, Warning, TEXT("Success: Found ideal teleport location. %d"), i + 1);
                break; // 이상적인 위치를 찾았으므로 반복 종료
            }

            // 이상적인 위치는 아니지만, 현재까지 찾은 위치 중 가장 먼 곳이라면 저장
            if (CurrentDistance > MaxDistanceFound)
            {
                MaxDistanceFound = CurrentDistance;
                FallbackLocation = TempLocation;
            }
        }
    }

    if (bFoundIdealLocation)
    {
        // 이상적인 위치로 텔레포트
        BossPawn->PlayTeleportMontage(IdealLocation.Location);
        return EBTNodeResult::Succeeded;
    }
    
    if (bFoundAnyLocation)
    {
        // 이상적인 위치는 못 찾았지만, 유효한 위치를 하나라도 찾았다면 가장 멀었던 곳으로 텔레포트
        UE_LOG(LogTemp, Warning, TEXT("Warning: Could not find ideal location. Using fallback location."));
        BossPawn->PlayTeleportMontage(FallbackLocation.Location);
        return EBTNodeResult::Succeeded;
    }

    // 어떠한 유효한 위치도 찾지 못하면 실패 처리
    UE_LOG(LogTemp, Error, TEXT("Fail: Could not find any navigable teleport location."));
    return EBTNodeResult::Failed;
}
