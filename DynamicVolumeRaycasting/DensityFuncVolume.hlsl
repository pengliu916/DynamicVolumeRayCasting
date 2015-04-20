#include "Header.h"
RWTexture3D<float4> tex_DataVolume : register(u0);
RWTexture3D<int> tex_FlagVolume : register(u1);
//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
cbuffer volumeInfo : register( b0 )
{
	float3 HalfVoxelRes;
	float VoxelSize;
};

cbuffer ballsInfo : register( b1 )
{
	float4 balls[MAX_BALLS];
	float4 ballsCol[MAX_BALLS];
	int numOfBalls;
};

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
float Ball(float3 Pos, float3 Center, float RadiusSq)
{
    float3 d = Pos - Center;
    float DistSq = dot(d, d);
    float InvDistSq = 1 / DistSq;
    return RadiusSq * InvDistSq;
}

//--------------------------------------------------------------------------------------
// Compute Shader
//--------------------------------------------------------------------------------------
[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void VolumeUpdateCS(uint3 DTid: SV_DispatchThreadID)
{
	// Current voxel pos in local space
	float3 currentPos = (DTid - HalfVoxelRes + 0.5f) * VoxelSize;

	float4 field = float4(0,1,1,1);

	for( uint i = 0; i < (uint)numOfBalls; i++ ){
		float density =  Ball( currentPos, balls[i].xyz, balls[i].w  );
		field.x += density;
		field.yzw += ballsCol[i].xyz * pow( density, 3 ) * 1000;
	}
	field.yzw = normalize( field.yzw );

	tex_DataVolume[DTid]=field;
}

[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void VolumeUpdatewithFlagUpdateCS(uint3 DTid: SV_DispatchThreadID)
{
	// Current voxel pos in local space
	float3 currentPos = (DTid - HalfVoxelRes + 0.5f) * VoxelSize;

	float4 field = float4(0,1,1,1);

	for( uint i = 0; i < (uint)numOfBalls; i++ ){
		float density =  Ball( currentPos, balls[i].xyz, balls[i].w  );
		field.x += density;
		field.yzw += ballsCol[i].xyz * pow( density, 3 ) * 1000;
	}
	field.yzw = normalize( field.yzw );

	tex_DataVolume[DTid]=field;

	if(field.x>=MIN_DENSITY && field.x<=MAX_DENSITY) tex_FlagVolume[DTid/VOXEL_BRICK_RATIO]=1;
}


[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void ResetVolumeCS(uint3 DTid: SV_DispatchThreadID)
{
	tex_DataVolume[DTid] = float4(0,0,0,0);
}

[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void FlagVolumeRestCS(uint3 DTid: SV_DispatchThreadID)
{
	tex_FlagVolume[DTid]=0;
}