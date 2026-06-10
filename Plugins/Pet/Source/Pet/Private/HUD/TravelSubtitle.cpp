#include "HUD/TravelSubtitle.h"
#include "Components/TextBlock.h" 
#include "TimerManager.h" 
#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"

void UTravelSubtitle::ShowSubtitle(FText InText, float Duration, UTexture2D* InPetIcon)
{
	// 1. [핵심] 기존의 모든 타이머와 애니메이션을 즉시 중지 및 초기화
	GetWorld()->GetTimerManager().ClearTimer(DisplayTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(FadeOutTimerHandle);

	if (IsAnimationPlaying(FadeOutAnim)) StopAnimation(FadeOutAnim);
	if (IsAnimationPlaying(FadeInAnim))  StopAnimation(FadeInAnim);

	// 2. 위젯 상태를 강제로 '완전 불투명'하게 초기화
	// 페이드 아웃 도중에 새로운 자막이 들어올 때 투명도를 1로 돌려야 합니다.
	SetRenderOpacity(1.0f);
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 3. 텍스트 및 아이콘 설정
	if (Text_Subtitle)
	{
		Text_Subtitle->SetText(InText);
	}
	if (Image_PetIcon && InPetIcon)
	{
		Image_PetIcon->SetBrushFromTexture(InPetIcon);
	}

	// 4. FadeIn 재생
	if (FadeInAnim)
	{
		PlayAnimation(FadeInAnim);
	}

	// 보이스 길이(Duration)에 1~1.5초 정도 여유주기
	float ExtraHoldTime = 1.0f; 
	float TotalDisplayTime = Duration + ExtraHoldTime;

	GetWorld()->GetTimerManager().SetTimer(DisplayTimerHandle, this, &UTravelSubtitle::StartFadeOut, TotalDisplayTime, false);
}

void UTravelSubtitle::StartFadeOut()
{
	if (FadeOutAnim)
	{
		PlayAnimation(FadeOutAnim);

		// FadeOut 애니메이션 길이만큼 기다렸다가 완전히 숨김 (예: 0.5초)
		float AnimEndTime = FadeOutAnim->GetEndTime();
		GetWorld()->GetTimerManager().SetTimer(FadeOutTimerHandle, this, &UTravelSubtitle::OnFadeOutFinished, AnimEndTime, false);
	}
	else
	{
		// 애니메이션 없으면 바로 숨김
		OnFadeOutFinished();
	}
}

void UTravelSubtitle::OnFadeOutFinished()
{
	SetVisibility(ESlateVisibility::Hidden);
}
