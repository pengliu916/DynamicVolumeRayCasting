#include "Header.h"
//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
// 3D texture contain the data volume
Texture3D<float4> g_srvDataVolume : register(t0);
// 3D texture contain the flag volume
Texture3D<int> g_srvFlagVolume : register(t1);

// 2D texture contain start and end depth info. for each ray
Texture2D<float4> g_srvFarNear : register(t2);

SamplerState samRaycast : register(s0);

// Phong shading variable
static const float3 aLight_col = float3( 0.01, 0.01, 0.01 );
static const float3 dLight_col = float3( 0.02, 0.02, 0.02 );
static const float3 dLight_dir = normalize( float3( -1, -1, 1 ));
static const float3 pLight_pos = float3( 1, 1, -1 );
static const float3 pLight_col = float3( 1, 1, 1 )*0.1;

static const float3 g_cf3InvVolSize = float3(1.f / (VOXEL_NUM_X*VOXEL_SIZE),1.f / (VOXEL_NUM_Y*VOXEL_SIZE),1.f / (VOXEL_NUM_Z*VOXEL_SIZE));
static const float3 g_cf3VoxelReso = float3(VOXEL_NUM_X,VOXEL_NUM_Y,VOXEL_NUM_Z);
static const float3 g_cf3InvVoxelReso = float3(1.f/VOXEL_NUM_X,1.f/VOXEL_NUM_Y,1.f/VOXEL_NUM_Z);

static const uint g_cuFlagVolSliceStep = VOXEL_NUM_X * VOXEL_NUM_Y / VOXEL_BRICK_RATIO / VOXEL_BRICK_RATIO;
static const uint g_cuFlagVolStripStep = VOXEL_NUM_X / VOXEL_BRICK_RATIO;
static const float3 g_cf3HalfBrickSize = float3(VOXEL_SIZE*VOXEL_BRICK_RATIO,VOXEL_SIZE*VOXEL_BRICK_RATIO,VOXEL_SIZE*VOXEL_BRICK_RATIO)*0.5f;

//--------------------------------------------------------------------------------------
// Const buffers
//--------------------------------------------------------------------------------------
cbuffer cbPerFrame : register(b0)
{
	matrix cb_mInvView;
	matrix cb_mView;
	matrix cb_mViewProj;
	float4 cb_f4ViewPos;
	float2 cb_f2HalfWinSize;
	float2 cb_f2WinReso;
	float2 cb_f2MinMaxDensity; // visible range of density, if in isosurface rendering, x is the isovalue
	float2 NIU;
};
//--------------------------------------------------------------------------------------
// Utility structure
//--------------------------------------------------------------------------------------
struct Ray
{
	float4 o;
	float4 d;
};

//--------------------------------------------------------------------------------------
// Shader I/O structure
//--------------------------------------------------------------------------------------
struct GS_INPUT{};

struct PS_INPUT
{
	float4	projPos : SV_POSITION;
	float4	Pos : TEXCOORD0;
};

struct MarchPS_INPUT
{
	float4 projPos: SV_Position;
	float4 Pos: NORMAL0;
	float2 Tex: TEXCOORD0;
};

struct PS_fDepth_OUT
{
	float4 RGBD : SV_Target;
	float Depth : SV_Depth;
};

struct MarchPS_OUT
{
	float4 RGBD : SV_TARGET0;
	float DepthOut : SV_Depth;
};

//--------------------------------------------------------------------------------------
//  Vertex Shaders
//--------------------------------------------------------------------------------------
/* Pass through VS */
GS_INPUT VS()
{
	GS_INPUT output = (GS_INPUT)0;
	return output;
}

//--------------------------------------------------------------------------------------
//  Geometry Shaders
//--------------------------------------------------------------------------------------
/*GS for rendering the NearFar tex,expand cube geometries for all active cell*/
[maxvertexcount(18)]
void ActiveCellAndSOGS(point GS_INPUT particles[1],uint primID : SV_PrimitiveID,inout TriangleStream<PS_INPUT> triStream)
{
	// calculate the idx for access the cell volume
	uint3 u3Idx;
	u3Idx.z = primID / g_cuFlagVolSliceStep;
	u3Idx.y = (primID % g_cuFlagVolSliceStep) / g_cuFlagVolStripStep;
	u3Idx.x = primID % g_cuFlagVolStripStep;

	// check whether the current cell is active,discard the cell if inactive
	int active = g_srvFlagVolume[u3Idx];

	if(active!=1)return;
	u3Idx.y = VOXEL_NUM_Y/VOXEL_BRICK_RATIO-1- u3Idx.y;

	// calculate the offset vector to move (0,0,0,1) to current cell center
	float4 f4CellCenterOffset = float4((u3Idx - g_cf3VoxelReso * 0.5f / VOXEL_BRICK_RATIO+0.5f) * VOXEL_SIZE * VOXEL_BRICK_RATIO,0.f);

	PS_INPUT output;
	// get half cell size to expand the cell center point to full cell cube geometry
	float4 f4HalfCellSize = float4(g_cf3HalfBrickSize*1.2f,1.f);
	// first expand centered cube with edge length 2 to actual cell size in world space
	// then move to it's right pos in world space,and store the world space position in it's Pos component.
	// after that,transform into view space and then project into image plane,and store image space 4D coord in it's projPos component
	output.Pos=float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	triStream.RestartStrip();

	output.Pos=float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	triStream.RestartStrip();

	output.Pos=float4(1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mViewProj);triStream.Append(output);

}

/*GS for rendering the active cells as cube wireframe*/
[maxvertexcount(24)]
void Debug_CellGS(point GS_INPUT particles[1],uint primID : SV_PrimitiveID,inout LineStream<PS_INPUT> lineStream)
{
	// calculate the idx for access the cell volume
	uint3 u3Idx;
	u3Idx.z = primID / g_cuFlagVolSliceStep;
	u3Idx.y = (primID % g_cuFlagVolSliceStep) / g_cuFlagVolStripStep;
	u3Idx.x = primID % g_cuFlagVolStripStep;

	// check whether the current cell is active,discard the cell if inactive
	int active = g_srvFlagVolume[u3Idx];

	if(active!=1)return;
	u3Idx.y = VOXEL_NUM_Y/VOXEL_BRICK_RATIO-1 - u3Idx.y;
	// calculate the offset vector to move (0,0,0,1) to current cell center
	float4 f4CellCenterOffset = float4((u3Idx - g_cf3VoxelReso * 0.5f / VOXEL_BRICK_RATIO+0.5f) * VOXEL_SIZE * VOXEL_BRICK_RATIO,0.f);

	PS_INPUT output;
	// get half cell size to expand the cell center point to full cell cube geometry
	float4 f4HalfCellSize = float4(g_cf3HalfBrickSize,1.f);
	// first expand centered cube with edge length 2 to actual cell size in world space
	// then move to it's right pos in world space,and store the world space position in it's Pos component.
	// after that,transform into view space and then project into image plane,and store image space 4D coord in it's projPos component
	output.Pos = float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	lineStream.RestartStrip();

	output.Pos = float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(-1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mViewProj);lineStream.Append(output);
}

 /*GS for ray marching,this will generate a fullscreen quad represent the projection plan of Kinect sensor in world space*/
[maxvertexcount(4)]
void RaymarchGS(point GS_INPUT particles[1],inout TriangleStream<MarchPS_INPUT> triStream)
{
	MarchPS_INPUT output;

	output.projPos=float4(-1.0f,1.0f,0.01f,1.0f);
	output.Pos=mul(float4(-cb_f2HalfWinSize.x, cb_f2HalfWinSize.y,1,1),cb_mInvView);
	output.Tex = float2(0.f,0.f);
	triStream.Append(output);

	output.projPos=float4(1.0f,1.0f,0.01f,1.0f);
	output.Pos=mul(float4(cb_f2HalfWinSize.x, cb_f2HalfWinSize.y,1,1),cb_mInvView);
	output.Tex = float2(cb_f2WinReso.x,0.f);
	triStream.Append(output);

	output.projPos=float4(-1.0f,-1.0f,0.01f,1.0f);
	output.Pos=mul(float4(-cb_f2HalfWinSize.x, -cb_f2HalfWinSize.y,1,1),cb_mInvView);
	output.Tex = float2(0.f,cb_f2WinReso.y);
	triStream.Append(output);

	output.projPos=float4(1.0f,-1.0f,0.01f,1.0f);
	output.Pos=mul(float4(cb_f2HalfWinSize.x, -cb_f2HalfWinSize.y,1,1),cb_mInvView);
	output.Tex = float2(cb_f2WinReso.x,cb_f2WinReso.y);
	triStream.Append(output);
}

//--------------------------------------------------------------------------------------
//  Pixel Shaders
//--------------------------------------------------------------------------------------
/*PS for rendering the NearFar Texture for raymarching*/
//[Warning] keep this PS short since during alpha blending, each pixel will be shaded multiple times
float4 FarNearPS(PS_INPUT input) : SV_Target
{
	// each pixel only output it's pos(view space!!)'s z component 
	// as output for red channel: for smallest depth
	// as output for alpha channel: for largest depth
	// this is achieved by using alpha blending which treat color and alpha value differently
	// during alpha blending, output merge only keep the smallest value for color component
	// while OM only keep the largest value for alpha component
	return float4(input.projPos.w,1.f,input.projPos.w,input.projPos.w);
}
/*PS for debug active cell, this will render each active cell as hollow cube wireframe */
PS_fDepth_OUT Debug_CellPS(PS_INPUT input)
{
	PS_fDepth_OUT output;
	output.RGBD = float4(0.133f,0.691f,0.297f,1.f)*0.8f;
	output.Depth = input.projPos.w/10.f; // specifically output to depth buffer for correctly depth culling
	return output;
}


float3 local2tex( float3 P)
{
	float3 uv = P * g_cf3InvVolSize.xyz + 0.5;
	uv.y = 1 - uv.y;
	return uv;
}

void isoSurfaceShading( Ray eyeray, float2 f2NearFar, float isoValue,
					   inout float4 outColor, inout float outDepth)
{
	float3 Pnear = eyeray.o.xyz + eyeray.d.xyz * f2NearFar.x;
	float3 Pfar = eyeray.o.xyz + eyeray.d.xyz * f2NearFar.y;

	float3 P = Pnear;
	float t = f2NearFar.x;
	float tSmallStep = 0.99 * VOXEL_SIZE;
	float3 P_pre = Pnear;
	float3 PsmallStep = eyeray.d.xyz * tSmallStep;

	float4 surfacePos;

	float field_pre ;
	float field_now = g_srvDataVolume.SampleLevel(samRaycast,local2tex(P),0).x;
	
	//outColor = float4(local2tex(P),0);return;


	while ( t <= f2NearFar.y ) {
		float3 texCoord = local2tex(P);
		float4 Field = g_srvDataVolume.SampleLevel(samRaycast, texCoord, 0);
		//float4 Field = g_srvDataVolume.Load( int4(texCoord*voxelInfo.xyz,0));

		float density = Field.x;
		float4 color = float4( Field.yzw, 0 );

		field_pre = field_now;
		field_now = density;

		if ( field_now >= isoValue && field_pre < isoValue )
		{
			// For computing the depth
			surfacePos = float4(P_pre + ( P - P_pre) * (isoValue-field_pre) / (field_now - field_pre),1.f);

			// For computing the normal

			float3 tCoord = local2tex(surfacePos.xyz);
			float depth_dx = g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 1, 0, 0 ) *g_cf3InvVoxelReso, 0 ).x - 
								g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( -1, 0, 0 ) *g_cf3InvVoxelReso, 0 ).x;
			float depth_dy = g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 0, -1, 0 ) *g_cf3InvVoxelReso, 0 ).x - 
								g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 0, 1, 0 ) *g_cf3InvVoxelReso, 0 ).x;
			float depth_dz = g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 0, 0, 1 ) *g_cf3InvVoxelReso, 0 ).x - 
								g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 0, 0, -1 ) *g_cf3InvVoxelReso, 0 ).x;

			float3 normal = -normalize ( float3 ( depth_dx, depth_dy, depth_dz ) );


			// shading part
			float3 ambientLight = aLight_col * color;

			float3 directionalLight = dLight_col * color * clamp( dot( normal, dLight_dir ), 0, 1 );

			float3 vLight = cb_f4ViewPos.xyz - surfacePos.xyz;
			float3 halfVect = normalize( vLight - eyeray.d.xyz );
			float dist = length( vLight ); vLight /= dist;
			float angleAttn = clamp ( dot ( normal, vLight ), 0, 1 );
			float distAttn = 1.0f / ( dist * dist ); 
			float specularAttn = pow( clamp( dot( normal, halfVect ), 0, 1 ), 128 );

			float3 pointLight = pLight_col * color * angleAttn + color * specularAttn ;

			outColor = float4(ambientLight + directionalLight + pointLight,1);
			surfacePos = mul(surfacePos,cb_mView);
			outDepth = surfacePos.z/10.f;
			return;
			//return float4(normal*0.5+0.5,0);
		}

		P_pre = P;
		P += PsmallStep;
		t += tSmallStep;
	}
	return;       
}


float transferFunction(float density)
{
	float opacity = (density-cb_f2MinMaxDensity.x)/(cb_f2MinMaxDensity.y-cb_f2MinMaxDensity.x);
	float p2 = opacity*opacity+0.02;
	float p4 = p2*p2;
	return p4*0.3+ p2*0.1 +opacity*0.15;
}
void accumulatedShading( Ray eyeray, float2 f2NearFar, float2 f2MinMaxDen,
					   inout float4 outColor, inout float outDepth)
{
	bool bFirstEnter = true;
	float3 Pnear = eyeray.o.xyz + eyeray.d.xyz * f2NearFar.x;
	float3 Pfar = eyeray.o.xyz + eyeray.d.xyz * f2NearFar.y;

	float3 P = Pnear;
	float t = f2NearFar.x;
	float tSmallStep = 0.8 * VOXEL_SIZE;
	float3 PsmallStep = eyeray.d.xyz * tSmallStep;

	float field_now = g_srvDataVolume.SampleLevel(samRaycast,local2tex(P),0).x;
	
	//outColor = float4(local2tex(P),0);return;
	float4 src = 0;
	float4 dst = 0;
	while ( t <= f2NearFar.y ) {
		float3 texCoord = local2tex(P);
		float4 Field = g_srvDataVolume.SampleLevel(samRaycast, texCoord, 0);

		if ( Field.x >= f2MinMaxDen.x && Field.x <= f2MinMaxDen.y )
		{
			if (bFirstEnter){
				float4 fPos = mul(float4(P, 1), cb_mView);
				outDepth = fPos.z / 10.f;
				bFirstEnter = false;
			}

			src = float4(Field.yzw, transferFunction(Field.x));
			src.a*=0.25f;

			src.rgb*=src.a;
			dst = (1.0f - dst.a)*src + dst;
			
			if(dst.a>=0.95f) break;
		}

		P += PsmallStep;
		t += tSmallStep;
	}
	outColor = dst*dst.a;
	return;       
}

/* PS for extracting shaded image from volume with depth buffer updated */
MarchPS_OUT RaymarchPS(MarchPS_INPUT input)
{
	MarchPS_OUT output;
	// initial the default value for no surface intersection
	output.RGBD = float4 ( 0,0,0,-1 );
	output.DepthOut = 1000.f;

	Ray eyeray;
	//world space
	eyeray.o = cb_f4ViewPos;
	eyeray.d = input.Pos - eyeray.o;
	float len=length(eyeray.d.xyz);
	eyeray.d = float4(normalize(eyeray.d.xyz),0);//world space

	eyeray.d.x = ( eyeray.d.x == 0.f ) ? 1e-15 : eyeray.d.x;
	eyeray.d.y = ( eyeray.d.y == 0.f ) ? 1e-15 : eyeray.d.y;
	eyeray.d.z = ( eyeray.d.z == 0.f ) ? 1e-15 : eyeray.d.z;

	// read the raymarching start t(tNear) and end t(tFar) from the precomputed FarNear tex
	int3 i3Idx = int3(input.Tex,0);

	// now f2NearFar hold depth near and depth far in view space from g_srvFarNear in red and alpha channel
	float2 f2NearFar = g_srvFarNear.Load(i3Idx).ra;

	//now f2NearFar hold t near and t far in view space
	f2NearFar = f2NearFar*len;

	//if(hit) output.RGBD=float4(1,1,1,1)*float4(abs(f2NearFar-float2(tnear,tfar)),0,1);

	//isoSurfaceShading(eyeray, f2NearFar,cb_f2MinMaxDensity.x,output.RGBD, output.DepthOut);
	accumulatedShading(eyeray, f2NearFar,cb_f2MinMaxDensity, output.RGBD,output.DepthOut);

	//if(abs(g_srvFarNear.Load(i3Idx).g-1.f)<0.001) output.RGBD = float4(1,1,1,1)*(f2NearFar.x)*0.25f;
	return output;
}
