#include "Header.h"

SamplerState samRaycast : register(s0);

Texture3D g_srvDataVolume : register(t0);


static const float3 aLight_col = float3( 0.01, 0.01, 0.01 );
static const float3 dLight_col = float3( 0.02, 0.02, 0.02 );
static const float3 dLight_dir = normalize( float3( -1, -1, 1 ));
static const float3 pLight_pos = float3( 1, 1, -1 );
static const float3 pLight_col = float3( 1, 1, 1 )*0.1;
//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
cbuffer volumeInfo : register( b0 )
{
	float4 voxelInfo;
	float4 inverseXYZsize; // isolevel in .w conponent
	matrix WorldViewProjection;
	matrix cb_mInvView;
	matrix cb_mView;
	float4 cb_f4ViewPos;
	float2 halfWinSize;
	float2 cb_f2MinMaxDensity;
	float4 boxMin;
	float4 boxMax;
};

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct Ray
{
	float4 o;
	float4 d;
};

struct GS_INPUT
{
};

struct PS_INPUT
{
	float4	projPos : SV_POSITION;
	float4	Pos : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// Utility Funcs
//--------------------------------------------------------------------------------------
bool IntersectBox(Ray r, float3 boxmin, float3 boxmax, out float tnear, out float tfar)
{
	// compute intersection of ray with all six bbox planes
	float3 invR = 1.0 / r.d.xyz;
		float3 tbot = invR * (boxmin.xyz - r.o.xyz);
		float3 ttop = invR * (boxmax.xyz - r.o.xyz);

		// re-order intersections to find smallest and largest on each axis
		float3 tmin = min (ttop, tbot);
		float3 tmax = max (ttop, tbot);

		// find the largest tmin and the smallest tmax
		float2 t0 = max (tmin.xx, tmin.yz);
		tnear = max (t0.x, t0.y);
	t0 = min (tmax.xx, tmax.yz);
	tfar = min (t0.x, t0.y);

	return tnear<=tfar;
}

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
GS_INPUT VS( )
{
	GS_INPUT output = (GS_INPUT)0;

	return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader
//--------------------------------------------------------------------------------------
/*GS for rendering the volume on screen ------------texVolume Read, no half pixel correction*/
[maxvertexcount(4)]
void GS_Quad(point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream)
{
	PS_INPUT output;
	output.projPos=float4(-1.0f,1.0f,0.01f,1.0f);
	output.Pos=mul(float4(-halfWinSize.x, halfWinSize.y,1,1),cb_mInvView);
	triStream.Append(output);

	output.projPos=float4(1.0f,1.0f,0.01f,1.0f);
	output.Pos=mul(float4(halfWinSize.x, halfWinSize.y,1,1),cb_mInvView);
	triStream.Append(output);

	output.projPos=float4(-1.0f,-1.0f,0.01f,1.0f);
	output.Pos=mul(float4(-halfWinSize.x, -halfWinSize.y,1,1),cb_mInvView);
	triStream.Append(output);

	output.projPos=float4(1.0f,-1.0f,0.01f,1.0f);
	output.Pos=mul(float4(halfWinSize.x, -halfWinSize.y,1,1),cb_mInvView);
	triStream.Append(output);
}

float3 local2tex( float3 P)
{
	float3 uv = P * inverseXYZsize.xyz + 0.5;
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
			float depth_dx = g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 1, 0, 0 ) /voxelInfo.xyz, 0 ).x - 
								g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( -1, 0, 0 ) /voxelInfo.xyz, 0 ).x;
			float depth_dy = g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 0, -1, 0 ) /voxelInfo.xyz, 0 ).x - 
								g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 0, 1, 0 ) /voxelInfo.xyz, 0 ).x;
			float depth_dz = g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 0, 0, 1 ) /voxelInfo.xyz, 0 ).x - 
								g_srvDataVolume.SampleLevel ( samRaycast, tCoord + float3 ( 0, 0, -1 ) /voxelInfo.xyz, 0 ).x;

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
			surfacePos = mul(surfacePos,cb_mInvView);
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

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	Ray eyeray;
	//world space
	eyeray.o = cb_f4ViewPos;
	eyeray.d = input.Pos - eyeray.o;
	eyeray.d = normalize(eyeray.d);
	eyeray.d.x = ( eyeray.d.x == 0.f ) ? 1e-15 : eyeray.d.x;
	eyeray.d.y = ( eyeray.d.y == 0.f ) ? 1e-15 : eyeray.d.y;
	eyeray.d.z = ( eyeray.d.z == 0.f ) ? 1e-15 : eyeray.d.z;
	 
	// calculate ray intersection with bounding box
	float tnear, tfar;
	bool hit = IntersectBox(eyeray, boxMin.xyz, boxMax.xyz , tnear, tfar);
	if(!hit) discard;
	if( tnear <= 0 ) tnear = 0;
	float4 col=float4( 1, 1, 1, 0 ) * 0.01;
	float depth=1000.f;

	//isoSurfaceShading(eyeray, float2(tnear,tfar),inverseXYZsize.w, col,depth);
	accumulatedShading(eyeray, float2(tnear,tfar),cb_f2MinMaxDensity, col,depth);

	return col;       
}



