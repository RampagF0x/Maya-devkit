//-
// ==========================================================================
// Copyright 2011 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk
// license agreement provided at the time of installation or download,
// or which otherwise accompanies this software in either electronic
// or hard copy form.
// ==========================================================================
//+

// Example plugin: vertexBufferGenerator.cpp
//
// This plug-in is an example of a custom MPxVertexBufferGenerator.
// It provides custom vertex streams based on shader requirements coming from 
// an MPxShaderOverride.  The semanticName() in the MVertexBufferDescriptor is used 
// to signify a unique identifier for a custom stream.

// This plugin is meant to be used in conjunction with the d3d11Shader or cgShader plugins.

// The vertexBufferGeneratorGL.cgfx and vertexBufferGeneratorDX11.fx files accompanying this sample
// can be loaded using the appropriate shader plugin.
// The Names of the streams and the stream data generated by this plugin match what is 
// expected from the included effects files.
// This sample use the MyCustomBufferGenerator to create a custom made streams.

// The vertexBufferGenerator2GL.cgfx and vertexBufferGenerator2DX11.fx files accompanying this sample
// can be loaded using the appropriate shader plugin.
// The Names of the streams and the stream data generated by this plugin match what is 
// expected from the included effects files.
// This sample use the MyCustomBufferGenerator2 to create a custom made streams
// by combining the Position and Normal streams in a single one.


#include <maya/MStatus.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnMesh.h>
#include <maya/MDrawRegistry.h>
#include <maya/MPxVertexBufferGenerator.h> 
#include <maya/MHWGeometry.h>

using namespace MHWRender;

class MyCustomBufferGenerator : public MHWRender::MPxVertexBufferGenerator
{
public:
    MyCustomBufferGenerator() {}
    virtual ~MyCustomBufferGenerator() {}

    virtual bool getSourceIndexing(const MObject& object, 
        MComponentDataIndexing& sourceIndexing) const
    {
        // get the mesh from the current path
        // if it is not a mesh we do nothing.
        MStatus status;
        MFnMesh mesh(object, &status);
        if (!status) return false; // failed

        MUintArray& vertToFaceVertIDs = sourceIndexing.indices();

        int faceNum = 0;
        int numPolys = mesh.numPolygons();

        // if it is an empty mesh we do nothing.
        if (numPolys <= 0)
            return false;

        // for each face
        for(int i = 0; i < numPolys; ++i)
        {
            // assign a color ID to all vertices in this face.
            int faceColorID = faceNum%3;

            int vertexCount = mesh.polygonVertexCount(i);
            for (int x = 0; x < vertexCount; ++x)
            {
                // set each face vertex to the face color
                vertToFaceVertIDs.append(faceColorID);
            }
            faceNum++;
        }

        // assign the source indexing
        sourceIndexing.setComponentType(MComponentDataIndexing::kFaceVertex);

        return true;
    }

	virtual bool getSourceStreams(const MObject& object,
		MStringArray &sourceStreams) const
	{
		// No source stream needed
		return false;
	}

    virtual void createVertexStream(const MObject& object,
        MVertexBuffer& vertexBuffer, const MComponentDataIndexing& targetIndexing, const MComponentDataIndexing& /*sharedIndexing*/, const MVertexBufferArray& /*sourceStreams*/) const
    {
        // get the descriptor from the vertex buffer.  
        // It describes the format and layout of the stream.
        const MVertexBufferDescriptor& descriptor = vertexBuffer.descriptor();
        
        // we are expecting a float stream.
        if (descriptor.dataType() != MGeometry::kFloat) 
            return;

        // we are expecting a float2
        if (descriptor.dimension() != 2)
            return;

        // we are expecting a texture channel
        if (descriptor.semantic() != MGeometry::kTexture)
            return;

        // get the mesh from the current path
        // if it is not a mesh we do nothing.
        MStatus status;
        MFnMesh mesh(object, &status);
        if (!status) return; // failed

        const MUintArray& indices = targetIndexing.indices();

        unsigned int vertexCount = indices.length();
        if (vertexCount <= 0)
            return;

        // acquire the buffer to fill with data.
        float* buffer = (float*)vertexBuffer.acquire(vertexCount, true /*writeOnly - we don't need the current buffer values*/);
        float* start = buffer;

        for(unsigned int i = 0; i < vertexCount; ++i)
        {
            // Here we are embedding some custom data into the stream.
            // The included effects (vertexBufferGeneratorGL.cgfx and
            // vertexBufferGeneratorDX11.fx) will alternate 
            // red, green, and blue vertex colored triangles based on this input.
            *(buffer++) = 1.0f;
            *(buffer++) = (float)indices[i]; // color index
        }

        // commit the buffer to signal completion.
        vertexBuffer.commit(start);
    }
};

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

class MyCustomBufferGenerator2 : public MHWRender::MPxVertexBufferGenerator
{
public:
    MyCustomBufferGenerator2() {}
    virtual ~MyCustomBufferGenerator2() {}

	virtual bool getSourceIndexing(const MObject& object, 
        MComponentDataIndexing& sourceIndexing) const
	{
        // get the mesh from the current path
        // if it is not a mesh we do nothing.
        MStatus status;
        MFnMesh mesh(object, &status);
        if (!status) return false; // failed

		MIntArray vertexCount, vertexList;
		mesh.getVertices(vertexCount, vertexList);

		MUintArray& vertices = sourceIndexing.indices();
		for(unsigned int i = 0; i < vertexList.length(); ++i)
			vertices.append( (unsigned int)vertexList[i] );

        return true;
	}

	virtual bool getSourceStreams(const MObject& object,
		MStringArray &sourceStreams) const
	{
		sourceStreams.append( "Position" );
		sourceStreams.append( "Normal" );
		return true;
	}

	virtual void createVertexStream(const MObject& object,
        MVertexBuffer& vertexBuffer, const MComponentDataIndexing& targetIndexing, const MComponentDataIndexing& /*sourceIndexing*/, const MVertexBufferArray& sourceStreams) const
	{
		// get the descriptor from the vertex buffer.  
		// It describes the format and layout of the stream.
		const MVertexBufferDescriptor& descriptor = vertexBuffer.descriptor();
        
		// we are expecting a float or int stream.
		MGeometry::DataType dataType = descriptor.dataType();
		if (dataType != MGeometry::kFloat && dataType != MGeometry::kInt32) 
			return;

		// we are expecting a dimension of 3 or 4
		int dimension = descriptor.dimension();
		if (dimension != 4 && dimension != 3)
			return;

		// we are expecting a texture channel
		if (descriptor.semantic() != MGeometry::kTexture)
			return;

		// get the mesh from the current path
		// if it is not a mesh we do nothing.
		MStatus status;
		MFnMesh mesh(object, &status);
		if (!status) return; // failed

        const MUintArray& indices = targetIndexing.indices();

        unsigned int vertexCount = indices.length();
        if (vertexCount <= 0)
            return;

		MVertexBuffer *positionStream = sourceStreams.getBuffer( "Position" );
		if(positionStream == NULL || positionStream->descriptor().dataType() != MGeometry::kFloat)
			return;
		int positionDimension = positionStream->descriptor().dimension();
		if(positionDimension != 3 && positionDimension != 4)
			return;

		MVertexBuffer *normalStream = sourceStreams.getBuffer( "Normal" );
		if(normalStream == NULL || normalStream->descriptor().dataType() != MGeometry::kFloat)
			return;
		int normalDimension = normalStream->descriptor().dimension();
		if(normalDimension != 3 && normalDimension != 4)
			return;

		float* positionBuffer = (float*)positionStream->map();
		if(positionBuffer)
		{
			float* normalBuffer = (float*)normalStream->map();
			if(normalBuffer)
			{
				void* compositeBuffer = vertexBuffer.acquire(vertexCount, true /*writeOnly - we don't need the current buffer values*/);
				if(compositeBuffer)
				{
					void* compositeBufferStart = compositeBuffer;
					float* compositeBufferAsFloat = (float*)compositeBuffer;
					int* compositeBufferAsInt = (int*)compositeBuffer;

					for(unsigned int i = 0; i < vertexCount; ++i)
					{
						if(dataType == MGeometry::kFloat)
						{
							*(compositeBufferAsFloat++) = *(positionBuffer + 1);	// store position.y 
							*(compositeBufferAsFloat++) = *(positionBuffer + 2);	// store position.z 
							*(compositeBufferAsFloat++) = *(normalBuffer);			// store normal.x
							if(dimension == 4)
								*(compositeBufferAsFloat++) = *(normalBuffer + 2);	// store normal.z
						}
						else if(dataType == MGeometry::kInt32)
						{
							*(compositeBufferAsInt++) = (int)(*(positionBuffer + 1) * 255);		// store position.y 
							*(compositeBufferAsInt++) = (int)(*(positionBuffer + 2) * 255);		// store position.z 
							*(compositeBufferAsInt++) = (int)(*(normalBuffer) * 255);			// store normal.x
							if(dimension == 4)
								*(compositeBufferAsInt++) = (int)(*(normalBuffer + 2) * 255);	// store normal.z
						}
		
						positionBuffer += positionDimension;
						normalBuffer += normalDimension;
					}

					vertexBuffer.commit(compositeBufferStart);
				}

				normalStream->unmap();
			}

			positionStream->unmap();
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

// This is the buffer generator creation function registered with the DrawRegistry.
// Used to initialize the generator.
static MPxVertexBufferGenerator* createMyCustomBufferGenerator()
{
    return new MyCustomBufferGenerator();
}

static MPxVertexBufferGenerator* createMyCustomBufferGenerator2()
{
	return new MyCustomBufferGenerator2();
}

// The following routines are used to register/unregister
// the vertex generators with Maya
//
MStatus initializePlugin( MObject obj )
{
    // register a generator based on a custom semantic for DX11.  You can use any name in DX11.
    MDrawRegistry::registerVertexBufferGenerator("myCustomStream", createMyCustomBufferGenerator);

    // register a generator based on a custom semantic for cg.  
    // Pretty limited in cg so we hook onto the "ATTR" semantics.
    MDrawRegistry::registerVertexBufferGenerator("ATTR8", createMyCustomBufferGenerator);


	// register a generator based on a custom semantic for DX11.  You can use any name in DX11.
	MDrawRegistry::registerVertexBufferGenerator("myCustomStreamB", createMyCustomBufferGenerator2);

    // register a generator based on a custom semantic for cg.  
    // Pretty limited in cg so we hook onto the "ATTR" semantics.
    MDrawRegistry::registerVertexBufferGenerator("ATTR7", createMyCustomBufferGenerator2);

	return MS::kSuccess;
}

MStatus uninitializePlugin( MObject obj)
{
    MDrawRegistry::deregisterVertexBufferGenerator("myCustomStream");

	MDrawRegistry::deregisterVertexBufferGenerator("ATTR8");

	MDrawRegistry::deregisterVertexBufferGenerator("myCustomStreamB");

	MDrawRegistry::deregisterVertexBufferGenerator("ATTR7");

	return MS::kSuccess;
}
