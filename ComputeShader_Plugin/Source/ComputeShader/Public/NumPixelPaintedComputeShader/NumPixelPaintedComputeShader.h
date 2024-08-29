#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialRenderProxy.h"

#include "NumPixelPaintedComputeShader.generated.h"

//언리얼 엔진에서 사용할 컴퓨트셰이더 인풋정보
struct COMPUTESHADER_API FNumPixelPaintedComputeShaderDispatchParams
{
	UTexture* InputTexture;//색칠 정보를 조사할 텍스쳐
	FLinearColor TargetColor1;//조사할 색상1
	FLinearColor TargetColor2;//조사할 색상2
	FIntPoint TextureSize;//텍스쳐 사이즈([Int,int])
};

// This is a public interface that we define so outside code can invoke our compute shader.
class COMPUTESHADER_API FNumPixelPaintedComputeShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FNumPixelPaintedComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal1,int OutputVal2)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FNumPixelPaintedComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal1,int OutputVal2)> AsyncCallback
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
	}

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FNumPixelPaintedComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal1,int OutputVal2)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}else{
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};


//블루프린트 노출용
UCLASS()
class COMPUTESHADER_API UNumPixelPaintedComputeShader_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
};