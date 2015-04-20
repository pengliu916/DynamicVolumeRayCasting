#pragma once
#include <D3D11.h>
#include <DirectXMath.h>
#include <vector>
#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"

#include "Header.h"

#define frand() ((float)rand()/RAND_MAX)

using namespace DirectX;
using namespace std;

// Ball structure
struct Ball
{
    float fPower;			// size of this metaball
    float fOribtRadius;		// radius of oribt
    float fOribtSpeed;		// speed of rotation
    float fOribtStartPhase;	// initial phase
	XMFLOAT4 f4Color;		// color
};


// constant buffer contain information of the volume, only need to submit to GPU once volume property changes
struct CB_VolumeInfo
{
	// the following three values suppose to be UINT, but to avoid UINT to float conversion 
	// late in the shader, we decide to use float at the first place
	float fHalfVolReso_x;
	float fHalfVolReso_y;
	float fHalfVolReso_z;

	float fVoxelSize;
};

// constant buffer contain volume content descriptions: balls info.
struct CB_Balls
{
	XMFLOAT4	fBalls[150]; //xyz: center pos; w: radiation power
	XMFLOAT4	fBallsCol[150]; // color for each ball
	int			iNumBalls;
	//padding
	int			dummy0;
	int			dummy1;
	int			dummy2;
};

class DensityFuncVolume
{
public:

	// compute shader for volume update with out flag volume update
	ID3D11ComputeShader*			m_pVolumeUpdateCS;
	// compute shader for volume update with flag volume update
	ID3D11ComputeShader*			m_pVolumeUpdatewithFlagUpdateCS;
	// compute shader for reset volume
	ID3D11ComputeShader*			m_pResetVolumeCS;
	// compute shader for reset flag volume
	ID3D11ComputeShader*			m_pRestFlagVolumeCS;

	// volume for density and color data: xyz for color and w for density
	ID3D11Texture3D*				m_pVolTex;
	ID3D11ShaderResourceView*		m_pVolSRV;
	ID3D11UnorderedAccessView*		m_pVolUAV;

	// flag volume to enable empty space skipping
	ID3D11Texture3D*				m_pBrickVolTex;
	ID3D11ShaderResourceView*		m_pBrickVolSRV;
	ID3D11UnorderedAccessView*		m_pBrickVolUAV;

	// constant buffer for volume parameters
	ID3D11Buffer*					m_pCB_VolumeInfo;
	CB_VolumeInfo					m_cbVolumeInfo;

	// constant buffer for volume content
	ID3D11Buffer*					m_pCB_Balls;
	CB_Balls						m_cbBalls;

	// info. to control volume update, processed by cpu
	std::vector<Ball>				m_vecBalls;

	// animation pause flag
	bool							m_bAnimated;

	// animation time line
	double							m_dTime;

	// function to add a ball entity into the volume
	void AddBall()
	{
		Ball ball;
		float r = ( 0.6f * frand() + 0.7f ) * m_cbVolumeInfo.fHalfVolReso_x * m_cbVolumeInfo.fVoxelSize * 0.1f;
		ball.fPower = r * r;
		ball.fOribtRadius = m_cbVolumeInfo.fHalfVolReso_x * m_cbVolumeInfo.fVoxelSize * (0.6f + ( frand() - 0.3f ) * 0.4f);

		if( ball.fOribtRadius + r > 0.45f * m_cbVolumeInfo.fHalfVolReso_x * m_cbVolumeInfo.fVoxelSize*2.f)
		{
			r = 0.45f * m_cbVolumeInfo.fHalfVolReso_x * m_cbVolumeInfo.fVoxelSize*2.f - ball.fOribtRadius;
			ball.fPower = r * r;
		}
		float speedF =  6.f * ( frand() - 0.5f );
		if( abs( speedF ) < 1.f) speedF = ( speedF > 0.f ? 1.f : -1.f ) * 1.f;
		ball.fOribtSpeed = 1.0f / ball.fPower * 0.0005f * speedF;
		ball.fOribtStartPhase = frand() * 6.28f;

		float alpha = frand() * 6.28f;
		float beta = frand() * 6.28f;
		float gamma = frand() * 6.28f;

		XMMATRIX rMatrix = XMMatrixRotationRollPitchYaw( alpha, beta, gamma );
		XMVECTOR colVect = XMVector3TransformNormal( XMLoadFloat3( &XMFLOAT3( 1, 0, 0 )), rMatrix );
		XMFLOAT4 col;
		XMStoreFloat4( &col, colVect );
		col.x = abs( col.x );
		col.y = abs( col.y );
		col.z = abs( col.z );

		ball.f4Color = col;

		if( m_vecBalls.size() < MAX_BALLS ) m_vecBalls.push_back( ball );
	}

	// function to remove a ball entity from the volume
	void RemoveBall()
	{
		if( m_vecBalls.size() > 0 ) m_vecBalls.pop_back();
	}

	// initialize the volume with input resolution and voxel size, and put 20 ball entities into it 
	DensityFuncVolume( float voxelSize, UINT width = 384, UINT height = 384, UINT depth = 384 )
	{
		m_cbVolumeInfo.fHalfVolReso_x = width / 2;
		m_cbVolumeInfo.fHalfVolReso_y = height / 2;
		m_cbVolumeInfo.fHalfVolReso_z = depth / 2;
		m_cbVolumeInfo.fVoxelSize = voxelSize;
		m_bAnimated = true;
		m_dTime = 0;

		for( int i = 0; i < 20; i++ )
			AddBall();
	}

	HRESULT CreateResource( ID3D11Device* pd3dDevice )
	{
		HRESULT hr = S_OK;

		wstring filename = L"DensityFuncVolume.hlsl";

		ID3DBlob* pCSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(filename.c_str(), nullptr, "VolumeUpdateCS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pVolumeUpdateCS));
		DXUT_SetDebugName(m_pVolumeUpdateCS, "m_pVolumeUpdateCS");
		V_RETURN(DXUTCompileFromFile(filename.c_str(), nullptr, "ResetVolumeCS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pResetVolumeCS));
		DXUT_SetDebugName(m_pResetVolumeCS, "m_pResetVolumeCS");
		V_RETURN(DXUTCompileFromFile(filename.c_str(), nullptr, "VolumeUpdatewithFlagUpdateCS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pVolumeUpdatewithFlagUpdateCS));
		DXUT_SetDebugName(m_pVolumeUpdatewithFlagUpdateCS, "m_pVolumeUpdatewithFlagUpdateCS");
		V_RETURN(DXUTCompileFromFile(filename.c_str(), nullptr, "FlagVolumeRestCS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pRestFlagVolumeCS));
		DXUT_SetDebugName(m_pRestFlagVolumeCS, "m_pRestFlagVolumeCS");
		pCSBlob->Release();


		D3D11_BUFFER_DESC bd;
		ZeroMemory( &bd, sizeof(bd) );

		// create constant buffer for volume info.
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0  ;
		bd.ByteWidth = sizeof( CB_VolumeInfo );
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCB_VolumeInfo ));
		DXUT_SetDebugName(m_pCB_VolumeInfo,"m_pCB_VolumeInfo");
		// const buffer for ball list
		bd.ByteWidth = sizeof( CB_Balls );
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCB_Balls ));
		DXUT_SetDebugName(m_pCB_Balls,"m_pCB_Balls");


		// Create the volume
		D3D11_TEXTURE3D_DESC dstex;
		dstex.Width = m_cbVolumeInfo.fHalfVolReso_x * 2;
		dstex.Height = m_cbVolumeInfo.fHalfVolReso_y * 2;
		dstex.Depth = m_cbVolumeInfo.fHalfVolReso_z * 2;
		dstex.MipLevels = 1;
		dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		dstex.Usage = D3D11_USAGE_DEFAULT;
		dstex.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		dstex.CPUAccessFlags = 0;
		dstex.MiscFlags = 0;
		V_RETURN( pd3dDevice->CreateTexture3D( &dstex, NULL, &m_pVolTex ));
		DXUT_SetDebugName(m_pVolTex,"m_pVolTex");
		// Create the resource view
		V_RETURN( pd3dDevice->CreateShaderResourceView( m_pVolTex, NULL,&m_pVolSRV ));
		DXUT_SetDebugName(m_pVolSRV,"m_pVolSRV");
		// Create the render target views
		V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pVolTex, NULL, &m_pVolUAV ));
		DXUT_SetDebugName(m_pVolUAV,"m_pVolUAV");

		// Create the flag volume
		dstex.Width = m_cbVolumeInfo.fHalfVolReso_x * 2 / VOXEL_BRICK_RATIO;
		dstex.Height = m_cbVolumeInfo.fHalfVolReso_y * 2 / VOXEL_BRICK_RATIO;
		dstex.Depth = m_cbVolumeInfo.fHalfVolReso_z * 2 / VOXEL_BRICK_RATIO;
		dstex.MipLevels = 1;
		dstex.Format = DXGI_FORMAT_R8_SINT;
		dstex.Usage = D3D11_USAGE_DEFAULT;
		dstex.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		dstex.CPUAccessFlags = 0;
		dstex.MiscFlags = 0;
		V_RETURN(pd3dDevice->CreateTexture3D(&dstex, NULL, &m_pBrickVolTex));
		DXUT_SetDebugName(m_pBrickVolTex, "m_pBrickVolTex");
		// Create the resource view
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pBrickVolTex, NULL, &m_pBrickVolSRV));
		DXUT_SetDebugName(m_pBrickVolSRV, "m_pBrickVolSRV");
		// Create the unorderd access views
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pBrickVolTex, NULL, &m_pBrickVolUAV));
		DXUT_SetDebugName(m_pBrickVolUAV, "m_pBrickVolUAV");

		ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
		pd3dImmediateContext->UpdateSubresource(m_pCB_VolumeInfo, 0, NULL, &m_cbVolumeInfo, 0, 0);

		return hr;
	}

	void Release()
	{
		SAFE_RELEASE(m_pVolumeUpdateCS);
		SAFE_RELEASE(m_pResetVolumeCS);
		SAFE_RELEASE(m_pRestFlagVolumeCS);
		SAFE_RELEASE(m_pVolumeUpdatewithFlagUpdateCS);

		SAFE_RELEASE( m_pVolSRV );
		SAFE_RELEASE( m_pVolTex );
		SAFE_RELEASE(m_pVolUAV);

		SAFE_RELEASE(m_pBrickVolTex);
		SAFE_RELEASE(m_pBrickVolSRV);
		SAFE_RELEASE(m_pBrickVolUAV);

		SAFE_RELEASE( m_pCB_VolumeInfo );
		SAFE_RELEASE( m_pCB_Balls );
	}

	void Update( double fTime, float fElapsedTime )
	{
		if( m_bAnimated ){
			m_dTime += fElapsedTime;
			m_cbBalls.iNumBalls = (int)m_vecBalls.size();
			for( int i = 0; i < m_vecBalls.size(); i++ ){
				Ball ball = m_vecBalls[i];
				m_cbBalls.fBalls[i].x = ball.fOribtRadius * (float)cosf( m_dTime * ball.fOribtSpeed + ball.fOribtStartPhase );
				m_cbBalls.fBalls[i].y = ball.fOribtRadius * (float)sinf( m_dTime * ball.fOribtSpeed + ball.fOribtStartPhase );
				m_cbBalls.fBalls[i].z = 0.3f * ball.fOribtRadius * (float)sinf( 2.f * m_dTime * ball.fOribtSpeed + ball.fOribtStartPhase );
				m_cbBalls.fBalls[i].w = ball.fPower;
				m_cbBalls.fBallsCol[i] = ball.f4Color;
			}
		}
	}

	void Render( ID3D11DeviceContext* pd3dImmediateContext, bool updateFlagVolume = false )
	{
		if( m_bAnimated ){
			// clean data volume
			DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Clean Data Volume");
			pd3dImmediateContext->CSSetShader(m_pResetVolumeCS, NULL, 0);
			UINT initCounts = 0;
			ID3D11UnorderedAccessView* uavs[2] = { m_pVolUAV, m_pBrickVolUAV };
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, uavs, &initCounts);
			pd3dImmediateContext->Dispatch(VOXEL_NUM_X / THREAD_X, VOXEL_NUM_Y / THREAD_Y, VOXEL_NUM_Z / THREAD_Z);
			DXUT_EndPerfEvent();

			if (updateFlagVolume){
				// clean flag volume
				DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Clean flag Volume");
				pd3dImmediateContext->CSSetShader(m_pRestFlagVolumeCS, NULL, 0);
				pd3dImmediateContext->Dispatch(ceil((float)VOXEL_NUM_X / VOXEL_BRICK_RATIO / THREAD_X),
											   ceil((float)VOXEL_NUM_Y / VOXEL_BRICK_RATIO / THREAD_Y),
											   ceil((float)VOXEL_NUM_Z / VOXEL_BRICK_RATIO / THREAD_Z));
				DXUT_EndPerfEvent();
			}

			// update data volume
			DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Update Data Volume");
			pd3dImmediateContext->UpdateSubresource( m_pCB_VolumeInfo, 0, NULL, &m_cbVolumeInfo, 0, 0 );
			pd3dImmediateContext->UpdateSubresource( m_pCB_Balls, 0, NULL, &m_cbBalls, 0, 0 );
			pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &m_pCB_VolumeInfo );
			pd3dImmediateContext->CSSetConstantBuffers( 1, 1, &m_pCB_Balls );
			if(updateFlagVolume) pd3dImmediateContext->CSSetShader(m_pVolumeUpdatewithFlagUpdateCS,NULL,0);
			else pd3dImmediateContext->CSSetShader(m_pVolumeUpdateCS, NULL, 0);
			pd3dImmediateContext->Dispatch(VOXEL_NUM_X / THREAD_X, VOXEL_NUM_Y / THREAD_Y, VOXEL_NUM_Z / THREAD_Z);
			DXUT_EndPerfEvent();

			ID3D11UnorderedAccessView* nulluavs[2] = { NULL, NULL };
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, nulluavs, &initCounts);

		}
	}

	LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch(uMsg)
		{
		case WM_KEYDOWN:
			{
				int nKey = static_cast<int>(wParam);

				if (nKey == '1')
				{
					RemoveBall();
				}
				if (nKey == '2')
				{
					AddBall();
				}
				if (nKey == VK_SPACE)
				{
					m_bAnimated = !m_bAnimated;
				}
				break;
			}
		}
		return 0;
	}
};