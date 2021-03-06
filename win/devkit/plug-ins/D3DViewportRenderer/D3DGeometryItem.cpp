//-
// ==========================================================================
// Copyright 2015 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk
// license agreement provided at the time of installation or download,
// or which otherwise accompanies this software in either electronic
// or hard copy form.
// ==========================================================================
//+

#include <stdio.h>

#include "D3DViewportRenderer.h"

#include <maya/MGlobal.h>
#include <maya/MString.h>
#include <maya/MMatrix.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MBoundingBox.h>
#include <maya/MImage.h>
#include <maya/MDrawTraversal.h>
#include <maya/MGeometryManager.h>
#include <maya/MGeometry.h>
#include <maya/MGeometryData.h>
#include <maya/MGeometryPrimitive.h>
#include <maya/MNodeMessage.h> // For monitor geometry list
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MFnSet.h>
#include <maya/MFnNumericData.h>
#include <maya/MItDependencyGraph.h>

#include <stdio.h>

#if defined(D3D9_SUPPORTED)

//
// Populate a D3DGeometry object from a Maya mesh
//
bool D3DGeometry::Populate( const MDagPath& dagPath, LPDIRECT3DDEVICE9 D3D)
{
	Release();
	MFnMesh mesh( dagPath.node());

	// Figure out texturing
	//
	MString pn = dagPath.fullPathName();
	//printf("Convert shape %s\n", pn.asChar());
	bool haveTexture = false;
	int	numUVsets = mesh.numUVSets();
	MString uvSetName;
	MObjectArray textures;
	if (numUVsets > 0)
	{
		mesh.getCurrentUVSetName( uvSetName );
		// Always send down uvs for now, since we don't dirty the populate
		// based on material texture connection.
		//
		//MStatus status = mesh.getAssociatedUVSetTextures(uvSetName, textures);
		//if (status == MS::kSuccess && textures.length())
		int numCoords = mesh.numUVs( uvSetName ); 
		if (numCoords > 0)
		{
			haveTexture = true;
		}
	}

	bool haveColors = false;
	int	numColors = mesh.numColorSets();
	MString colorSetName;
	if (numColors > 0)
	{
		haveColors = true;
		mesh.getCurrentColorSetName(colorSetName);
	}

	bool useNormals = true;

	// Setup our requirements needs.
	MGeometryRequirements requirements;
	requirements.addPosition();
	if (useNormals)
		requirements.addNormal();
	if (haveTexture)
		requirements.addTexCoord( uvSetName );
	if (haveColors)
		requirements.addColor( colorSetName );

	// Test for tangents and binormals
	bool testBinormal = false;
	if (testBinormal)
		requirements.addBinormal( uvSetName );
	bool testTangent= false;
	if (testTangent)
		requirements.addTangent( uvSetName );

	MGeometry geom = MGeometryManager::getGeometry( dagPath, requirements, NULL );

	unsigned int numPrims = geom.primitiveArrayCount();
	if( numPrims)
	{
		const MGeometryPrimitive prim = geom.primitiveArray(0);

		NumIndices = prim.elementCount();
		if( NumIndices)
		{
			//MGeometryData::ElementType primType = prim.dataType();
			unsigned int *idx = (unsigned int *) prim.data();

			// Get the position data
			const MGeometryData pos = geom.position();
			float * posPtr = (float * )pos.data();
			if( !idx || !posPtr) return false;
			NumVertices = pos.elementCount();

			// Start building our vertex format. We always have position, so
			// start with that and add in all the elements we find along the way
			FVF = D3DFVF_XYZ;
			Stride = sizeof( float) * 3;

			// Get the normals data
			float * normPtr = NULL;
			if( useNormals)
			{
				const MGeometryData norm = geom.normal();				
				normPtr = (float * )norm.data();
				Stride += sizeof( float) * 3;
				FVF |= D3DFVF_NORMAL;
			}

			// Get the texture coordinate data
			float *uvPtr = NULL;
			if( haveTexture)
			{
				const MGeometryData uvs = geom.texCoord( uvSetName );
				uvPtr = (float *)uvs.data();
				Stride += sizeof( float) * 2;
				FVF |= D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0);
			}

			unsigned int numColorComponents = 4;
			float *clrPtr = NULL;
			if (haveColors)
			{
				const MGeometryData clrs = geom.color( colorSetName );
				clrPtr = (float *)clrs.data();
			}
			else if (testBinormal)
			{
				const MGeometryData binorm = geom.binormal( uvSetName );
				clrPtr = (float *)binorm.data();
				numColorComponents = 3;
			}
			else if (testTangent)
			{
				const MGeometryData tang = geom.tangent( uvSetName );
				clrPtr = (float *)tang.data();
				numColorComponents = 3;
			}

			// Allocate our vertex buffer
			//
			if( D3D->CreateVertexBuffer( Stride * NumVertices, D3DUSAGE_WRITEONLY, FVF, 
											D3DPOOL_DEFAULT, &VertexBuffer, NULL) != D3D_OK)
			{
				MGlobal::displayWarning( "Direct3D renderer : Unable to allocate vertex buffer\n");
				return false;
			}

			// Copy our vertex data into the buffer
			//
			float* VertexData = NULL;
			int FloatsPerVertex = Stride / sizeof( float);
			int StrideOffset = FloatsPerVertex - 3;
			//MGlobal::displayInfo( MString( "Allocating buffers for ") + NumVertices + MString( " verts and ") + NumIndices + MString( " indices\n"));
			VertexBuffer->Lock( 0, 0, (void**)&VertexData, D3DLOCK_DISCARD);
			for( unsigned int i = 0; i < NumVertices; i++)
			{
				*VertexData++ = *posPtr++;
				*VertexData++ = *posPtr++;
				*VertexData++ = *posPtr++;
				VertexData += StrideOffset;
			}
			VertexData -= NumVertices * FloatsPerVertex - 3;

			if( normPtr)
			{
				for( unsigned int i = 0; i < NumVertices; i++)
				{
					*VertexData++ = *normPtr++;
					*VertexData++ = *normPtr++;
					*VertexData++ = *normPtr++;
					VertexData += StrideOffset;
				}
				VertexData -= NumVertices * FloatsPerVertex - 3;
			}

			if( uvPtr)
			{
				StrideOffset = FloatsPerVertex - 2;
				for( unsigned int i = 0; i < NumVertices; i++)
				{
					*VertexData++ = *uvPtr++;
					*VertexData++ = 1.0f - *uvPtr++;
					VertexData += StrideOffset;
				}
				VertexData -= NumVertices * FloatsPerVertex - 2;
			}

			VertexBuffer->Unlock();

			// Allocate our index buffer
			//
			if( D3D->CreateIndexBuffer( NumIndices * sizeof( DWORD), 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &IndexBuffer, NULL) != D3D_OK)
			{
				MGlobal::displayWarning( "Direct3D renderer : Unable to allocate index buffer\n");
				return false;
			}

			// Copy our index data into the buffer
			//
			unsigned int* IndexData = NULL;
			IndexBuffer->Lock( 0, 0, (void**)&IndexData, D3DLOCK_DISCARD);
			memcpy( IndexData, idx, NumIndices * sizeof(DWORD));
			IndexBuffer->Unlock();
		}
	}
	return IndexBuffer && VertexBuffer;
}


#endif



