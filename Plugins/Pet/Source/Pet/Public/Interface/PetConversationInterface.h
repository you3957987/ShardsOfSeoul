#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Header/PetState.h"
#include "Header/PetType.h"
#include "PetConversationInterface.generated.h"

// 언리얼 엔진 리플렉션용 클래스 (내용 없음)

UINTERFACE(MinimalAPI)
class UPetConversationInterface : public UInterface
{
	GENERATED_BODY()
};

// 실제 기능을 정의하는 인터페이스 클래스
class PET_API IPetConversationInterface
{
	GENERATED_BODY()
	
public:
	// 펫의 상태를 설정하는 함수 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetPetState(EPetState NewState);
	
	// 대화 시작을 요청하는 함수 (BlueprintNativeEvent로 선언하여 C++과 블루프린트 양쪽에서 구현 가능하게 함)
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void TriggerPetBigConversation(FName DialogueID);
	
	// 추가할 함수: 펫 객체를 주인에게 등록합니다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetMyPet(AActor* NewPet);

	// 펫의 타입을 반환하는 함수
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	EPetType GetMyPetType() const;

	// 펫이 가진 애님 몽타주 재생 함수
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PlayPetMontageFromConversation(UAnimMontage* MontageToPlay);
	
	// 주인 쪽에서 펫의 대화 시작함수를 호출하는 함수( 이상한거 맞음 )( Implementation 없이 펫 안의 StartConversation 다이렉트로 호출 )
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void MasterToPetBigConversation(FName DialogueID);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void TriggerPetSmallConversation(FName DialogueID);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void MasterToPetSmallConversation(FName DialogueID);
	
};