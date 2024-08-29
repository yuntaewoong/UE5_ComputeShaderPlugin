#include "NumPixelPaintedComputeShader.h"
#include "ComputeShader/Public/NumPixelPaintedComputeShader/NumPixelPaintedComputeShader.h"
#include "PixelShaderUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "CanvasTypes.h"
#include "MeshDrawShaderBindings.h"
#include "RHIGPUReadback.h"
#include "MeshPassUtils.h"
#include "MaterialShader.h"

//통계 그룹을 정의합니다
DECLARE_STATS_GROUP(TEXT("NumPixelPaintedComputeShader"), STATGROUP_NumPixelPaintedComputeShader, STATCAT_Advanced);
//본 컴퓨트셰이더의 실행시간을 측정합니다
DECLARE_CYCLE_STAT(TEXT("NumPixelPaintedComputeShader Execute"), STAT_NumPixelPaintedComputeShader_Execute, STATGROUP_NumPixelPaintedComputeShader);

//NumPixelPaintedComputeShader 컴퓨트셰이더를 언리얼 엔진이 알아볼수 있도록 정의합니다.
class COMPUTESHADER_API FNumPixelPaintedComputeShader: public FGlobalShader
{
public:
	//언리얼 셰이더 컴파일러가 인식할 수 있도록 전역으로 선언합니다.
	DECLARE_GLOBAL_SHADER(FNumPixelPaintedComputeShader);
	//컴퓨트 셰이더에서 사용될 파라미터 구조체를 정의합니다.
	SHADER_USE_PARAMETER_STRUCT(FNumPixelPaintedComputeShader, FGlobalShader);
	
	
	//셰이더의 Permutation(순열)을 정의합니다. (ex/ 상황에  따라 다른 셰이더 코드 적용)
	class FNumPixelPaintedComputeShader_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<
		FNumPixelPaintedComputeShader_Perm_TEST
	>;

	//셰이더 파라미터 정의 시작(CPU->GPU 전송할 데이터 정의)
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, InputTexture) 
		SHADER_PARAMETER(FLinearColor, TargetColor1)
		SHADER_PARAMETER(FLinearColor, TargetColor2)
		SHADER_PARAMETER(FIntPoint, TextureSize)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutputCounter)
	END_SHADER_PARAMETER_STRUCT()
	//셰이더 파라미터 정의 종료

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		//컴퓨트 셰이더에서 사용될 상수를 정의합니다
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_NumPixelPaintedComputeShader_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_NumPixelPaintedComputeShader_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_NumPixelPaintedComputeShader_Z);
	}
private:
};

//실제 셰이더 코드의 파일위치,함수명,셰이더타입을 정의합니다.
IMPLEMENT_GLOBAL_SHADER(
	FNumPixelPaintedComputeShader,															//셰이더 클래스
	"/ComputeShaderShaders/NumPixelPaintedComputeShader/NumPixelPaintedComputeShader.usf",  //셰이더 위치
	"NumPixelPaintedComputeShader",															//함수명
	SF_Compute																				//셰이더타입						
);

//RHI: Rendering Hardware Interface
void FNumPixelPaintedComputeShaderInterface::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,				//그래픽 하드웨어를 제어하는 인터페이스
	FNumPixelPaintedComputeShaderDispatchParams Params, //CPU->GPU 전송할 데이터형식
	TFunction<void(int OutputVal1,int OutputVal2)> AsyncCallback        //비동기 작업 완료시 호출할 콜백함수
) 
{
	//RenderDependencyGroup 빌더로 셰이더 실행설정
	FRDGBuilder GraphBuilder(RHICmdList);
	{
		//디버깅을 위한 처리
		SCOPE_CYCLE_COUNTER(STAT_NumPixelPaintedComputeShader_Execute);
		DECLARE_GPU_STAT(NumPixelPaintedComputeShader)
		RDG_EVENT_SCOPE(GraphBuilder, "NumPixelPaintedComputeShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, NumPixelPaintedComputeShader);
		
		//Permutation설정
		typename FNumPixelPaintedComputeShader::FPermutationDomain PermutationVector;
		TShaderMapRef<FNumPixelPaintedComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid) 
		{//컴퓨트셰이더가 정상적으로 로드

			//GPU에 전달해줄 파라미터 생성
			FNumPixelPaintedComputeShader::FParameters* PassParameters =
				GraphBuilder.AllocParameters<FNumPixelPaintedComputeShader::FParameters>();

			//CPU메모리에 있는 데이터 Get
			
			// UTexture*를 GPU에서 사용 가능한 FRHITexture* 로변환
			check(Params.InputTexture->GetResource());
			check(Params.InputTexture->GetResource()->TextureRHI);
			FRHITexture* RHIInputTexture = Params.InputTexture->GetResource()->TextureRHI;


			//GPU에서 OutputData를 저장하기 위한 버퍼 생성
			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 2),
				TEXT("OutputBuffer")
			);

			//셰이더에서 사용될 데이터를 지정해줍니다
			PassParameters->InputTexture = RHIInputTexture;
			PassParameters->TargetColor1 = Params.TargetColor1;
			PassParameters->TargetColor2 = Params.TargetColor2;
			PassParameters->TextureSize = Params.TextureSize;
			PassParameters->OutputCounter = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_SINT));
			
			//워크 그룹의 크기, 한 워크 그룹당 쓰레드 수를 이용해서 그룹 카운트 계산
			FIntVector GroupCount(
				Params.TextureSize.X/NUM_THREADS_NumPixelPaintedComputeShader_X, 
				Params.TextureSize.Y/NUM_THREADS_NumPixelPaintedComputeShader_Y,
				1
			);

			//컴퓨트 셰이더 pass를 추가해서 실행가능하도록 설정
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteNumPixelPaintedComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{//Pass에서 실행되는 코드, 실제 컴퓨트 셰이더 실행
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				}
			);

			//GPU->CPU 전송을 위한 버퍼 생성
			FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteNumPixelPaintedComputeShaderOutput"));
			//OutputBuffer의 데이터를 GPUBufferReadback에 복사하는 Pass를 큐에 추가
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);

			//GPU->CPU 전송이 진행되는 람다함수 정의
			auto RunnerFunc = [GPUBufferReadback, AsyncCallback](auto&& RunnerFunc) -> void {
				if (GPUBufferReadback->IsReady()) 
				{//GPU->CPU 전송이 완료되면

					//Lock을 걸고 데이터를 읽어옴(GPU가 해당 버퍼에 작업할수 없게 막음)
					int32* Buffer = (int32*)GPUBufferReadback->Lock(1);
					int OutVal1 = Buffer[0];
					int OutVal2 = Buffer[1];
					GPUBufferReadback->Unlock();

					//완료 콜백 호출
					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, OutVal1,OutVal2]() {
						AsyncCallback(OutVal1,OutVal2);
					});

					delete GPUBufferReadback;
				} else 
				{
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
						RunnerFunc(RunnerFunc);
					});
				}
			};
			//정의한 람다함수를 렌더링 스레드에서 비동기 실행
			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
				RunnerFunc(RunnerFunc);
			});
			
		} 
		else 
		{
			#if WITH_EDITOR
				GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The compute shader has a problem.")));
			#endif
			
		}
	}

	GraphBuilder.Execute();
}