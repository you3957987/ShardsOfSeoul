#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConversationTrigger.generated.h"

// 펫의 위치랑 매칭시킬 펫의 상태 열거형
UENUM(BlueprintType)
enum class EConversationType : uint8
{
	ECT_BIG UMETA(DisplayName = "Big"), // 큰 자막바 
	ECT_SMALL UMETA(DisplayName = "Small"), // 작은 자막바
	
	ECT_MAX UMETA(DisplayName = "Default") // 최대값, 추가적인 값을 위한 공간
};

UCLASS()
class PET_API AConversationTrigger : public AActor
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	
	// 트리거 범위 박스 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* TriggerBox;

	// 트리거 박스에 들어왔을 때의 처리 함수
	UFUNCTION()
	void OnTriggerBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	
	// 감지할 태그를 에디터에서 지정 (기본값은 Player )
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FName TargetTag = TEXT("Player");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	EConversationType ConversationType = EConversationType::ECT_BIG;

	// 트리거 박스 보이게 할지 말지 (디버그용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	bool bDebugMode = false;
	
public:
	AConversationTrigger();

	// 이 부분이 빠져 있어서 에러가 발생했습니다. 추가해주세요.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FName DialogueID;


#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
