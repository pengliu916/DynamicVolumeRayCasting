// Need to be improved
#pragma once
#include <D3D11.h>
#include <DirectXMath.h>
#include "DXUT.h"
#include "DXUTcamera.h"

#include "header.h"


struct CB_EfficientRayCast_Frame
{
	XMMATRIX mInvView;
	XMMATRIX mView;
	XMMATRIX mViewProj;
	XMFLOAT4 f4ViewPos;
	XMFLOAT2 f2HalfWinSize;
	XMFLOAT2 f2WinReso;
	XMFLOAT2 f2MinMaxDensity;
	float NIU;
	float NIU1;

};


class EfficientRayCast
{
public:
#if DEBUG
	// the shader resource for render the active brick geometry over shaded image
	ID3D11GeometryShader*			m_pDebug_CellGS;
	ID3D11PixelShader*				m_pDebug_CellPS;
	ID3D11Texture2D*				m_pDebug_CellTex;
	ID3D11ShaderResourceView*		m_pDebug_CellSRV;
	ID3D11RenderTargetView*			m_pDebug_CellRTV;

	ID3D11Texture2D*				m_pDebug_DSTex;
	ID3D11DepthStencilView*			m_pDebug_DSSV;
#endif
	CModelViewerCamera				m_cCamera;
	D3D11_VIEWPORT					m_Viewport;

	ID3D11VertexShader*				m_pPassVS;
	// geometry shader used for active brick check and cube mesh generation
	ID3D11GeometryShader*			m_pActiveCellAndSOGS;
	// geometry shader for ray casting
	ID3D11GeometryShader*			m_pRaymarchGS;
	// pixel shader to render texture of ray start and end pos
	ID3D11PixelShader*				m_pFarNearPS;
	// pixel shader for ray casting
	ID3D11PixelShader*				m_pRayCastingPS;
	ID3D11Buffer*					m_pOutVB;
	ID3D11Buffer*					m_pPassVB;
	ID3D11InputLayout*				m_pOutVL;

	ID3D11RasterizerState*			m_pBackFaceRS;// Output rasterizer state
	ID3D11RasterizerState*			m_pFarNearRS;// Output rasterizer state

	ID3D11BlendState*				m_pBlendState;

	// for final image
	ID3D11Texture2D*				m_pOutputTex; 
	ID3D11ShaderResourceView*		m_pOutputSRV;
	ID3D11RenderTargetView*			m_pOutputRTV;

	// for NearFar image
	ID3D11Texture2D*				m_pFarNearTex;
	ID3D11ShaderResourceView*		m_pFarNearSRV;
	ID3D11RenderTargetView*			m_pFarNearRTV;

	ID3D11SamplerState*				m_pGenSampler;

	CB_EfficientRayCast_Frame		m_CBperFrame;
	ID3D11Buffer*					m_pCBperFrame;

	ID3D11ShaderResourceView*		m_pVolumeSRV;
	ID3D11ShaderResourceView*		m_pFlagVolumeSRV;

	bool							m_bShowGrid;

	EfficientRayCast( )
	{
		XMVECTORF32 vecEye = { 0.0f, 0.0f, -2.0f };
		XMVECTORF32 vecAt = { 0.0f, 0.0f, -0.0f };
		m_cCamera.SetViewParams(vecEye, vecAt);
		m_CBperFrame.f2MinMaxDensity = XMFLOAT2(MIN_DENSITY,MAX_DENSITY);
		m_bShowGrid = true;
	}

	HRESULT CreateResource( ID3D11Device* pd3dDevice,
						   ID3D11ShaderResourceView*	pVolumeSRV,
						   ID3D11ShaderResourceView*	pFlagVolumeSRV )
	{
		HRESULT hr = S_OK;

		m_pFlagVolumeSRV = pFlagVolumeSRV;
		m_pVolumeSRV = pVolumeSRV;

		ID3DBlob* pVSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"EfficientRayCast.fx", nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPassVS));
		DXUT_SetDebugName(m_pPassVS, "m_pPassVS");
		D3D11_SO_DECLARATION_ENTRY outputstreamLayout[] = { { 0, "POSITION", 0, 0, 3, 0 } };
		D3D11_INPUT_ELEMENT_DESC inputLayout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
		V_RETURN(pd3dDevice->CreateInputLayout(inputLayout, ARRAYSIZE(inputLayout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pOutVL));
		DXUT_SetDebugName(m_pOutVL, "m_pOutVL");
		pVSBlob->Release();

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"EfficientRayCast.fx", nullptr, "FarNearPS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pFarNearPS));
		DXUT_SetDebugName(m_pFarNearPS, "m_pFarNearPS");
		V_RETURN(DXUTCompileFromFile(L"EfficientRayCast.fx", nullptr, "RaymarchPS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pRayCastingPS));
		DXUT_SetDebugName(m_pRayCastingPS, "m_pRayCastingPS");

#if DEBUG
		// debug layer, show cell grid
		V_RETURN(DXUTCompileFromFile(L"EfficientRayCast.fx", nullptr, "Debug_CellPS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pDebug_CellPS));
		DXUT_SetDebugName(m_pDebug_CellPS, "m_pDebug_CellPS");
#endif
		pPSBlob->Release();

		
		ID3DBlob* pGSBlob = NULL;
		// For output-stream GS
		// Streamoutput GS
		UINT stride = 3 * sizeof(float);
		UINT elems = sizeof(outputstreamLayout) / sizeof(D3D11_SO_DECLARATION_ENTRY);
		V_RETURN(DXUTCompileFromFile(L"EfficientRayCast.fx", nullptr, "ActiveCellAndSOGS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pActiveCellAndSOGS));
		/*V_RETURN(pd3dDevice->CreateGeometryShaderWithStreamOutput(pGSBlob->GetBufferPointer(),
			pGSBlob->GetBufferSize(), outputstreamLayout, elems, &stride, 1,
			0, NULL, &m_pActiveCellAndSOGS));*/
		DXUT_SetDebugName(m_pActiveCellAndSOGS, "m_pActiveCellAndSOGS");

		V_RETURN(DXUTCompileFromFile(L"EfficientRayCast.fx", nullptr, "RaymarchGS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pRaymarchGS));
		DXUT_SetDebugName(m_pRaymarchGS, "m_pRaymarchGS");

#if DEBUG
		V_RETURN(DXUTCompileFromFile(L"EfficientRayCast.fx", nullptr, "Debug_CellGS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pDebug_CellGS));
		DXUT_SetDebugName(m_pDebug_CellGS, "m_pDebug_CellGS");
#endif
		pGSBlob->Release();


		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(float) * 8 * VOXEL_NUM_X * VOXEL_NUM_Y * VOXEL_NUM_Z / VOXEL_BRICK_RATIO / VOXEL_BRICK_RATIO / VOXEL_BRICK_RATIO;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pOutVB));
		DXUT_SetDebugName(m_pOutVB, "m_pOutVB");
		bd.ByteWidth = sizeof(float);
		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pPassVB));
		DXUT_SetDebugName(m_pPassVB, "m_pPassVB");

		ZeroMemory( &bd, sizeof(bd) );
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0    ;
		bd.ByteWidth = sizeof(CB_EfficientRayCast_Frame);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCBperFrame ));
		DXUT_SetDebugName( m_pCBperFrame, "EfficientRayCast_m_pCBperFrame");

		// Create the sample state
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory( &sampDesc, sizeof(sampDesc) );
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pd3dDevice->CreateSamplerState( &sampDesc, &m_pGenSampler ));
		DXUT_SetDebugName(m_pGenSampler, "m_pGenSampler");

		
		// rasterizer state
		D3D11_RASTERIZER_DESC rsDesc;
		rsDesc.FillMode = D3D11_FILL_SOLID;
		rsDesc.CullMode = D3D11_CULL_NONE;
		rsDesc.FrontCounterClockwise = FALSE;
		rsDesc.DepthBias = 0;
		rsDesc.DepthBiasClamp = 0.0f;
		rsDesc.SlopeScaledDepthBias = 0.0f;
		rsDesc.DepthClipEnable = TRUE;
		rsDesc.ScissorEnable = FALSE;
		rsDesc.MultisampleEnable = FALSE;
		rsDesc.AntialiasedLineEnable = FALSE;
		V_RETURN(pd3dDevice->CreateRasterizerState(&rsDesc, &m_pBackFaceRS));
		DXUT_SetDebugName(m_pBackFaceRS, "m_pBackFaceRS");


		// Create the blend state
		/* use blend to achieve rendering both tFar and tNear into RT for the coming raymarching pass
		 * the alpha channel store the tFar while the red channel stores the tNear*/
		D3D11_BLEND_DESC blendDesc;
		ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;// since Red channel has tNear so we keep the Min

		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;// alpha has tFar so we keep Max
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		//blendDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F ;
		V_RETURN(pd3dDevice->CreateBlendState(&blendDesc, &m_pBlendState));
		
		return hr;
	}

	HRESULT Resize(ID3D11Device* pd3dDevice, int iWidth, int iHeight)
	{
		HRESULT hr = S_OK;

		float fAspectRatio = iWidth / ( FLOAT )iHeight;
		// Setup the camera's projection parameters
		m_cCamera.SetProjParams(XM_PI / 4, fAspectRatio, 0.01f, 500.0f);
		m_CBperFrame.f2HalfWinSize.y = tan(XM_PI / 8.0f);
		m_CBperFrame.f2HalfWinSize.x = m_CBperFrame.f2HalfWinSize.y*fAspectRatio;
		m_CBperFrame.f2WinReso.x = iWidth;
		m_CBperFrame.f2WinReso.y = iHeight;
		m_cCamera.SetWindow(iWidth,iHeight );
		m_cCamera.SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);
		m_cCamera.SetRadius(2.f, 0.1f, 10.f);


		m_Viewport.Width = (float)iWidth;
		m_Viewport.Height = (float)iHeight;
		m_Viewport.MinDepth = 0.0f;
		m_Viewport.MaxDepth = 1.0f;
		m_Viewport.TopLeftX = 0;
		m_Viewport.TopLeftY = 0;


		// Create resource for texture
		D3D11_TEXTURE2D_DESC	TEXDesc;
		ZeroMemory(&TEXDesc, sizeof(TEXDesc));
		TEXDesc.MipLevels = 1;
		TEXDesc.ArraySize = 1;
		TEXDesc.SampleDesc.Count = 1;
		TEXDesc.Usage = D3D11_USAGE_DEFAULT;
		TEXDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		TEXDesc.CPUAccessFlags = 0;
		TEXDesc.MiscFlags = 0;
		TEXDesc.Width = iWidth;
		TEXDesc.Height = iHeight;
		TEXDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pOutputTex)); DXUT_SetDebugName(m_pOutputTex, "m_pOutputTex");
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pFarNearTex)); DXUT_SetDebugName(m_pFarNearTex, "m_pFarNearTex");// Normal tex
#if DEBUG
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pDebug_CellTex)); DXUT_SetDebugName(m_pDebug_CellTex, "m_pDebug_CellTex");
#endif

		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pOutputTex, nullptr, &m_pOutputRTV)); DXUT_SetDebugName(m_pOutputRTV, "m_pOutputRTV");
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pFarNearTex, nullptr, &m_pFarNearRTV)); DXUT_SetDebugName(m_pFarNearRTV, "m_pFarNearRTV");
#if DEBUG
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pDebug_CellTex, nullptr, &m_pDebug_CellRTV)); DXUT_SetDebugName(m_pDebug_CellRTV, "m_pDebug_CellRTV");
#endif

		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pOutputTex, nullptr, &m_pOutputSRV)); DXUT_SetDebugName(m_pOutputSRV, "m_pOutputSRV");
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pFarNearTex, nullptr, &m_pFarNearSRV)); DXUT_SetDebugName(m_pFarNearSRV, "m_pFarNearSRV");
#if DEBUG
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pDebug_CellTex, nullptr, &m_pDebug_CellSRV)); DXUT_SetDebugName(m_pDebug_CellSRV, "m_pDebug_CellSRV");

		D3D11_TEXTURE2D_DESC DSDesc;
		ZeroMemory(&DSDesc, sizeof(DSDesc));
		DSDesc.MipLevels = 1;
		DSDesc.ArraySize = 1;
		DSDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DSDesc.SampleDesc.Count = 1;
		DSDesc.Usage = D3D11_USAGE_DEFAULT;
		DSDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		DSDesc.Width = iWidth;
		DSDesc.Height = iHeight;
		V_RETURN(pd3dDevice->CreateTexture2D(&DSDesc, NULL, &m_pDebug_DSTex));
		DXUT_SetDebugName(m_pDebug_DSTex, "m_pDebug_DSTex");
		V_RETURN(pd3dDevice->CreateDepthStencilView(m_pDebug_DSTex, NULL, &m_pDebug_DSSV));
		DXUT_SetDebugName(m_pDebug_DSSV, "m_pDebug_DSSV");
#endif
		
		return hr;
	}

	void Destory()
	{
		SAFE_RELEASE(m_pPassVS);
		SAFE_RELEASE(m_pActiveCellAndSOGS);
		SAFE_RELEASE(m_pRaymarchGS);
		SAFE_RELEASE(m_pFarNearPS);
		SAFE_RELEASE(m_pRayCastingPS);
		SAFE_RELEASE(m_pOutVB);
		SAFE_RELEASE(m_pPassVB);
		SAFE_RELEASE(m_pOutVL);

		SAFE_RELEASE(m_pBackFaceRS);
		SAFE_RELEASE(m_pFarNearRS);

		SAFE_RELEASE(m_pBlendState);


#if DEBUG
		SAFE_RELEASE(m_pDebug_CellGS);
		SAFE_RELEASE(m_pDebug_CellPS);
#endif

		SAFE_RELEASE( m_pGenSampler);
		
		SAFE_RELEASE( m_pCBperFrame );
	}

	void Release()
	{

		SAFE_RELEASE(m_pOutputTex);
		SAFE_RELEASE(m_pOutputSRV);
		SAFE_RELEASE(m_pOutputRTV);

		SAFE_RELEASE(m_pFarNearTex);
		SAFE_RELEASE(m_pFarNearSRV);
		SAFE_RELEASE(m_pFarNearRTV);

#if DEBUG
		SAFE_RELEASE(m_pDebug_CellTex);
		SAFE_RELEASE(m_pDebug_CellSRV);
		SAFE_RELEASE(m_pDebug_CellRTV);

		SAFE_RELEASE(m_pDebug_DSTex);
		SAFE_RELEASE(m_pDebug_DSSV);
#endif
	}

	~EfficientRayCast()
	{
	}

	void Update( float fElapsedTime )
	{
		m_cCamera.FrameMove( fElapsedTime );
	}

	void Render( ID3D11DeviceContext* pd3dImmediateContext )
	{
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Efficient Ray Casting");

		// Update infor for GPU
		XMMATRIX m_Proj = m_cCamera.GetProjMatrix();
		XMMATRIX m_View = m_cCamera.GetViewMatrix();
		XMMATRIX m_World =m_cCamera.GetWorldMatrix();
		XMMATRIX m_WorldViewProjection = m_World*m_View*m_Proj;
		XMVECTOR t;

		m_CBperFrame.mViewProj = XMMatrixTranspose( m_WorldViewProjection );
		m_CBperFrame.mInvView = XMMatrixTranspose(XMMatrixInverse(&t, m_World*m_View));
		m_CBperFrame.mView = XMMatrixTranspose(m_World*m_View);
		XMStoreFloat4( &m_CBperFrame.f4ViewPos,m_cCamera.GetEyePt());

		pd3dImmediateContext->UpdateSubresource( m_pCBperFrame, 0, NULL, &m_CBperFrame, 0, 0 );

		// render the near_far texture for later raymarching
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Render the tNearFar");
		// Clear the render targets and depth view
		float ClearColor[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
		float ClearColor1[4] = { 50.0f, 50.0f, 50.0f, -10.0f };

		pd3dImmediateContext->ClearRenderTargetView(m_pOutputRTV, ClearColor);
		pd3dImmediateContext->ClearRenderTargetView(m_pFarNearRTV, ClearColor1);
#if DEBUG
		pd3dImmediateContext->ClearDepthStencilView(m_pDebug_DSSV, D3D11_CLEAR_DEPTH, 1.0, 0);
#endif		
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		pd3dImmediateContext->OMSetRenderTargets(1, &m_pFarNearRTV, NULL);
		UINT offset[1] = { 0 };
		//pd3dImmediateContext->SOSetTargets(1, &m_pOutVB, offset);
		pd3dImmediateContext->IASetInputLayout(m_pOutVL);
		UINT stride = sizeof(short);
		pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pPassVB, &stride, offset);
		pd3dImmediateContext->RSSetViewports(1, &m_Viewport);
		pd3dImmediateContext->VSSetShader(m_pPassVS, NULL, 0);
		pd3dImmediateContext->GSSetShader(m_pActiveCellAndSOGS, NULL, 0);
		pd3dImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBperFrame);
		pd3dImmediateContext->GSSetShaderResources(1, 1, &m_pFlagVolumeSRV);
		pd3dImmediateContext->PSSetShader(m_pFarNearPS, NULL, 0);
		pd3dImmediateContext->RSSetState(m_pBackFaceRS);
		ID3D11BlendState* bs;
		FLOAT blendFactor[4];
		UINT StencilRef;
		pd3dImmediateContext->OMGetBlendState(&bs,blendFactor,&StencilRef);
		pd3dImmediateContext->OMSetBlendState(m_pBlendState,NULL,0xffffffff);

		pd3dImmediateContext->Draw(VOXEL_NUM_X * VOXEL_NUM_Y * VOXEL_NUM_Z / VOXEL_BRICK_RATIO / VOXEL_BRICK_RATIO / VOXEL_BRICK_RATIO, 0);

		pd3dImmediateContext->OMSetBlendState(bs,blendFactor,StencilRef);
		SAFE_RELEASE(bs);
		DXUT_EndPerfEvent();
		// render through raymarching
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Raymarching");
		pd3dImmediateContext->PSSetSamplers(0, 1, &m_pGenSampler);

		pd3dImmediateContext->GSSetShader(m_pRaymarchGS,NULL,0);
		pd3dImmediateContext->PSSetShader(m_pRayCastingPS, NULL, 0);
#if DEBUG
		pd3dImmediateContext->OMSetRenderTargets(1,&m_pOutputRTV,m_pDebug_DSSV);
		//pd3dImmediateContext->OMSetRenderTargets(1, &m_pOutputRTV, NULL);
#else
		pd3dImmediateContext->OMSetRenderTargets(1,&m_pOutputRTV,NULL);
#endif
		pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pCBperFrame);
		pd3dImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBperFrame);
		pd3dImmediateContext->PSSetShaderResources(0, 1, &m_pVolumeSRV);
		pd3dImmediateContext->PSSetShaderResources(2, 1, &m_pFarNearSRV);
		pd3dImmediateContext->Draw(1,0);
		DXUT_EndPerfEvent();

#if DEBUG
		if (m_bShowGrid)
		{
			DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR2, L"Rendering active cell grid");
			//pd3dImmediateContext->ClearRenderTargetView(m_pDebug_CellRTV, ClearColor);
			pd3dImmediateContext->GSSetShader(m_pDebug_CellGS, NULL, 0);
			pd3dImmediateContext->PSSetShader(m_pDebug_CellPS, NULL, 0);
			//pd3dImmediateContext->OMSetRenderTargets(1, &m_pOutputRTV[2], NULL);
			pd3dImmediateContext->Draw(VOXEL_NUM_X * VOXEL_NUM_Y * VOXEL_NUM_Z / VOXEL_BRICK_RATIO / VOXEL_BRICK_RATIO / VOXEL_BRICK_RATIO, 0);
			/*	ID3D11RasterizerState* rs;
				pd3dImmediateContext->RSGetState(&rs);
				pd3dImmediateContext->RSSetState(m_pBackFaceRS);


				pd3dImmediateContext->RSSetState(rs);
				SAFE_RELEASE(rs);*/
			DXUT_EndPerfEvent();
		}
#endif
		ID3D11ShaderResourceView* ppSRVNULL[3] = { NULL, NULL, NULL};
		pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		pd3dImmediateContext->GSSetShaderResources(0, 3, ppSRVNULL);

		ID3D11RenderTargetView* ppRTVNULL[1] = {NULL};
		pd3dImmediateContext->OMSetRenderTargets(1, ppRTVNULL, NULL);
		DXUT_EndPerfEvent();

	}

	LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
	{
		m_cCamera.HandleMessages( hWnd, uMsg, wParam, lParam );
		switch (uMsg)
		{
		case WM_KEYDOWN:
		{
			int nKey = static_cast<int>(wParam);

			if (nKey == 'G')
			{
				m_bShowGrid = !m_bShowGrid;
			}
			break;
		}
		}
		return 0;
	}
};
