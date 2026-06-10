#include "Ping/PingActor.h"

#include "NiagaraComponent.h"
#include "Components/SphereComponent.h"

APingActor::APingActor()
{
	PrimaryActorTick.bCanEverTick = true;

	PrimaryActorTick.bCanEverTick = true; // Tick 활성화 필수

	// 1. 루트 컴포넌트 설정
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	RootComponent = SphereComp;
	SphereComp->SetSphereRadius(10.0f);
	SphereComp->SetCollisionProfileName(TEXT("NoCollision")); // 핑은 통과해야 하므로 충돌 없음

	// 2. 이펙트 컴포넌트 설정
	EffectComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EffectComp"));
	EffectComp->SetupAttachment(RootComponent);
	
}

void APingActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void APingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsMoving)
	{
		RunningTime += DeltaTime;

		// 1. '가상의 직선 위치'를 목표 지점으로 이동 (이전과 동일)
		CurrentLinearPos = FMath::VInterpConstantTo(CurrentLinearPos, TargetLocation, DeltaTime, MoveSpeed);

		// 2. 흔들림 계산 (Sine Wave)
		// 진행 방향 구하기
		FVector ForwardDir = (TargetLocation - StartLocation).GetSafeNormal();
		// 진행 방향의 오른쪽 벡터 구하기 (이쪽으로 흔들 예정)
		FVector RightDir = FVector::CrossProduct(ForwardDir, FVector::UpVector);

		// 남은 거리 비율 계산 (1.0 = 시작점, 0.0 = 도착점)
		float DistRemaining = FVector::Dist(CurrentLinearPos, TargetLocation);
		float Alpha = FMath::Clamp(DistRemaining / TotalDistance, 0.0f, 1.0f);

		// 사인 파동 계산: (방향 * sin(시간) * 폭 * 거리비율)
		// * Alpha를 곱해주는 이유: 도착 지점에 가까워질수록 흔들림을 0으로 줄여서 정확히 꽂히게 함
		FVector SwayOffset = RightDir * FMath::Sin(RunningTime * SwayFrequency) * SwayAmplitude * Alpha;

		// 3. 최종 위치 적용 (직선 위치 + 흔들림)
		SetActorLocation(CurrentLinearPos + SwayOffset);

		// 4. 도착 확인 (가상의 직선 위치 기준)
		if (DistRemaining < 10.0f) // 매우 가깝게 설정
		{
			// 더 이상 이동 로직(Tick)이 돌지 않도록 플래그 해제
			bIsMoving = false;

			// 흔들림(Sine) 때문에 위치가 살짝 어긋나 있을 수 있으므로, 목표 지점에 정확히 고정
			SetActorLocation(TargetLocation);

			// 일정 시간 후에 액터 제거
			SetLifeSpan(1.0f);
		}
	}
}

void APingActor::StartPingMovement(const FVector& TargetPos)
{
	TargetLocation = TargetPos + FVector(0.0f, 0.0f, 50.0f); 

	StartLocation = GetActorLocation();
	CurrentLinearPos = StartLocation; // 가상 위치 초기화

	TotalDistance = FVector::Dist(StartLocation, TargetLocation);
	if (TotalDistance <= 0.1f) TotalDistance = 1.0f; // 0 나누기 방지

	bIsMoving = true;
	RunningTime = 0.0f;
}

