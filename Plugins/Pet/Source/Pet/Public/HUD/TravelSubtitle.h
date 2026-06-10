#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TravelSubtitle.generated.h"

UCLASS()
class PET_API UTravelSubtitle : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 자막을 숨기는 내부 함수
	void HideSubtitle();
	
	// UMG 에디터의 텍스트 블록과 연결 (이름 필수 일치: Txt_Subtitle)
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Text_Subtitle;

	// 펫 이미지
	UPROPERTY(meta = (BindWidget))
	class UImage* Image_PetIcon;
	
	// 시간을 재기 위한 타이머 핸들
	FTimerHandle TimerHandle_HideSubtitle;

	void StartFadeOut();
	void OnFadeOutFinished();
	FTimerHandle DisplayTimerHandle;
	FTimerHandle FadeOutTimerHandle;
	
	
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FadeInAnim;
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FadeOutAnim;


public:
	// 외부에서 이 함수를 호출하여 자막을 띄웁니다.
	// Duration: 자막이 떠있는 시간 (기본 3초)
	// 텍스트와 함께 펫 아이콘도 받도록 수정 (기본값 nullptr)
	UFUNCTION(BlueprintCallable)
	void ShowSubtitle(FText InText, float Duration, UTexture2D* InPetIcon = nullptr);
};
